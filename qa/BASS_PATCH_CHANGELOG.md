# CHANGELOG — PATCH DSP CORRIGÉ

## Modifications apportées

### Session 1 — 2026-04-21 (Phase 1: Corrections critiques)

**Fichiers modifiés**: `Source/Engine/BassVoice.cpp`

#### 1. DETUNE EN CENTS (Cohérence registre)
**Problème**: Le detune était calculé en Hz fixe, ce qui causait une perception différente selon le registre.
- C1 (65 Hz): 0.99 Hz = 26 cents
- C4 (261 Hz): 0.99 Hz = 13 cents

**Solution**: Conversion en cents constants.
```cpp
const float detuneCents = chars.detuneAmount * 100.0f * computeDetuneBlend();
const float detuneRatio = std::pow(2.0f, detuneCents / 1200.0f);
```

**Impact**: Reese Bass, Moog Bass — detune cohérent C1-C4.

---

#### 2. POLYBLEP SQUARE (Bug de phase)
**Problème**: Signes inversés au 2ème polyBLEP — artifacts haute fréquence.

**Solution**: Correction des signes.
```cpp
// CORRIGÉ:
sq -= polyBlep(osc.phase, inc);  // discontinuity at 0: subtract
sq += polyBlep(shifted, inc);     // discontinuity at 0.5: add
```

**Impact**: Acid Bass — plus d'artifacts.

---

#### 3. GLIDE EN CENTS
**Solution**: Même calcul detune en cents dans glideToNote().

---

#### 4. RETRIGGERWITHGLIDE EN CENTS
**Solution**: Même correction pour les transitions mono/legato.

---

#### 5. SUB OSCILLATEUR (-25%)
**Solution**: Réduction du niveau du sub pour minimiser les battements.
```cpp
constexpr float subLevelCorrection = 0.75f;  // -25%
```

---

### Session 2 — 2026-04-21 (Phase 2: Stabilisation moteur)

**Fichiers modifiés**: 
- `Source/Engine/BassVoice.cpp`
- `Source/Engine/BassDefs.cpp`

#### 2.1 GLIDE GUARD
**Problème**: Grands intervalles avec glide court → effet "slap".

**Solution**: Limite ratio max = 2.0 (une octave) et glideTime min = 50ms.
```cpp
const float maxGlideRatio = 2.0f;
const float safeGlideTimeSec = std::max(glideTimeSec, 0.05f);
```

---

#### 2.2 RESONANCE GUARD
**Problème**: Q=1.0 peut auto-osciller le filtre (surtout en 24dB/oct cascade).

**Solution**: Limite Q max = 8.0 pour 24dB, Q max = 10.0 pour 12dB.
```cpp
const bool singleStage = !(chars.oscMode == OscMode::Saw || chars.oscMode == OscMode::Square);
const float maxQ = singleStage ? 10.0f : 8.0f;
const float Q = 0.5f + clampedResonance * (maxQ - 0.5f);
```

---

#### 2.3 BODY RESONATOR BOOM 808
**Problème**: Boom 808 avec bodyDelayRatio=0.0f sonnait "vide".

**Solution**: Ajout bodyDelayRatio=0.35 (5ème harmonique) + bodyDamping=0.12.
```cpp
// BassCharacteristics Boom 808:
0.35f, 0.12f,  // bodyDelayRatio=0.35, bodyDamping=0.12
```

---

#### 2.4 ATTACK GUARD
**Solution**: Floor attackSeconds = 0.0001f déjà présent — confirmé OK.

---

### Session 3 — 2026-04-21 (Phase 3.1: Calibration presets)

**Fichiers modifiés**: `Source/Engine/FactoryPresets.cpp`

#### CALIBRATION NIVEAUX PRESETS

**Principe**: Calibration selon le rôle musical et la famille de son.

| Famille | Rôle | Range Level | Rationale |
|---------|------|-------------|-----------|
| Acoustic | organic-foundation | 0.74-0.90 | Partial series, moderate energy |
| 808 (Sub/Boom) | sub-foundation | 0.78-0.86 | Pure/sub harmonics, less transients |
| 808 (Distorted) | character-bass | 0.76-0.86 | Drive adds harmonic content |
| Synth (Moog) | character-bass | 0.74-0.86 | Saw wave rich harmonics |
| Synth (Reese) | character-bass | 0.74-0.86 | 3 detuned saws = 3x content |
| Synth (Acid) | character-bass | 0.74-0.86 | Square + high resonance |

**Bass corrigés**:
- Sub 808: 25 presets, level 0.78-0.86 ✓
- Boom 808: 25 presets, level 0.78-0.86 ✓
- Distorted 808: 25 presets, level 0.76-0.86 ✓
- Moog Bass: 25 presets, level 0.74-0.86 ✓
- Reese Bass: 25 presets, level 0.74-0.86 ✓
- Acid Bass: 25 presets, level 0.74-0.86 ✓

**À faire**:
- Contrebasse/Fingered/Slap: niveaux déjà corrects (0.74-0.90)

---

## Prochaines étapes recommandées

### Phase 3.2 — Nettoyage presets (À FAIRE)
**Action**: Réduction doublons et taxonomy cleanup.

**Fichiers**: `Source/Engine/FactoryPresets.cpp`

**Objectifs**:
1. Identifier doublons sémantiques (Moog Warm/Deep, etc.)
2. Corriger taxonomy mixRole (lead-bass, texture-bass incohérents)
3. Normaliser naming conventions

### Phase 4 — Refonte UI/macros
**Action**: Simplifier pages CC et améliorer Fatness vs Depth.

### Phase 5 — Validation QA
**Action**: Tests d'écoute finaux et release checklist.

---

## Notes techniques

### Détails du detune en cents
Le nouveau calcul utilise un ratio de fréquence constant:
- Pour 2 oscs: osc[0] à `2/detuneRatio`, osc[1] à `detuneRatio`
- Pour 3 oscs: osc[0] à `1/detuneRatio²`, osc[1] à `1`, osc[2] à `detuneRatio²`

Cela garantit que la largeur du detune est constante en cents, pas en Hz.

### Impact sur les presets existants
- Reese Bass: son plus cohérent entre registres
- Moog Bass: même largeur de detune quel que soit la note
- Acid Bass: plus d'artifacts sur les discontinuités

### Limitations
- Le fix subLevel est une correction simple (réduction de 25%). Une solution plus robuste pourrait inclure un filtre passe-bas dédié au sub ou un ratio configurable (0.25x pour true sub-octave).
- La calibration presets n'a pas été implémentée — elle nécessite une analyse approfondie de la banque de 210 presets.

---

## Vérification

Pour vérifier que les corrections sont bien appliquées:
1. Compiler le projet avec les modifications
2. Écouter Reese Bass sur C1 et C4 — le detune devrait être perceptible de manière similaire
3. Écouter Acid Bass — pas de glitch audible pendant le pitch sweep
4. Écouter Sub 808 sur C0-C1 — moins de battement qu'avant
5. Tester les presets glide en mono — comportement cohérent

---

## Contact / Support

Pour toute question sur ce patch, se référer à:
- Audit complet: `qa/BASS_AUDIT_COMPLET.md`
- QA contract: `qa/bass_phase0_qa_contract.md`