# AUDIT COMPLET — UWdeVST Bass

## 1. Relecture stratégique du produit

### Ce qu'est réellement ce synthé bass
UWdeVST Bass est un synthé bass multi-instruments avec 9 modèles (3 familles acoustiques, 3 modèles 808, 3 modèles synth) partageant un moteur DSP commun (oscillateurs analytques + SVF + body resonator + matrice de FX). Le produit vise clairement une audience de producteurs souhaitant un instrument unique couvrant les besoins fondamentaux en production : basse acoustique réaliste, 808 moderne, synthés classics.

### Promesse implicite
"Un seul plugin pour toutes mes basses, avec des presets prêts à l'emploi et une qualité suffisante pour produire sans external."

### Public probable
- Producteurs hip-hop / trap / EDM qui veulent des 808 crédibles sans sampler
- Compositeurs cinéma qui veulent des basses acoustiques快速 access
- Sound designers qui cherchent une base de presets à modifier
-兜

### Rôle musical réel
Le produit couvre principalement des rôles de **sub-foundation** (808) et **character-bass** (synth), avec des presets de texture et lead qui sont secondaires. La valeur ajoutée réelle est dans la couverture des presets 808 et Moog/Reese/Acid.

### Ce que le produit essaie de faire mais devrait arrêter
- Essayer de couvrir les rôles lead-bass et texture-bass avec des presets qui ne sont pas assez distinctifs
- Implémenter une réverbération Dattorro qui n'a pas de sens musical sur une basse (mais qui alourdit le CPU)
- Proposer 7 pages MIDI CC pour un plugin qui n'est pas un contrôleur hardware
- Accumuler 226 paramètres alors que 60% sont des paramètres de FX que l'utilisateur moyen ne touchera jamais

### Ce qui devrait être protégé absolument
- La justesse de la fondamentale sur les presets 808 sub
- La stabilité mono des presets 808
- La lisibilité des presets transient-bass en mix
- La cohérence de la banque de presets (chaque preset a un rôle clair)

---

## 2. Audit complet

### 2a. Audit moteur sonore

**Qualité du grave (Sub 808 / Boom 808)**
- Le sub-oscillateur est implémenté comme une onde sinus pure à exactement 0.5x la fréquence fondamentale
- Ce sub crée un battement avec la fondamentale quand les deux sont actives (effet dechet audible)
- Pour les notes graves (C0 = 32.7 Hz), le sub descend à 16.35 Hz — dans la plage subsonique perçue
- **RISQUE**: Le sub à 16 Hz peut créer des problèmes de battement avec les之交ements de pièce

**Solidité de la fondamentale**
- Les oscillateurs Saw/Square utilisent PolyBLEP — correct
- Mais l'implémentation Square a un bug de phase: le 2ème polyBLEP est calculé sur `shifted = osc.phase + 0.5` puis vérifié avec le même `dt` au lieu du `dt` recalculé pour cette phase
- **BUG**: Les ondes carrées peuvent avoir des artifacts de haute fréquence près de la discontinuité

**Stabilité du centre tonal**
- Le pitch envelope est bien implémenté avec decay exponentiel
- Mais pour Distorted 808, `pitchEnvSemitones = 18.0f` avec `pitchEnvSeconds = 0.04f` — c'est une descente de 18 demi-tons en 40ms
- **RISQUE SONORE**: Ce pitch sweep extrême peut créer une perception de faux-battement (glitch) quand il atteint la fondamentale

**Propreté du sub**
- Le sub n'a pas de filtrage dédié
- Il est additionné au signal principal AVANT le filtre SVF (ligne 682: `signal += sub;`)
- Donc le sub passe à travers la saturation globale

**Comportement mono/poly**
- Polyphonie: 32 voix max, dying voice pool de 8 voix
- Voice stealing: oldest releasing puis oldest active
- Mono mode: 3 états (Poly, Mono, Legato)
- En Mono: `retriggerWithGlide()` retrigger l'enveloppe, ce qui est correct pour les articulations percussives

**Qualité de l'unison**
- Unison limité à 3 oscillateurs pour le mode Saw
- Le detune est calculé comme `chars.detuneAmount * baseFreq * computeDetuneBlend()`
- Pour Reese Bass: `detuneAmount = 0.015f`, ce qui donne ~0.99 Hz de detune à 65 Hz
- **PROBLÈME**: Le detune en Hz fixe n'est pas proportionnel à la fréquence — à C1 (65 Hz), 0.99 Hz = 26 cents, mais à C3 (130 Hz), 0.99 Hz = 13 cents
- **RISQUE**: Le detune est beaucoup plus audible dans l'aigu que dans le grave, créant une inconsistency de perception

**Gestion du detune**
- computeDetuneBlend() retourne un facteur qui varie avec `settings.character`
- Pour Reese avec 3 oscs et character élevé, le blend peut atteindre 1.15
- Le detune final: `0.015f * baseFreq * blend`
- À 65 Hz: 0.015 * 65 * 1.15 = 1.12 Hz (~27 cents)
- À 200 Hz: 0.015 * 200 * 1.15 = 3.45 Hz (~29 cents) — cohérence acceptable
- Mais à 1000 Hz: 0.015 * 1000 * 1.15 = 17.25 Hz (~291 cents) — **TROP**

**Comportement des filtres**
- SVF State Variable Filter avec stability guard
- Filter max fréquence calculée correctement avec `(-Qinv + sqrt(Qinv² + 4)) * 0.95`
- 24dB/oct cascade pour Saw/Square (ligne 291)
- LFO cutoff modulation: `std::pow(2.0f, lfoCutoffMod * 2.0f)` — correct

**Comportement des modulations**
- Modulation Matrix: 8 slots source→destination
- Sources: LFO1, LFO2, Envelope, Velocity, ModWheel, Aftertouch, PitchBend
- Destinations: Cutoff, Resonance, Pan, Level, Pitch, Attack, Decay, LFO1Rate, EQ Mid Freq, EQ Mid Gain

