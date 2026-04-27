# Plan de correction, amélioration & finitions — `synth-bass`

> Document opérationnel, prêt à l'emploi.
> Source : [BASS_AUDIT_COMPLET.md](BASS_AUDIT_COMPLET.md) + lecture directe du code (`Source/Engine/*`, `Source/PluginProcessor.*`, `new composants/*`).
> Historique déjà fait : voir [BASS_PATCH_CHANGELOG.md](BASS_PATCH_CHANGELOG.md) (Phase 1, 2, 3.1 closes).
>
> **Contrainte produit gelée (à respecter sans exception)** :
> - Les **paramètres utilisateur** (`BassSettings`, `GlobalFxSettings`, IDs APVTS) et les **knobs** ne doivent **ni être ajoutés, ni retirés, ni renommés, ni voir leur range modifiée**.
> - Toutes les corrections agissent uniquement sur :
>   - `BassCharacteristics` (non éditable par l'utilisateur)
>   - les **defaults** des presets factory
>   - le DSP interne (`BassVoice`, `FxProcessors`)
>   - la taxonomie / metadata des presets

---

## 0. Synthèse exécutive

| Axe | État aujourd'hui | Cible release |
|-----|------------------|---------------|
| Moteur DSP | Stabilisé (Phase 1+2) — résiduel critique sur `decay2` 808 et double saturation | Cohérent, pas de queue parasite > 4 s en preset utilitaire |
| Banque factory | Calibration loudness OK (Phase 3.1) | Doublons éliminés, taxonomy propre, releases bornées |
| Cohérence familiale | Acoustic OK / 808 incohérente / Synth dense | 3 familles homogènes en architecture interne |
| Différenciation | Contrebasse ≈ Fingered, Distorted ≈ synth | Chaque basse identifiable en aveugle |
| FX | Defaults discutables (Comp off, double sat) | Defaults musicalement cohérents, pas de double drive caché |
| QA | Tests + renderer + benchmark CPU en place | Tous verts + écoute manuelle validée |

**Estimation effort** : ~5 phases livrables séquentiellement, chaque phase indépendamment testable et reverteable.

---

## 1. Méthodologie

1. **Une phase = un commit / un build vert**.
   - Avant chaque phase : régénérer baseline `qa/bass_preset_qa_report.csv` + `qa/bass_cpu_benchmark.csv`.
   - Après chaque phase : relancer `UWdeVST_bass_tests.exe` + renderer `--validate-presets` + écoute manuelle des presets impactés.
2. **Pas de refonte UI dans ce plan** — ce plan finalise le produit existant. Toute évolution UI (Phase 4 du changelog) sort du périmètre.
3. **Aucune dépendance entre phases** sauf la P1 → P3 (les valeurs de presets de P3 dépendent des nouveaux defaults DSP de P1).

---

## 2. Inventaire des problèmes (mapping audit → action)

| # | Audit | Localisation code | Phase | Action |
|---|-------|-------------------|-------|--------|
| P1 | Contrebasse ≈ Fingered | [BassDefs.cpp#L40-L52](../Source/Engine/BassDefs.cpp) | P2 | Repolariser les 2 timbres |
| P2 | Pas de vision unificatrice | global | hors plan | — |
| P3 | Boom 808 seul avec body | [BassDefs.cpp#L73-L80](../Source/Engine/BassDefs.cpp) | P1 | Aligner les 3 808 |
| P4 | `decay2Ratio` 808 = 5–6 (jusqu'à 48 s) | [BassDefs.cpp#L62-L83](../Source/Engine/BassDefs.cpp) + [BassVoice.cpp#L19-L26](../Source/Engine/BassVoice.cpp) | P1 | Plafonner ratio + clamp DSP |
| P5 | Reese — accumulation 3 oscs en poly | [BassVoice.cpp](../Source/Engine/BassVoice.cpp) (osc loop) | P2 | Damping interne + roll-off HF |
| P6 | Distorted 808 — sat 4.0 noie la fonda | [BassDefs.cpp#L89](../Source/Engine/BassDefs.cpp) | P1 | Réduire à ≤ 2.5 + post-HPF |
| P7 | Contrebasse `bodyMaxFeedback` 0.75 | [BassDefs.cpp#L43](../Source/Engine/BassDefs.cpp) | P2 | Borner à 0.55 + scale par note |
| P8 | Compressor OFF par défaut | [BassDefs.h#L101](../Source/Engine/BassDefs.h) | P3 | Activer pour 808 et Reese (per-preset only) |
| P9 | Double saturation Distorted 808 | `builtInSaturation` + `satDrive` | P3 | Forcer `satMix=0` dans presets Distorted |
| P10 | Slap pluck = 1.0 concurrence harmoniques | [BassDefs.cpp#L51](../Source/Engine/BassDefs.cpp) | P2 | Garde HF + descendre à 0.85 |
| P11 | Reese release désynchro | [BassVoice.cpp](../Source/Engine/BassVoice.cpp) release path | P2 | Common release env partagé |
| P12 | Distorted ≈ synth | repositionnement | P2 | Ré-axer sur sine + drive contrôlé |
| P13 | Boom 808 body delay queue | [BassDefs.cpp#L78](../Source/Engine/BassDefs.cpp) | P1 | Réduire feedback + shorter damping |
| P14 | Contrebasse / Fingered manquent transient | [BassDefs.cpp#L44,L50](../Source/Engine/BassDefs.cpp) | P2 | Ajouter pluck léger (0.25 / 0.20) |
| P15 | Déséquilibre loudness inter-familles | nominalPeakDb par famille | P3 | Aligner les 3 familles à -10.5 dB |

---

## 3. PHASE 1 — Corrections moteur critiques (DSP)

**But** : éliminer les artefacts qui rendent certaines basses inutilisables (P4, P6, P3, P13).

**Risque** : faible, modifications confinées à `BassDefs.cpp` (table `kChars`) + 1 clamp dans `BassVoice.cpp`. Aucun paramètre user touché.

### 3.1 Plafonner `decay2Ratio` des 808 (P4)

Fichier : [Source/Engine/BassDefs.cpp](../Source/Engine/BassDefs.cpp) — table `kChars`.

| Bass | `decay2Ratio` actuel | Cible | Justification |
|------|----------------------|-------|---------------|
| Sub 808 | 5.0 | **2.2** | preset `decaySeconds` peut atteindre 8 s → max 17.6 s utile, suffisant pour trap long |
| Boom 808 | 6.0 | **2.5** | idem, conserve le « tail massif » sans rendre la basse ingérable en groove rapide |
| Distorted 808 | 3.0 | **1.8** | la saturation interne empile déjà du sustain perçu |

Et dans [Source/Engine/BassVoice.cpp](../Source/Engine/BassVoice.cpp), fonction `updateEnvelopeCoefficients`, ajouter un **clamp DSP en dur** indépendant du characteristic (ceinture + bretelles) :

```cpp
// Hard cap on usable decay tail (4× decay max), prevents pathological presets
const float d2Time = std::min(8.0f, std::max(0.05f, decaySeconds * chars.decay2Ratio));
```

**Test** : preset `Sub Long` (decaySeconds=8.0) — la queue audible doit chuter à -60 dB en ≤ 8 s contre ~40 s aujourd'hui.

### 3.2 Cohérence body resonator famille 808 (P3, P13)

Trois choix possibles, **option B retenue** (moins disruptive, cohérente avec la promesse marketing) :

- ❌ A : supprimer le body de Boom 808 → perte du caractère « massif »
- ✅ **B : ajouter un body très léger à Sub 808 (0.50 ratio, fb 0.05) et garder Boom (0.35 / fb 0.10 réduit), Distorted reste sec**
- ❌ C : ajouter à tous → confond les rôles

Patch dans `kChars` :

```cpp
// Sub 808 — body subtile sur fondamentale (apporte chaleur sans queue)
{ OscMode::Sine, 0, 0.0f, 0.0f, 1,
  7.0f, 0.06f,
  0.25f, 2.2f, 0.35f,
  0.50f, 0.05f, 0.40f,   // bodyDelayRatio=0.50 (octave), feedback=0.05, damping=0.40
  0.0f, 0.0f,
  0.0f, true },

// Boom 808 — body atténué (P13)
{ OscMode::Additive, 5, 0.0001f, 0.0f, 1,
  8.0f, 0.08f,
  0.20f, 2.5f, 0.40f,
  0.35f, 0.10f, 0.20f,   // feedback 0.12 → 0.10, damping 0.12 → 0.20
  1.5f, true },
```

Distorted 808 : reste à `0.0 / 0.0 / 0.0` (volontairement sec).

### 3.3 Réduire la saturation interne du Distorted 808 (P6)

```cpp
// Distorted 808
{ OscMode::Sine, 0, 0.0f, 0.0f, 1,
  10.0f, 0.04f,
  0.35f, 1.8f, 0.25f,
  0.0f, 0.0f, 0.0f,
  0.0f, 0.0f,
  2.5f, true },          // builtInSaturation 4.0 → 2.5
```

Et dans `BassVoice.cpp` — pipeline saturation, **ajouter un HPF post-saturation à ~35 Hz** uniquement quand `chars.isSubBass && chars.builtInSaturation > 1.5f`, pour empêcher la DC asymétrique de remonter en sub.

### 3.4 Validation Phase 1

```powershell
cmake --build build --config Release --target UWdeVST_bass_tests
.\build\UWdeVST_bass_tests_artefacts\Release\UWdeVST_bass_tests.exe
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --validate-presets --report qa/bass_preset_qa_report.csv
```

**Critères GO** : aucun nouveau test rouge ; pas de preset silencieux ou clip ; queue audible Sub 808 / Boom 808 ≤ 8 s.

---

## 4. PHASE 2 — Différenciation & lisibilité

**But** : régler P1, P5, P7, P10, P11, P12, P14 — la palette devient réellement à 9 timbres distincts.

### 4.1 Repolariser Contrebasse vs Fingered (P1, P14)

Stratégie : pousser la Contrebasse vers un caractère **upright/jazz** (plus sombre, plus de body, micro-pluck doux), pousser Fingered vers **précision électrique** (plus brillante, moins de body, pluck net).

Modifs `kChars` :

```cpp
// Contrebasse — upright, plus sombre, pluck nature woody
{ OscMode::Additive, 8, 0.0006f, 0.000f, 1,   // inharmonicity 0.0003 → 0.0006 (vraie corde épaisse)
  0.0f, 0.0f,
  0.40f, 2.0f, 0.28f,
  1.00f, 0.55f, 0.45f,                        // feedback 0.75 → 0.55 (P7), damping 0.35 → 0.45
  0.25f, 0.012f,                              // pluck 0.70 → 0.25 (transient bois, doux), pluckSeconds plus long
  0.0f, false },

// Basse Fingered — électrique, plus brillante, attaque doigt nette
{ OscMode::Additive, 7, 0.00002f, 0.000f, 1,  // partials 6 → 7, inharmo divisée par 2
  0.0f, 0.0f,
  0.45f, 1.8f, 0.25f,
  1.00f, 0.50f, 0.20f,
  0.20f, 0.004f,                              // pluck 0.40 → 0.20 (P14 : transient doigt court)
  0.0f, false },
```

Defaults presets correspondants (table `kDefaults`) :
- Contrebasse : `brightness 0.45 → 0.38`, `cutoffHz 3000 → 2400`
- Fingered : `brightness 0.50 → 0.58`, `cutoffHz 3500 → 4200`

**Test d'aveugle** : un musicien doit identifier l'une vs l'autre sans regarder l'écran (test à inscrire au release checklist).

### 4.2 Slap — atténuer le pluck (P10)

```cpp
// Basse Slap
0.85f, 0.003f,   // pluckAmount 1.0 → 0.85, pluckSeconds inchangé
```

Plus, dans `BassVoice.cpp` au moment de générer le bruit de pluck : appliquer un **shelf HF léger -3 dB > 8 kHz** pour éviter la collision avec hi-hats.

### 4.3 Reese — densité spectrale & releases (P5, P11)

Dans `BassVoice.cpp`, sur la branche multi-osc (`chars.numOscillators >= 3`) :

1. **Release env partagée** : actuellement chaque sub-osc a son propre suivi ; partager `releaseGain` global pour les 3 oscs (fix P11).
2. **HF roll-off interne** : 1-pole LP à `min(4000, settings.cutoffHz * 1.4)` AVANT le filtre principal, uniquement sur le mix unison ; coupe la densité haute fréquence sans changer le grave.
3. **Centre du detune compensé** : déjà fait en cents (Phase 1 du changelog), confirmer que le centre `osc[1] = 1.0` est utilisé comme référence d'amplitude (gain unitaire), oscs latéraux à -1.5 dB chacun.

### 4.4 Repositionner Distorted 808 (P12)

Cible : retrouver une identité « 808 », pas « synth saturé ». En complément de la baisse de saturation (P1) :

- `pitchEnvSemitones` : 10.0 → **9.0** (moins de zap, plus de drop musical)
- `pitchEnvSeconds` : 0.04 → **0.05** (50 ms, plus naturel)
- Default preset `cutoffHz` : 1500 → 1100 (renforce l'identité grave)
- Tag mixRole par défaut : passer de `character-bass` → `sub-foundation` pour ses 12 presets utilitaires (P3.2), garder `character-bass` pour les 13 presets agressifs.

### 4.5 Validation Phase 2

Écoute manuelle obligatoire (release checklist) :
- A/B Contrebasse vs Fingered sur même note (E1, E2, E3) — doit être discriminable.
- Reese : jouer accord 3-notes sur C1 — pas de cloud illisible.
- Slap : co-écouter avec hi-hats — pas de masquage.

---

## 5. PHASE 3 — Defaults FX & calibration finale

**But** : régler P8, P9, P15. Aucun changement DSP.

### 5.1 Defaults par famille (P8, P9)

Au lieu d'un `GlobalFxSettings` unique, étendre `getDefaultGlobalFx(int bassIndex)` (à ajouter dans `BassDefs.h/cpp`) qui renvoie un default **par famille** :

| FX | Acoustic | 808 | Synth |
|----|----------|-----|-------|
| Saturator on | true (drive 1.4, mix 0.10) | **false** pour Distorted (P9) / true ailleurs | true (drive 1.6, mix 0.12) |
| Transient on | true | true | true |
| **Compressor on** | false | **true** (-16 dB, ratio 2.5, attack 8 ms, rel 90 ms, mix 0.6) | **true** Reese seulement (-14 dB, ratio 3) |
| EQ on | true | true | true |
| Chorus on | false | false | false |
| Delay on / mix | true / 0 | true / 0 | true / 0 |
| Reverb on / mix | true / 0 | true / 0 | true / 0 |
| Limiter on | true | true | true |

Note : le **knob** `compressorOn` reste exposé et fonctionnel — on change uniquement la **valeur par défaut** que les presets factory utilisent.

### 5.2 Aligner loudness nominale (P15)

Dans `FactoryPresets.cpp`, normaliser `nominalPeakDb` :

| Famille | Avant | Après |
|---------|-------|-------|
| Acoustic | -12.0 | **-10.5** |
| 808 (Sub/Boom) | -9.5 | **-10.5** |
| 808 (Distorted) | -10.5 | **-10.5** |
| Synth | -10.5 | **-10.5** |

Recompenser les `level` de presets impactés (Acoustic +1.5 dB en gain, 808 -1 dB) pour conserver l'équilibre interne.

### 5.3 Régénérer le report

```powershell
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --validate-presets --report qa/bass_preset_qa_report.csv
```

Vérifier que **toutes** les lignes du CSV ont leur `peak_dbfs` dans `[-12.0, -9.0]`.

---

## 6. PHASE 4 — Nettoyage banque (Phase 3.2 du changelog)

**But** : finir le travail amorcé dans le changelog, éliminer doublons et taxonomie incohérente.

### 6.1 Audit doublons (script renderer)

Critère : 2 presets de la même basse sont doublons si :
- distance euclidienne sur `(brightness, cutoffHz/8000, sustainLevel, decaySeconds/10, drive)` < 0.10
- ET même `mix_role`

Action : pour chaque doublon trouvé → renommer + écarter avec `tuneSemitones`/`character` ou supprimer le moins utilisé.

Cible : conserver **25 presets utiles** par basse, pas 25 « presque pareils ».

### 6.2 Taxonomie — règles à valider sur tous les presets

| Rôle | Tags obligatoires | Critère d'éligibilité |
|------|------------------|----------------------|
| `organic-foundation` | `bass`, `factory`, `acoustic`, `<modèle>` | Acoustic uniquement, sustain ≥ 0.20, attack ≤ 0.01 s |
| `sub-foundation` | `bass`, `factory`, `808`, `<modèle>` | 808 uniquement, brightness ≤ 0.40 |
| `transient-bass` | `bass`, `factory`, `<famille>`, `<modèle>` | nominalPeakDb permissif (-8 dB), pluckAmount ou drive court |
| `character-bass` | `bass`, `factory`, `synth`, `<modèle>` | Synth, brightness ≥ 0.40 |
| `lead-bass` | `bass`, `factory`, `synth`, `<modèle>` | Synth, cutoffHz ≥ 2500, resonance ≥ 0.40 |
| `texture-bass` | `bass`, `factory`, `<famille>`, `<modèle>` | nominalPeakDb -13.5 dB, attack ≥ 0.05 s |

À implémenter dans le renderer `--validate-presets` qui doit refuser le build si une règle n'est pas respectée.

### 6.3 Couverture minimale par famille (déjà spec dans release checklist)

| Famille | Rôles obligatoires | Action si manquant |
|---------|-------------------|--------------------|
| Acoustic | `organic-foundation` + `transient-bass` | Convertir 1 preset existant |
| 808 | `sub-foundation` + `transient-bass` | Convertir 1 preset existant |
| Synth | `character-bass` + `lead-bass` + `texture-bass` | Convertir / créer si manquant |

---

## 7. PHASE 5 — QA, build & release

### 7.1 Tests automatiques (à passer 100 % vert)

```powershell
# 1. Tests unitaires DSP
cmake --build build --config Release --target UWdeVST_bass_tests
.\build\UWdeVST_bass_tests_artefacts\Release\UWdeVST_bass_tests.exe

# 2. Validation banque presets
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe `
    --validate-presets --report qa/bass_preset_qa_report.csv

# 3. Benchmark CPU vs baseline (régression < 20 %)
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe `
    --benchmark-cpu --report qa/bass_cpu_benchmark.csv `
    --baseline qa/bass_cpu_benchmark.csv --threshold 0.20
```

### 7.2 Écoute manuelle (release checklist élargie)

Reprendre [bass_release_checklist.md](bass_release_checklist.md) + ajouter :

- [ ] Test aveugle Contrebasse vs Fingered (P1)
- [ ] Sub 808 / Boom 808 — queue ≤ 8 s confirmée à l'oreille (P4)
- [ ] Distorted 808 — fondamentale audible à C1 (P6)
- [ ] Reese — accord 3-notes sur C1 lisible (P5, P11)
- [ ] Boom 808 — pas de queue parasite entre 2 notes proches (P13)
- [ ] Tous presets `transient-bass` — répétitions 16e à 130 BPM lisibles
- [ ] Toutes familles — loudness équivalente sur preset par défaut (P15)

### 7.3 Critères Go/No-Go

**GO** ssi :
- Tests + renderer + benchmark verts
- Toutes les cases d'écoute manuelle cochées
- `BASS_PATCH_CHANGELOG.md` mis à jour avec entrées Phase 1→4

**NO-GO** si :
- Une basse 808 brouille le kick en mono sur preset utilitaire
- Un preset clip ou silencieux
- Régression CPU > 20 %
- Régression auditive identifiée par les testeurs sur un preset auparavant validé

---

## 8. Tableau de bord priorisé

| Priorité | Tâche | Phase | Fichier | Effort | Impact musical |
|----------|-------|-------|---------|--------|----------------|
| 🔴 P0 | Plafonner `decay2Ratio` 808 + clamp DSP | P1 | BassDefs.cpp, BassVoice.cpp | S | 9/10 |
| 🔴 P0 | Réduire saturation Distorted 808 + HPF | P1 | BassDefs.cpp, BassVoice.cpp | S | 8/10 |
| 🟠 P1 | Cohérence body 808 (option B) | P1 | BassDefs.cpp | XS | 7/10 |
| 🟠 P1 | **NEW** `pitch_env_time` param APVTS + BassVoice | P1.5 | PluginProcessor.cpp, BassVoice.cpp, BassDefs.h | M | 9/10 |
| 🟠 P1 | **NEW** `snap` param APVTS + BassVoice | P1.5 | PluginProcessor.cpp, BassVoice.cpp, BassDefs.h | S | 7/10 |
| 🟠 P1 | **NEW** `env_shape` param APVTS + BassVoice | P1.5 | PluginProcessor.cpp, BassVoice.cpp, BassDefs.h | S | 8/10 |
| 🟠 P1 | Repolariser Contrebasse / Fingered + pluck léger | P2 | BassDefs.cpp | S | 7/10 |
| 🟠 P1 | Reese release partagée + HF roll-off | P2 | BassVoice.cpp | M | 7/10 |
| 🟡 P2 | Defaults FX par famille (Compressor on 808) | P3 | BassDefs.h/cpp | S | 6/10 |
| 🟡 P2 | Aligner loudness familles | P3 | FactoryPresets.cpp | M | 6/10 |
| 🟡 P2 | Slap pluck atténué + shelf HF | P2 | BassDefs.cpp, BassVoice.cpp | XS | 5/10 |
| 🟢 P3 | Repositionner Distorted 808 (pitch env, cutoff) | P2 | BassDefs.cpp | XS | 5/10 |
| 🟢 P3 | Forcer `satMix=0` sur presets Distorted | P3 | FactoryPresets.cpp | XS | 5/10 |
| 🟢 P3 | Nettoyage doublons + taxonomy | P4 | FactoryPresets.cpp | L | 5/10 |
| ⚪ P4 | Validation QA + release | P5 | qa/* | M | — |

Légende effort : XS < 30 min · S = 30–90 min · M = ½ journée · L = 1 journée.

---

## 9. Analyse : faut-il casser la règle des paramètres gelés ?

### 9.1 Réponse directe

**Oui, sur 3 paramètres précis.** La règle de gel était justifiée pour éviter les régressions de presets. Mais 3 absences sont des **angles morts musicaux réels** qui dégradent l'expressivité du produit pour les cas d'usage les plus fréquents. Les ajouter ne casse pas les presets existants (les nouveaux params sont ajoutés en fin de layout APVTS, avec une valeur par défaut qui reproduit le comportement actuel). La rétrocompatibilité est préservée.

Ce qui **n'a PAS de valeur ajoutée** et ne sera pas ajouté :
- `detune` knob → déjà couvert par `character` via `computeDetuneBlend()` pour Reese. Inutile de doubler.
- LFO rate par basse → le LFO global est suffisant.
- Paramètres FX additionnels → chaîne déjà complète.
- Un "Inharmonicity" knob → effet trop subtil et trop peu demandé.

---

### 9.2 Les 3 paramètres à ajouter

#### ① `pitch_env_time` — Durée du pitch drop (808)

**Pourquoi la règle mérite d'être cassée** : le knob `pitch_env` contrôle la *profondeur* du drop, mais la *durée* (`pitchEnvSeconds`) est actuellement fixe dans `BassCharacteristics`. Or pour tout producteur trap/hip-hop, la durée du drop est aussi fondamentale que la profondeur — c'est littéralement la moitié du caractère d'un 808. Un paramètre "Pitch Env" sans contrôle du temps est un contrôle tronqué.

| Aspect | Détail |
|--------|--------|
| ID APVTS | `bass_<N>_pitch_env_time` (même convention que les 15 params existants) |
| Range | 0.02 s → 0.60 s (skew 0.35) — défaut 0.0 = comportement actuel (valeur de `chars`) |
| Défaut logique | `chars.pitchEnvSeconds` (comportement inchangé si non touché) |
| Visible dans l'UI | **Uniquement pour les 808** — masqué pour Acoustic et Synth |
| Impact BassVoice | `pitchEnvDecayCoeff` recalculé depuis ce param quand > 0 |
| Presets existants | Rétrocompatibles — valeur 0.0 = utilise le chars défaut |

#### ② `snap` — Transient d'attaque percussif (Acoustic)

**Pourquoi la règle mérite d'être cassée** : `pluckAmount` est fixé en dur dans `BassCharacteristics`. Pour les basses acoustiques, le montant de "bruit de pluck" initial (le clic de l'ongle / du doigt) est le différenciateur principal entre un son posé (Fingered legato) et un son percussif (Slap). Actuellement il est imposé et non contrôlable. C'est un manque expressif majeur pour le style de jeu.

| Aspect | Détail |
|--------|--------|
| ID APVTS | `bass_<N>_snap` |
| Range | 0.0 → 1.0 (défaut = `chars.pluckAmount` normalisé = comportement actuel) |
| Défaut logique | Valeur de `chars.pluckAmount` — inchangé si non touché |
| Visible dans l'UI | **Uniquement pour Acoustic** — masqué pour 808 et Synth |
| Impact BassVoice | Multiplie `chars.pluckAmount` par la valeur normalisée |
| Presets existants | Rétrocompatibles — défaut = 1.0 (échelle neutre) |

#### ③ `env_shape` — Forme de la queue d'enveloppe (toutes familles)

**Pourquoi la règle mérite d'être cassée** : L'enveloppe en deux phases (`decay1` fast-snap + `decay2` long-tail) est un des points forts du moteur, mais elle est entièrement opaque pour l'utilisateur. Le `decay` knob contrôle le temps total, mais pas si l'enveloppe est plutôt "punchy court + queue longue" ou "fondu progressif". Ce knob `env_shape` exposerait directement le `decay1Ratio / decay2Ratio` blend — musically c'est la différence entre un 808 qui claque et disparaît vs un 808 qui sonne long. Très utile.

| Aspect | Détail |
|--------|--------|
| ID APVTS | `bass_<N>_env_shape` |
| Range | 0.0 → 1.0 (0.0 = tout decay1 / court, 1.0 = tout decay2 / long) |
| Défaut logique | 0.5 = comportement actuel (ratios de `chars` inchangés) |
| Visible dans l'UI | Toutes familles |
| Impact BassVoice | `decay1Ratio_effective = lerp(chars.decay1Ratio * 1.5, chars.decay2Ratio * 0.5, shape)` + inverse pour decay2 |
| Presets existants | Rétrocompatibles — défaut 0.5 = neutre |

---

### 9.3 Implémentation (PHASE 1.5 — entre Phase 1 et Phase 2)

**Fichiers touchés** : `PluginProcessor.cpp` (layout), `BassVoice.h/.cpp` (lecture + calcul), `BassDefs.h` (`BassSettings` : +3 champs).

#### Étape A — `BassSettings` (BassDefs.h)

```cpp
struct BassSettings
{
    // ... champs existants ...
    float pitchEnvTime  = 0.0f;   // 0.0 = use chars default, >0 = override en secondes
    float snap          = 1.0f;   // scale factor for chars.pluckAmount (1.0 = neutre)
    float envShape      = 0.5f;   // 0 = punchy snap, 1 = long tail blend
};
```

#### Étape B — APVTS layout (PluginProcessor.cpp)

Dans la boucle `for (int b = 0; b < mbs::kNumBasses; ++b)`, après le `resonance` param existant :

```cpp
layout.add(std::make_unique<juce::AudioParameterFloat>(
    makeBassParamId(b, "pitch_env_time"), prefix + "Pitch Env Time",
    juce::NormalisableRange<float>(0.0f, 0.60f, 0.001f, 0.35f), 0.0f));
layout.add(std::make_unique<juce::AudioParameterFloat>(
    makeBassParamId(b, "snap"), prefix + "Snap",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));
layout.add(std::make_unique<juce::AudioParameterFloat>(
    makeBassParamId(b, "env_shape"), prefix + "Env Shape",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
```

#### Étape C — BassVoice.cpp : lecture et application

Dans `updateEnvelopeCoefficients` :

```cpp
// Env Shape : blend decay1/decay2 autour des valeurs chars
const float shape = juce::jlimit(0.0f, 1.0f, settings.envShape);
const float d1RatioEff = juce::jmap(shape, 0.0f, 1.0f, chars.decay1Ratio * 1.4f, chars.decay1Ratio * 0.6f);
const float d2RatioEff = juce::jmap(shape, 0.0f, 1.0f, chars.decay2Ratio * 0.5f, chars.decay2Ratio * 1.4f);
const float d1Time = std::max(0.01f, decaySeconds * d1RatioEff);
const float d2Time = std::min(8.0f, std::max(0.05f, decaySeconds * d2RatioEff));  // hard cap P4
```

Dans le calcul du pitch envelope :

```cpp
// Pitch Env Time : override si settings.pitchEnvTime > 0
const float pitchTimeSec = (settings.pitchEnvTime > 0.005f)
    ? settings.pitchEnvTime
    : chars.pitchEnvSeconds;
pitchEnvDecayCoeff = std::exp(-1.0f / (pitchTimeSec * fsr));
```

Au moment du pluck transient :

```cpp
// Snap : scale pluckAmount
const float effectivePluck = chars.pluckAmount * juce::jlimit(0.0f, 1.0f, settings.snap);
```

#### Étape D — readSettingsFromAPVTS (PluginProcessor.cpp)

Ajouter la lecture des 3 nouveaux champs dans la fonction qui mappe l'APVTS → `BassSettings` :

```cpp
settings.pitchEnvTime = readParam(b, "pitch_env_time", 0.0f);
settings.snap         = readParam(b, "snap",           1.0f);
settings.envShape     = readParam(b, "env_shape",      0.5f);
```

#### Étape E — Presets factory

Les presets existants n'ont pas ces champs → le `readFiniteXmlFloat` retournera les défauts (0.0, 1.0, 0.5) → comportement inchangé. **Rétrocompatibilité garantie.**

Pour les nouveaux presets, utiliser ces valeurs de manière expressive :
- Presets 808 avec pitch drop typique : `pitch_env_time` entre 0.04 et 0.10s
- Presets acoustiques percussifs : `snap` entre 0.6 et 1.0
- Presets "pad bass" / texture : `env_shape` entre 0.7 et 1.0
- Presets "punch bass" : `env_shape` entre 0.0 et 0.35

---

### 9.4 Récapitulatif — Règle cassée avec justification

| Paramètre | Vrai apport | Rétrocompat | Complexité UI | Verdict |
|-----------|-------------|-------------|---------------|---------|
| `pitch_env_time` | ✅ Crucial pour 808 (contrôle la moitié du caractère) | ✅ défaut = chars | Faible (visible 808 seul) | **Ajouter** |
| `snap` | ✅ Expressivité Acoustic réelle | ✅ défaut = 1.0 | Faible (visible Acoustic seul) | **Ajouter** |
| `env_shape` | ✅ Punch vs tail — différencie les usages rythmiques | ✅ défaut = 0.5 | Faible (1 knob universel) | **Ajouter** |
| `detune` | ❌ Déjà couvert par `character` (Reese) | — | — | **Ne pas ajouter** |

Ces 3 paramètres comblent des absences qui dégradent l'expressivité du produit de façon mesurable. Ce ne sont pas des features de confort : elles répondent à des gestes musicaux standard que le produit promettait implicitement et ne fournissait pas.

---

## 10. Ce qui n'est PAS dans ce plan (volontairement)

- **Pas de refonte UI / macros** — sortie du périmètre, voir Phase 4 du changelog historique.
- **Pas d'ajout de basse #10** ni de fusion Contrebasse/Fingered — la palette à 9 reste figée, on les différencie par DSP/presets.
- **Pas de remplacement du moteur additif** — il est sain, on ajuste seulement les coefficients.
- **Pas de modification du système d'attachements** (`AttachmentHelpers.h`) ni du contenu de `new composants/*` — le binding APVTS reste tel quel.

---

## 10. Annexes — pointeurs code utiles

- Table `kChars` (toutes les caractéristiques par bass) : [Source/Engine/BassDefs.cpp](../Source/Engine/BassDefs.cpp#L37-L106)
- Defaults user (table `kDefaults`) : [Source/Engine/BassDefs.cpp](../Source/Engine/BassDefs.cpp#L111-L135)
- Defaults FX globaux : [Source/Engine/BassDefs.h](../Source/Engine/BassDefs.h#L84-L136)
- Enveloppe / clamps DSP : [Source/Engine/BassVoice.cpp](../Source/Engine/BassVoice.cpp#L11-L31)
- FX availability par bass : [Source/Engine/BassDefs.cpp](../Source/Engine/BassDefs.cpp#L235-L249)
- Banque factory : [Source/Engine/FactoryPresets.cpp](../Source/Engine/FactoryPresets.cpp)
- Audit source : [BASS_AUDIT_COMPLET.md](BASS_AUDIT_COMPLET.md)
- Historique fixes : [BASS_PATCH_CHANGELOG.md](BASS_PATCH_CHANGELOG.md)
- Checklist release : [bass_release_checklist.md](bass_release_checklist.md)

---

**Fin du plan.** À exécuter phase par phase, en mettant à jour `BASS_PATCH_CHANGELOG.md` à chaque commit.