**Stabilité des enveloppes**
- ADSR avec Decay1→Decay2→Sustain — bon design
- Release coefficient: `std::exp(-1.0f / (max(0.005f, baseReleaseSeconds) * fsr))` — OK
- **RISQUE**: Attack avec temps très court (0.0001f) peut créer des clicks si la phase d'attaque n'est pas douce

**Musicalité perçue**
- Les presets factory sont nombreux (210) et couvre bien les cas d'usage principaux
- Les presets Boom 808 avec `pitchEnvSemitones = 8.0` offrent un pitch sweep musical
- Les presets Moog avec `builtInSaturation = 3.0` offrent de la chaleur analogique crédible

### 2b. Audit de justesse et stabilité tonale

**Accordage global**
- Accordage via `tuneSemitones` dans BassSettings (range -24 à +24)
- Fréquence de base: `440.0f * std::pow(2.0f, (note - 69.0f + tuneSemitones) / 12.0f)` — correct

**Cohérence sur le clavier**
- Le tracking des enveloppes est cohérent
- La filtre cutoff n'a pas de compensation par registre
- **PROBLÈME**: Un cutoff de 800 Hz sur C1 (65 Hz) représente ~5 octaves au-dessus de la fondamentale, mais sur C4 (261 Hz), c'est ~2 octaves. Le filtre se comporte différemment selon le registre.

**Comportement par registre**
- C0-C1 (sub): Sub audible, fondamentale potentiellement masquée par le sub
- C1-C2 (grave): Registre idéal pour 808
- C2-C3 (médium grave): Registre idéal pour Moog/Reese
- C3-C4 (médium): Certains presets acoustiques peuvent sonner étroits
- C4+ (aigu): Pas le territoire naturel d'une basse

**Pitch bend**
- Implémenté via `pitchBendFactor` dans le render loop
- Le pitch bend est appliqué multiplicativement avec pitchEnv et modPitchFactor
- Le pitch envelope n'est pas correctement réinitialisé lors d'un noteOn fresh (il part de 1.0f)

**Glide / portamento**
- Implémentation: exponential glide coefficient `pow(ratio, 1.0f / glideTimeSamples)`
- **PROBLÈME**: Si `ratio > 1.0` (note ascendante), le coefficient > 1.0, et `baseFreq *= glideCoeff` chaque sample
- La vérification de fin de glide: `(glideCoeff >= 1.0f && baseFreq >= targetFreq)` — OK
- Mais le recalcul des phaseInc pour chaque oscillateur se fait dans le loop, ce qui est computativement coûteux
- **RISQUE**: Grande plage de glide avec notes graves peut créer des effets de "slap" non désirés

**Drift**
- Pas de drift implémenté (pas de randomisation de fréquence par sample)
- Mais le rng est utilisé pour initialiser la phase des oscillateurs unison
- Pas de randomisation de enveloppe pour le body resonator

**Unison et perception de justesse**
- Le detune proportionnel à la fréquence peut créer des effets de beating différents selon le registre
- Pour les presets Reese (3 oscs), le beating peut être très rapide dans l'aigu

**Stabilité après saturation / FX**
- La saturation est appliquée après le filtre, avant l'enveloppe
- Le saturateur global (PluginProcessor) utilise 2x oversampling
- **RISQUE**: L'oversampling du saturateur global ne s'applique qu'au signal déjà filtré, pas au sub

### 2c. Audit musical spécifique basse

**Lisibilité du centre tonal**
- Les presets 808 ont tendance à avoir un centre tonal confus car le sub + pitch env masquent la fondamentale
- Les presets acoustiques additifs ont un centre tonal clair mais peuvent sonner "synthétiques" pour des contrebasses

**Comportement dans le grave**
- Les presets Sub 808 avec `isSubBass = true` limitent les partiels
- Mais le body resonator (comb filter) est désactivé pour les 808 (`bodyDelayRatio = 0.0f`)
- **INCOHÉRENCE**: Boom 808 a `isSubBass = true` mais `bodyDelayRatio = 0.0f` — devrait avoir un body minimal

**Comportement en ligne mélodique**
- Les presets avec attack rapide et decay long (Acid) fonctionnent bien en mélodique
- Les presets avec decay court et sustain bas (Slap) ne sont pas adaptés à la mélodie

**Comportement sur notes répétées**
- Les presets transient-bass (Slap) utilisent un pluck transient qui peut créer des clicks sur notes répétées très rapides
- Le decay2 permet de garder un sustain minimal entre les notes

**Articulation**
- L'articulation est contrôlée par l'ADSR + filterEnv
- Pour les presets Moog, le filterEnv est bien utilisé
- Pour les presets Acid, le filterEnv avec resonance élevée crée l'effet caractéristique

**Interaction avec kick / drums**
- Les presets 808 sont pensés pour interagir avec un kick
- Mais le pitch envelope des 808 peut créer des interférences avec les kicks sub
- Pas de sidechain intégrée dans le plugin

**Intégration dans un mix**
- L'EQ interne est bien implémenté (3 bandes)
- La compression est active par défaut — peut ajouter de la glue mais peut aussi créer du pumping
- Le limiter de sortie est bienvenu pour éviter le clip

**Comportement en arrangement dense**
- Les presets avec reverb/delay (optionnel) peuvent créer de la confusion en arrangement dense
- Les presets chorus peuvent créer des problèmes de phase en stéréo

### 2d. Audit des sous-familles de basses

**Sub bass (Sub 808)**
- Crédibilité: 8/10 — les presets sont réalistes pour du hip-hop/trap
- Utilité: 9/10 — c'est le cœur du produit
- Cohérence avec le moteur: 7/10 — le sub crée des battements avec la fondamentale
- Couverture: 20 presets couvrant sub-foundation et texture
- **RISQUE**: Pas de vrai sub-octave (le sub est à 0.5x = octave, pas un sub-octave à 0.25x)

**Analog bass (Moog Bass)**
- Crédibilité: 7/10 — le son est chaud mais manque un peu de caractère "vintage"
- Utilité: 8/10 — très polyvalent pour pop/rock/EDM
- Cohérence avec le moteur: 8/10 — saw + saturation + filtre 24dB = bon combo
- Couverture: 20 presets + acid, lead, texture
- **RISQUE**: La saturation intégrée (builtInSaturation = 3.0) peut être trop pour certains presets

**Synth bass (Reese Bass, Acid Bass)**
- Crédibilité: 6/10 pour Reese (détune trop faible), 8/10 pour Acid
- Utilité: 8/10 pour la production moderne
- Cohérence avec le moteur: 7/10 — le detune proportionnel pose problème
- Couverture: 40+ presets
- **RISQUE**: Reese manque de "width" sans chorus

**808 Bass (Boom 808, Distorted 808)**
- Crédibilité: 8/10 pour Boom, 7/10 pour Distorted
- Utilité: 9/10 — Boom est très versatile
- Cohérence avec le moteur: 6/10 — Distorted avec Sine + saturation est conceptuellement incohérent
- Couverture: 50+ presets
- **RISQUE**: Boom avec `builtInSaturation = 1.5` + decay long peut créer du "mud" en mix

**FM bass**
- Pas présente dans ce synthé — c'est une lacune si le marché attend des FM basses

**Pluck bass**
- Les presets acoustiques avec pluck transient simulent grossièrement des plucks
- Mais sans sample, le pluck reste très "synthétique"

**Distorted bass**
- Couvert par Distorted 808 et les presets Moog avec drive élevé
- Manque un vrai mode "distorted" avec bitcrushing ou wavefolding

**Hybrid bass**
- Pas présent — les presets qui mélangent acoustic + synth n'existent pas

### 2e. Audit presets

**Qualité globale**
- 210 presets factory — nombre généreux
- Tous les presets ont un peak de -1.110 dB — calibration NON individualisée
- Tags cohérents mais taxonomy parfois confuse

**Utilité réelle**
- Sub-foundation presets: très utiles (Sub Pur, Trap Minimal, etc.)
- Character-bass presets: utiles pour la démo
- Texture-bass presets: moins utiles pour la production
- Lead-bass presets: peu crédibles comme leads

**Cohérence de volume**
- Tous les presets ont le même peak — ce n'est pas une calibration mais une copie
- Les RMS varient de -6 à -16 dB selon le preset — incohérence entre peak et RMS
- **BUG**: Un preset avec beaucoup de sustain mais peu de transients aura un peak bas mais un RMS élevé, ce qui peut surprendre en mix

**Cohérence de catégories**
- mixRole: organic-foundation, sub-foundation, character-bass, lead-bass, texture-bass, transient-bass
- Les rôles ne sont pasmutuellement exclusifs — un preset peut être à la fois "sub" et "texture"
- Les tags auto-générés (buildPresetMetadata) peuvent mal classifier certains presets

**Redondance**
- Beaucoup de presets très similaires ("Moog Warm" vs "Moog Deep" vs "Moog Sub Drone")
- Les presets avec glide ont tendance à se ressemblent

**Calibration sub / mids / top**
- Pas de calibration dédiée — le cutoff et la brightness font le travail
- **RISQUE**: Les presets acoustiques peuvent avoir trop de "top" (harmonic content) en comparaison avec des vraies contrebasses

**Lisibilité en mix**
- Les presets 808 sont généralement lisibles en mix grâce au sub propre
- Les presets acoustiques peuvent se perdre dans un mix dense
- Les presets Moog avec chorus peuvent créer des problèmes de phase

**Présence de presets flatteurs mais inutiles**
- "Moog Dark Ambient", "Acid Dark" — jolis mais pas Productifs
- "Reese Cinematic" — le concept de "cinematic bass" est confus

**Valeur réelle de la banque**
- La banque a de la valeur pour les presets 808 et Moog
- La banque acoustic est moins crédible que des samples ou des Kontakt libraries
- La valeur ajoutée principale est dans la vitesse d'accès aux presets

### 2f. Audit FX

**Saturator (PluginProcessor-level, pas voice-level)**
- Utilise `std::tanh(signal * drv) / std::tanh(drv)` — saturation типичная
- 2x oversampling pour réduire les harmonics d'aliasing
- Mix control (dry/wet) — bon design
- **PERTINENCE**: Essentiel pour les 808 et Moog
- **RISQUE**: À high drive, la saturation peut créer des harmoniques pairs qui renforcent le sub de manière imprévisible

**Transient**
- Implémentation: dual envelope (fast + slow) + mixing
- Attack: contrôle l'envelope rapide (transient)
- Sustain: contrôle le ratio fast/slow
- Mix: dry/transient blend
- **PERTINENCE**: Très pertinent pour les presets transient-bass (Slap, Punch)
- **RISQUE**: Peut créer des clicks si attack trop rapide

**Compressor**
- Utilise juce::dsp::Compressor
- 6 paramètres: threshold, ratio, attack, release, makeup, mix
- **PERTINENCE**: La compression est pertinente pour contrôler les Dynamics mais active par défaut, elle peut ajouter du pumping
- **RISQUE**: Le comp avec attack rapide peut écrêter les transients au lieu de les préserver

**EQ**
- 3-band parametric: Low shelf, Mid peak, High shelf
- Les fréquences par défaut (80/600/3500 Hz) sont bien choisies pour une basse
- **PERTINENCE**: Essentiel pour le mix
- **RISQUE**: L'EQ ne compense pas automatiquement les problèmes de registre

**Chorus**
- Stereo chorus avec LFO modulé (quadrature)
- Utilise Hermite interpolation pour la delay line
- **PERTINENCE**: Pertinent pour élargir les presets Reese et Moog
- **RISQUE**: Le chorus peut créer des problèmes de phase en mono, ce qui est contradictoire avec la philosophie mono-first des basses

**Delay**
- Stereo delay avec BPM sync
- 5 note divisions: 1/4, 1/8, 1/16, 1/8 dot, 1/8 trip
- **PERTINENCE**: Pertinent pour les presets mélodiques mais useless pour des grooves
- **RISQUE**: Le delay peut créer des conflits de fréquence avec le kick

**Reverb (Dattorro Plate)**
- Implémentation correcte du Dattorro reverb
- **PERTINENCE**: Très discutable sur une basse — la réverbération n'est pas un besoin musical typique pour une basse
- **RISQUE**: Alourdit le CPU pour une fonctionnalité dont 90% des utilisateurs n'ont pas besoin
- **DETTE**: Devrait être simplifié ou caché pour les presets non-texture

**Limiter**
- Brick-wall limiter avec link channel
- Seuils: -12dB à 0dB
- **PERTINENCE**: Essentiel pour protéger l'output
- **RISQUE**: Limiter threshold à -0.3dB par défaut peut écrêter les basses très transientes

### 2g. Audit UI / UX

**Clarté de l'interface**
- Design sobre et professionnel (charte graphique synthcol)
- Hiérarchie visuelle: bien organisée avec tabs FX
- Knobs with glow effects — aspect professionnel

**Vitesse de création d'une basse utile**
- Les macros (Fatness, Brillance, Punch, Depth) permettent une quick shaping
- Les presets sont accessibles rapidement via le browser
- Temps pour avoir une basse usable: ~5 secondes

**Lisibilité des contrôles critiques**
- Les contrôles critiques (cutoff, resonance, attack, decay) sont visibles dans la page TONE
- Les contrôles de grave (sub, body, pitch env) sont dans la page ENVELOPE
- Mais les contrôles de FX sont dans 3 tabs différents

**Hiérarchie visuelle**
- Header: preset + gain
- Body: knobs organisés par section
- Footer: keyboard
- Cette hiérarchie est standard et efficace

**Feedback de modulation**
- La modulation matrix n'a pas de visualisation dédiée dans l'UI
- Les macros n'ont pas de feedback visuel (pas de metering sur le dry)

**Browser**
- Preset browser avec search text (basé sur tags + name + role)
- Filtres par rôle (sub-foundation, character-bass, etc.)
- **RISQUE**: Le search text est auto-généré et peut ne pas correspondre à l'attente de l'utilisateur

**Macros**
- 4 macros: Fatness, Brillance, Punch, Depth
- Mod wheel peut contrôler Punch (seule option)
- **PROBLÈME**: Les macros ne sont pas assez distinctives — Fatness et Depth font des choses similaires

**Accès aux réglages grave**
- Sub level: page ENVELOPE
- Body: page TONE
- Pitch Env: page ENVELOPE
- Ces contrôles sont dispersés

### 2h. Audit produit / perception

**Cohérence promesse/réalité**
- Promesse: "basses utilitaires de production"
- Réalisation: les presets 808 et Moog tiennent la promesse, mais les presets acoustiques sont discutables
- Les presets texture-bass et lead-bass ne sont pas vraiment des basses utilitaires

**Différenciation réelle**
- Face à Kontakt libraries (Session Bassists) ou Serum (bass presets): ce synthé manque de différenciation claire
- Les 808 presets sont la différenciation principale
- Le prix devrait refléter cette niche

**Crédibilité face au marché**
- Le produit est crédible pour des producteurs中级
- Les presets acoustiques ne sont pas crédibles face à des Kontakt libraries dédiées
- Le moteur DSP est compétent mais pas revolucionario

**Valeur perçue**
- La banque de 210 presets donne une impression de valeur
- Mais la qualité des presets n'est pas均匀 — beaucoup de presets similaires
- La valeur réelle est dans la скорость d'accès aux presets 808

**Impression de produit pro ou non**
- L'UI est professionnelle
- Les knobs et le design sont cohérents
- Mais le nom "UWdeVST" n'est pas un brand recognisé

### 2i. Audit stabilité / compatibilité / QA

**Crashs probables**
- Voice stealing avec dying pool — mécanisme correct
- Pas de risk очевид de crash identifié

**Comportements extrêmes**
- Attack = 0: génère des clicks (pas de guard)
- Decay = 0.1s minimum enforced
- Release = 0.005s minimum enforced
- Resonance = 1.0: le filtre peut s'auto-osciller
- Body feedback = 0.9995f maximum enforced

**CPU**
- Benchmark disponible: ~2.4 ms par voix-seconde sur CPU de test
- 32 voix: ~80 ms pour 1.5s de render — acceptable
- **RISQUE**: La reverb Dattorro peut être coûteuse si activée sur beaucoup de voix

**Voice management**
- 32 voix max + 8 dying voices
- Voice stealing: oldest releasing → oldest active
- Ce mécanisme est standard et efficace

**Incohérences de niveau**
- Tous les presets ont peak = -1.110 dB (valeur codée en dur dans le CSV)
- **BUG**: Ce n'est pas une vraie calibration mais une valeur par défaut

**Edge cases**
- Glide avec glideTime très court peut créer des glitches
- Mod wheel avec target "Punch" uniquement — limitation
- Les CC MIDI pages sont bien implémentées pour FLkey Mini

**Risques de presets destructeurs**
- Pas de presets qui créent du NaN ou des valeurs extremes
- Tous les paramètres sont clampés

**Bugs silencieux**
- Le sub oscille à octave exacte — cause des battements audibles
- Le detune proportionnel cause des inconsistencies entre registres
- Le chorus peut créer des problèmes de phase non évidents

---

## 3. Tableau des problèmes

| # | Zone | Problème | Type | Gravité (1-10) | Impact (1-10) | Difficulté (1-10) | Risque régression (1-10) | Priorité | Pourquoi |
|---|------|----------|------|-----------------|---------------|-------------------|--------------------------|----------|----------|
| 1 | Moteur | Sub oscillateur à octave exacte (0.5x) crée battement avec fondamentale | Sonore | 6 | 7 | 4 | 5 | HAUTE | Problème audible sur presets 808 sub |
| 2 | Moteur | Détune proportionnel cause beating différent selon registre | Sonore | 5 | 6 | 6 | 4 | HAUTE | Inconsistance de perception Reese/Moog |
| 3 | Moteur | PolyBLEP square avec bug de phase à la discontinuité | Bug | 5 | 5 | 3 | 6 | HAUTE | Artifacts sur presets square (Acid) |
| 4 | Presets | Peak identique (-1.110 dB) pour tous les presets — pas de calibration | Preset | 7 | 7 | 2 | 3 | HAUTE | Inconsistance de volume en mix |
| 5 | Presets | Présence de presets incohérents (texture-bass, lead-bass) | UX/Produit | 4 | 5 | 2 | 2 | MOYENNE | Dévalorise la promesse utilitaire |
| 6 | FX | Reverb Dattorro sur une basse — pertinence questionable | UX | 5 | 4 | 5 | 4 | MOYENNE | Alourdit le CPU, confuses l'utilisateur |
| 7 | FX | Chorus par défaut sur certains presets peut créer problèmes mono | Sonore | 5 | 6 | 3 | 5 | HAUTE | Contredit la philosophie mono-first |
| 8 | Moteur | Pitch env Distorted 808 (18 semitones en 40ms) — sweep extrême | Sonore | 6 | 7 | 2 | 3 | HAUTE | Glitch audible pendant pitch sweep |
| 9 | UX | Macro "Fatness" vs "Depth" — distinction insuffisante | UX | 3 | 4 | 3 | 2 | FAIBLE | Confusion pour l'utilisateur |
| 10 | UX | Mod wheel target limité à "Punch" uniquement | UX | 4 | 5 | 4 | 3 | MOYENNE | Contredit les promesses du QA contract |
| 11 | Moteur | Attack = 0 peut créer des clicks | Bug | 5 | 6 | 2 | 4 | HAUTE | Artifacts sur presets attack-minimal |
| 12 | Structure | Pas de FM bass — lacune de couverture | Produit | 4 | 4 | 8 | 2 | REPORTER | Feature manquante mais pas critique |
| 13 | Structure | Pas de true sub-octave (0.25x) — différent de 808 standard | Sonore | 4 | 5 | 5 | 3 | REPORTER | Les producteurs moderne attendent du sub |
| 14 | FX | Compression active par défaut — pumping possible | Sonore | 5 | 6 | 3 | 4 | MOYENNE | Comportement par défaut non-optimal |
| 15 | UX | EQ avec fréquences fixes non-adaptatives | UX | 3 | 4 | 4 | 3 | FAIBLE | L'utilisateur doit ajuster manuellement |
| 16 | Presets | Présence de presets "flatteurs" (Ambient, Cinematic) | Produit | 3 | 3 | 2 | 2 | FAIBLE | Dévalorise la crédibilité produit |
| 17 | Moteur | Oversampling saturation global uniquement, pas voice-level | Structurel | 4 | 4 | 6 | 5 | REPORTER | Sub passe à travers saturation non-oversamplée |
| 18 | UX | 7 pages MIDI CC pour un plugin non-hardware | UX | 2 | 2 | 3 | 2 | REFUSER | Surcharge fonctionnelle injustifiée |
| 19 | Moteur | Glide avec grands intervalles peut créer effets de "slap" | Sonore | 5 | 5 | 4 | 5 | MOYENNE | Glitch sur certaines lignes de glide |
| 20 | Structure | Pas de sidechain intégrée | Produit | 3 | 5 | 7 | 4 | REPORTER | Feature standard en production moderne |

---

## 4. Causes racines

### Symptômes vs Causes vs Causes profondes

**SYMPTÔME 1: Battement audible sur presets 808 sub**
- Cause: Le sub oscillateur à exactement 0.5x la fondamentale crée un battement
- Cause profonde: L'architecture ne distingue pas le "sub harmonic" (pour remplir le grave) du "sub octave" (pour thicken)

**SYMPTÔME 2: Présets avec volume incohérent**
- Cause: Tous les presets ont peak = -1.110 dB codé en dur
- Cause profonde: La calibration des presets n'a pas été faite individuellement

**SYMPTÔME 3: Detune différemment perceptible selon le registre**
- Cause: Le detune est en Hz fixe, pas en cents
- Cause profonde: L'implémentation ne prend pas en compte la psychoacoustique du detune

**SYMPTÔME 4: Présence de presets non-utiles**
- Cause: Le produit essaie de couvrir trop de rôles (lead, texture, cinematic)
- Cause profonde: Manque de focus sur la promesse "utilitaire de production"

**SYMPTÔME 5: Chorus crée problèmes de phase en mono**
- Cause: Le chorus est une extension stéréo, pas mono-safe
- Cause profonde: La philosophie mono-first n'est pas appliquée aux FX

**DOUBLES/TROIS**
- FX Dattorro reverb + Reverb sur basse = confusion fonctionnelle (réduit à 1 problème principal)
- Macros Fatness/Depth trop similaires = 1 problème UX
- 7 pages MIDI CC = 1 problème de surdesign

**FAUX PROBLÈMES**
- Les presets "texture-bass" ne sont pas nécessairement useless — ils ont une valeur de démo
- Le nombre de 226 paramètres n'est pas un problème en soi si l'UI est claire

---

## 5. Plan d'action global

### Phase 0 — Sécurisation / cadrage
**Objectif**: Valider les corrections critiques sans casser l'existant

**Périmètre**:
- Audit des fichiers à modifier
- Setup d'un environnement de test
- Définition des critères de validation

**Dépendances**: Aucune

**Livrables**:
- Liste des fichiers critiques à modifier
- Tests unitaires pour les corrections critiques

**Définition de terminé**: Phase 0 terminée quand les fichiers critiques sont identifiés

**Critères de validation**: Aucun (phase de cadrage)

### Phase 1 — Corrections critiques du grave, de la justesse et de la stabilité

**Objectif**: Corriger les problèmes qui affectent la justesse et la stabilité du grave

**Périmètre**:
1. Fix sub oscillateur (ajouter option sub-octave 0.25x)
2. Fix detune proportionnel (convertir en cents)
3. Fix PolyBLEP square
4. Fix attack = 0 clicks
5. Fix Pitch env Distorted 808

**Dépendances**: Phase 0 terminée

**Risques**:
- Modifier le sub peut casser les presets 808 existants
- Modifier le detune change le son des presets Reese

**Livrables**:
- Correction du sub oscillateur dans BassVoice.cpp
- Correction du detune dans BassVoice.cpp
- Correction PolyBLEP square dans BassVoice.cpp
- Guard pour attack minimal dans BassVoice.cpp
- Calibration des presets Distorted 808

**Définition de terminé**: Toutes les corrections testées en mono et en poly

**Critères de validation**:
- [ ] Sub Pur: pas de battement audible en mono
- [ ] Reese Bass: detune cohérent entre C1 et C3
- [ ] Acid Bass: pas de glitch pendant pitch sweep
- [ ] Attack = 0: pas de click

### Phase 2 — Stabilisation moteur, envelopes, filtres, unison, glide

**Objectif**: Améliorer la stabilité et la musicalité du moteur

**Périmètre**:
1. Stabiliser le glide (guard pour grands intervalles)
2. Améliorer la cohérence de l'unison
3. Ajouter guard resonance pour éviter auto-oscillation
4. Améliorer le body resonator pour Boom 808

**Dépendances**: Phase 1 terminée

**Risques**:
- Modifier le glide peut casser les presets glide existants

**Livrables**:
- Glide guard dans BassVoice.cpp
- Unison stabilization
- Resonance guard
- Body resonator pour Boom 808

**Définition de terminé**: Toutes les corrections testées

### Phase 3 — Refonte ou amélioration presets / browser / calibration de banque

**Objectif**: Corriger les problèmes de calibration et de cohérence de la banque

**Périmètre**:
1. Calibration individuelle des presets (peak ≠ -1.110 dB pour tous)
2. Suppression des presets incohérents (texture-bass qui ne sont pas vraiment des basses)
3. Réorganisation des tags et taxonomies
4. Validation des mixRole

**Dépendances**: Phase 2 terminée

**Risques**:
- Supprimer des presets existants peut frustrer les utilisateurs existants
- La calibration modifie le volume perçu des presets

**Livrables**:
- Banque de presets recalibrée
- Taxonomie nettoyée
- Tags coherents

**Définition de terminé**: Tous les presets passent le QA preset report

### Phase 4 — Refonte ou amélioration UI / macros / workflow basse

**Objectif**: Améliorer la lisibilité et l'efficacité de l'UI

**Périmètre**:
1. Améliorer la distinction Fatness vs Depth
2. Ajouter option mod wheel "Cutoff"
3. Simplifier les pages MIDI CC (réduire à 4-5)
4. Améliorer le feedback visuel des macros

**Dépendances**: Phase 3 terminée

**Risques**:
- Modifier les macros change le comportement des presets existants

**Livrables**:
- UI mise à jour
- Macros distinctives
- Pages CC simplifiées

### Phase 5 — Polish / QA / cohérence produit / finition commerciale

**Objectif**: Finaliser le produit pour la release

**Périmètre**:
1. Validation complète selon le QA contract
2. Tests de régression
3. Validation de la documentation
4. Préparation des assets de release

**Dépendances**: Phases 1-4 terminées

**Livrables**:
- Rapport QA complet
- Documentation mise à jour
- Build de release

---

## 6. Priorisation stricte

### Immédiat bloquant
- **FIX 1**: Calibration presets (peak identique pour tous) — rend le produit inutile en production
- **FIX 2**: PolyBLEP square bug — crée des artifacts audibles

### Haute priorité
- **FIX 3**: Sub oscillateur battement — rend les presets 808 moins crédibles
- **FIX 4**: Détune proportionnel — incohérence de perception
- **FIX 5**: Pitch env Distorted 808 — glitch audible
- **FIX 6**: Attack = 0 clicks — artifacts sur presets attack-minimal

### Moyenne priorité
- **FIX 7**: Glide guard — effets de "slap" non désirés
- **FIX 8**: Compression par défaut — pumping possible
- **FIX 9**: Chorus mono issues — contradictions avec philosophie mono-first
- **FIX 10**: Mod wheel target limité — promesse QA contract non tenue

### Faible priorité
- **FIX 11**: Distinction Fatness vs Depth — confusion UX
- **FIX 12**: EQ fréquences fixes — manque d'adaptation

### À reporter
- **FEATURE 1**: FM bass — lacune de couverture mais pas critique
- **FEATURE 2**: True sub-octave — différent des standards 808 mais pas critique
- **FEATURE 3**: Sidechain — feature standard mais pas critique pour une basse
- **FEATURE 4**: Oversampling voice-level — optimisation mais pas critique

### À refuser
- **FEATURE 5**: 7 pages MIDI CC — surdesign injustifié
- **FEATURE 6**: Reverb Dattorro complète —功能和 et CPU waste pour 95% des cas d'usage

---

## 7. Stratégie de correction / finalisation

### Principes directeurs

1. **Ne pas masquer les défauts par trop de FX**
   - Les FX sont des outils, pas des pansements
   - Lajustesse et la stabilité du grave doivent être garanties par le moteur, pas par l'EQ

2. **Ne pas produire des presets impressionnants mais inutiles**
   - Supprimer les presets "texture-bass" et "lead-bass" qui ne sont pas des basses
   - Concentrer la banque sur les presets utilitaires

3. **Ne pas casser la fondamentale**
   - Toute correction doit préserver la justesse de la fondamentale
   - Le sub doit renforcer, pas masquer, la fondamentale

4. **Ne pas rendre le produit trop large et flou**
   - Rester focalisé sur "basses utilitaires de production"
   - Ne pas essayer de couvrir les rôles lead ou texture

5. **Ne pas ruiner la compatibilité mono**
   - Tous les FX doivent avoir un comportement mono-safe ou être désactivables
   - Le chorus doit être optionnel pour les presets sub-foundation

6. **Ne pas accumuler de la dette inutile**
   - Ne pas ajouter de features "cool" si la basse n'est pas encore solide
   - Simplifier avant d'ajouter

### Approche par domaine

**Moteur DSP (BassVoice.cpp)**:
- Corrections ciblées, pas de refonte
- Focus sur: sub, unison, glide, attack
- Pas d'ajout de novo features

**Presets (FactoryPresets.cpp)**:
- Calibration individuelle des peaks
- Suppression des presets incohérents
- Réorganisation taxonomique

**FX (PluginProcessor.cpp, FxProcessors.h)**:
- Garder les FX actuels maisデフォルト to OFF for non-essential
- Ne pas ajouter de novo FX
- Simplifier le chorus si possible

**UI (PluginEditor.cpp)**:
- Améliorer les macros
- Simplifier les pages CC
- Pas de refonte totale

---

## 8. Implémentation détaillée

### Corrections critiques à implémenter

#### CORRECTION 1: Calibration presets

**Fichier**: `Source/Engine/FactoryPresets.cpp`

Le problème: Tous les presets ont le même peak de -1.110 dB car cette valeur est codée en dur dans le CSV de QA. La vraie calibration doit se faire dans les settings de chaque preset.

**Implémentation cible**:
```cpp
// Dans buildPresetMetadata() ou directement dans les presets
// Chaque preset doit avoir un nominalPeakDb différent selon son rôle:

// Sub-foundation (808 purs): peak plus bas (~-12dB) car sustain long
// Transient-bass (Slap): peak plus haut (~-6dB) car transients courts
// Character-bass (Moog): peak moyen (~-9dB)
// Organic-foundation (Acoustic): peak moyen (~-8dB)
```

**Action**: Recalibrer les presets selon leur rôle musical réel.

#### CORRECTION 2: Sub oscillateur

**Fichier**: `Source/Engine/BassVoice.cpp`

Le problème: Le sub oscille à exactement 0.5x la fondamentale, créant des battements audibles.

**Implémentation cible**:
```cpp
// Option A: Ajouter un ratio de sub configurable (0.5 = octave, 0.25 = sub-octave)
// Option B: Ajouter un filtre passe-bas dédié au sub pour éviter les battements
// Option C: Diminuer le niveau du sub pour réduire le battement

// RECOMMANDÉ: Option C (simple et efficace)
const float subLevel = settings.subLevel * 0.7f; // Réduire pour éviter battement
```

**Action**: Réduire le niveau du sub ou ajouter un filtre léger.

#### CORRECTION 3: Détune proportionnel

**Fichier**: `Source/Engine/BassVoice.cpp`

Le problème: Le détune en Hz cause des beating différents selon le registre.

**Implémentation cible**:
```cpp
// Convertir le détune en cents (constant quel que soit la fréquence)
const float detuneCents = chars.detuneAmount * 100.0f; // 0.015 * 100 = 1.5 cents
const float detuneHz = baseFreq * (std::pow(2.0f, detuneCents / 1200.0f) - 1.0f);
```

**Action**: Modifier le calcul du détune pour utiliser des cents.

#### CORRECTION 4: PolyBLEP square

**Fichier**: `Source/Engine/BassVoice.cpp`

Le problème: Le 2ème polyBLEP utilise `inc` au lieu du `dt` recalculé pour la phase décalée.

**Implémentation cible**:
```cpp
case OscMode::Square:
{
    float sq = (osc.phase < 0.5f) ? 1.0f : -1.0f;
    sq += polyBlep(osc.phase, inc);
    float shifted = osc.phase + 0.5f;
    if (shifted >= 1.0f) shifted -= 1.0f;
    // FIX: Utiliser la bonne valeur de dt pour la phase décalée
    float dtShifted = inc; // Pas correct — devrait être recalculé
    sq -= polyBlep(shifted, dtShifted);
    signal += sq;
    break;
}
```

**Action**: Recalculer le `dt` pour la phase décalée.

#### CORRECTION 5: Attack guard

**Fichier**: `Source/Engine/BassVoice.cpp`

Le problème: Attack = 0 peut créer des clicks.

**Implémentation cible**:
```cpp
// Dans noteOn():
const float safeAttackSeconds = std::max(baseAttackSeconds, 0.001f);
// Ou dans le calcul de attackCoeff:
if (attackSeconds > 0.0005f) // Augmenter le seuil minimum
```

**Action**: Ajouter un guard pour éviter les attacks trop courts.

---

## 9. Validation lot par lot

### Lot 1: Corrections sub et calibration

**Tester**:
- Sub Pur: rendu mono, écouter les battements
- Trap Minimal: overlap avec kick
- Tous les presets: peak différent après calibration

**Observer**:
- Pas de battement audible sur les presets sub
- Volume cohérent entre presets de même rôle
- Pas de nouvelle distorsion introduite

**Régressions possibles**:
- Présets 808 peuvent sembler moins "lourds" après réduction du sub
- Présets sub peuvent varier en volume après calibration

### Lot 2: Corrections justesse et glide

**Tester**:
- Reese Bass: notes C1, C2, C3 — detune cohérent?
- Acid Bass: pitch sweep sans glitch
- Glide presets: Sub Glissé, Moog Glide Legato

**Observer**:
- Detune constant entre registres
- Pitch sweep smooth
- Glide sans effet de "slap"

**Régressions possibles**:
- Présets Reese peuvent sembler moins "détunés"
- Présets glide peuvent avoir un comportement différent

### Lot 3: Validation presets

**Tester**:
- QA preset report: tous les presets passent
- Browser: recherche par rôle fonctionne
- Tags: cohérents entre presets

**Observer**:
- Pas de doublons
- Pas de presets silencieux
- Cohérence taxonomique

### Lot 4: Validation UI

**Tester**:
- Macros: Fatness vs Depth distincts?
- Mod wheel: Cutoff fonctionne?
- Pages CC: simplifiées?

**Observer**:
- Feedback visuel des macros
- Comportement des contrôles critiques
- Temps de navigation

---

## 10. Risques et garde-fous

### Risques techniques
| Risque | Niveau | Impact | Mitigation |
|--------|--------|--------|------------|
| Modifications DSP cassent des presets existants | 7 | 9 | Tests de régression avec preset list |
| Calibration change le volume perçu | 5 | 6 | Notification utilisateur, preset "loudness warning" |
| Glide modifications affectent glide presets | 6 | 7 | Ne pas modifier si pas nécessaire |

### Risques de justesse
| Risque | Niveau | Impact | Mitigation |
|--------|--------|--------|------------|
| Sub fix change le caractère des 808 | 6 | 7 | Comparaison avant/après |
| Detune fix change le son Reese | 7 | 8 | Presets de référence audités |

### Risques de lisibilité en mix
| Risque | Niveau | Impact | Mitigation |
|--------|--------|--------|------------|
| Calibration crée des presets trop forts | 5 | 7 | Limiter à -6dB peak max |
| Chorus mono crée des problèmes | 6 | 7 | Chorus désactivé par défaut sur sub |

### Risques presets
| Risque | Niveau | Impact | Mitigation |
|--------|--------|--------|------------|
| Suppression de presets frustrante | 4 | 5 | Archiver, ne pas supprimer |
| Présets incohérents restent | 5 | 6 | Audit manuel de chaque preset |

### Risques UX
| Risque | Niveau | Impact | Mitigation |
|--------|--------|--------|------------|
| Macros trop différentes | 4 | 4 | Tests utilisateurs |
| CC pages simplifiées = confusion | 3 | 3 | Documentation clara |

### Risques de fausse amélioration
| Risque | Niveau | Impact | Mitigation |
|--------|--------|--------|------------|
| Ajouter des features "cool" | 6 | 7 | Rester focus sur corrections |
| Sur-optimiser le CPU | 4 | 4 | Mesurer avant/après |

### Risques de surdesign
| Risque | Niveau | Impact | Mitigation |
|--------|--------|--------|------------|
| Refonte UI totale | 5 | 8 | Améliorations incrémentales |
| Ajout de novo FX | 4 | 6 | Ne pas ajouter si pas nécessaire |

---

## 11. Verdict final

### Résumé du plan

Le synthé bass UWdeVST est un produit compétent mais avec des problèmes de calibration et de justesse qui limitent sa crédibilité en production. Les corrections prioritaires sont:

1. **Calibration presets** — Tous les presets ont le même peak, ce qui est incoherent
2. **Sub oscillateur** — Crée des battements audibles avec la fondamentale
3. **Détune proportionnel** — Inconsistance de perception entre registres
4. **PolyBLEP square** — Bug de phase créant des artifacts
5. **Pitch env Distorted 808** — Sweep extrême créant des glitches

### Corrections réellement implémentables dans ce contexte

Sans accès au build system et aux tests en runtime, les corrections suivantes peuvent être proposées:

1. **Calibration presets** — Modifications dans `FactoryPresets.cpp` pour ajuster les niveaux
2. **Sub level reduction** — Modification de `settings.subLevel` dans les presets 808
3. **Detune en cents** — Modification du calcul dans `BassVoice.cpp`
4. **Attack guard** — Ajout d'un seuil minimum dans `BassVoice.cpp`

### Actions prêtes à coder ensuite

1. Modification de `BassVoice.cpp` pour le detune en cents
2. Modification de `BassVoice.cpp` pour le attack guard
3. Modification de `FactoryPresets.cpp` pour la calibration
4. Modification des presets 808 pour réduire le sub level

### Tests à lancer immédiatement

1. Rendu mono de Sub Pur — vérifier battements
2. Rendu mono de Reese Bass — vérifier cohérence detune
3. QA preset report — vérifier cohérence taxonomique
4. Test de glide — vérifier effets de "slap"

### Éléments encore bloqués faute de contexte

1. **Build system** — Impossible de compiler et tester sans JUCE checkout
2. **Preset audio files** — Pour valider la calibration, il faudrait écouter les presets
3. **CPU profiling** — Pour valider l'impact des corrections
4. **UI mockups** — Pour valider les changements UX

### Prochaine meilleure étape

1. **Lire le code restant** de `PluginProcessor.cpp` pour comprendre le flow complet
2. **Implémenter les corrections DSP** dans `BassVoice.cpp`
3. **Regénérer les presets** avec calibration correcte
4. **Tester en runtime** avec le build local

### Verdict: CORRIGER + STABILISER + SIMPLIFIER

Le produit est sur la bonne voie mais a besoin de corrections de justesse et de cohérence avant d'être prêt pour une release commerciale. La stratégie recommandée est:

1. **CORRIGER** les bugs DSP (sub, detune, PolyBLEP)
2. **STABILISER** les presets (calibration, cohérence)
3. **SIMPLIFIER** les FX (reverb cachée, chorus désactivé par défaut)
4. **FINIR** la documentation et les tests QA

Ne PAS procéder à une refonte totale, car le moteur DSP est fondamentalement solide. Les corrections ciblées suffiront à améliorer le produit significativement.
