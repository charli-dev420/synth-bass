# INVENTAIRE EXHAUSTIF DES PARAMETRES - SYNTH BASS (MBS)

> Plugin: **UWdeVST Bass** | Namespace: `mbs::` | 9 instruments | 3 familles | **226 parametres APVTS**

---

## 1. INSTRUMENTS

### Fonction de generation d'ID

```cpp
juce::String makeBassParamId(int bassIndex, const juce::String& suffix)
// "bass_" + bassIndex + "_" + suffix
```

### Familles et instruments

| Index | Nom | Short | Famille | Idx Famille |
|-------|-----|-------|---------|-------------|
| 0 | Contrebasse | CTRBS | Acoustique | 0 |
| 1 | Basse Fingered | FINGR | Acoustique | 0 |
| 2 | Basse Slap | SLAP | Acoustique | 0 |
| 3 | Sub 808 | SUB | 808 | 1 |
| 4 | Boom 808 | BOOM | 808 | 1 |
| 5 | Distorted 808 | DIST | 808 | 1 |
| 6 | Moog Bass | MOOG | Synthe | 2 |
| 7 | Reese Bass | REESE | Synthe | 2 |
| 8 | Acid Bass | ACID | Synthe | 2 |

---

## 2. PARAMETRES GLOBAUX HOTE

### 2.1 Master / Selection / Performance

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step |
|---|-------------|-------------|------|-----|-----|---------|------|
| 1 | `output_gain` | Output Gain | Float | -24.0 dB | 12.0 dB | -3.0 | 0.01 |
| 2 | `selected_bass` | Selected Bass | Choice | 0 | 8 | 0 (Contrebasse) | - |
| 3 | `lfo_rate` | LFO Rate | Float | 0.05 Hz | 12.0 Hz | 1.2 | 0.0001 |
| 4 | `lfo_depth` | LFO Depth | Float | 0.0 | 1.0 | 0.0 | 0.0001 |
| 5 | `lfo_wave` | LFO Wave | Choice | - | - | 0 (Sine) | - |
|   | | | | Choix: Sine, Triangle, Saw, Square | | | |
| 6 | `lfo_dest` | LFO Dest | Choice | - | - | 0 (Trem/Pan) | - |
|   | | | | Choix: Trem/Pan, Cutoff, Both | | | |
| 7 | `mono_mode` | Mono Mode | Choice | - | - | 0 (Poly) | - |
|   | | | | Choix: Poly, Mono, Legato | | | |
| 8 | `glide_time` | Glide Time | Float | 0.0 s | 0.5 s | 0.0 | 0.001 |
| 9 | `macro_fatness` | Macro Fatness | Float | 0.0 | 1.0 | 0.5 | 0.0001 |
| 10 | `macro_brillance` | Macro Brillance | Float | 0.0 | 1.0 | 0.5 | 0.0001 |
| 11 | `macro_punch` | Macro Punch | Float | 0.0 | 1.0 | 0.5 | 0.0001 |
| 12 | `macro_depth` | Macro Depth | Float | 0.0 | 1.0 | 0.3 | 0.0001 |
| 13 | `mod_wheel_target` | Mod Wheel Target | Choice | - | - | 1 (Punch) | - |
|   | | | | Choix: Off, Punch | | | |
| 14 | `velocity_curve` | Velocity Curve | Int | 0 | 6 | 0 | 1 |
| 15 | `pitch_bend_range` | Pitch Bend Range | Float | 1.0 st | 24.0 st | 2.0 | 1.0 |
| 16 | `mod_lfo2_rate` | Mod LFO2 Rate | Float | 0.05 Hz | 12.0 Hz | 2.0 | 0.0001 |
| 17 | `mod_lfo2_wave` | Mod LFO2 Wave | Choice | - | - | 0 (Sine) | - |
|   | | | | Choix: Sine, Triangle, Saw, Square | | | |
| 18 | `fx_lock` | FX Lock | Bool | false | true | false | - |

### 2.2 Toggles Enable FX

| # | Parameter ID | Nom Affiche | Type | Default | Tab FX |
|---|-------------|-------------|------|---------|--------|
| 19 | `fx_tab0_en` | Saturator Enable | Bool | true | 0 - SAT |
| 20 | `fx_tab1_en` | Transient Enable | Bool | true | 1 - TRANS |
| 21 | `fx_tab2_en` | Compressor Enable | Bool | true | 2 - COMP |
| 22 | `fx_tab3_en` | EQ Enable | Bool | true | 3 - EQ |
| 23 | `fx_tab4_en` | Chorus Enable | Bool | true | 4 - CHORUS |
| 24 | `fx_tab5_en` | Delay Enable | Bool | true | 5 - DELAY |
| 25 | `fx_tab6_en` | Reverb Enable | Bool | true | 6 - REVERB |
| 26 | `fx_tab7_en` | Limiter Enable | Bool | true | 7 - LIMITER |

### 2.3 Saturator

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step |
|---|-------------|-------------|------|-----|-----|---------|------|
| 27 | `sat_drive` | Sat Drive | Float | 1.0 | 16.0 | 1.8 | 0.01 |
| 28 | `sat_mix` | Sat Mix | Float | 0.0 | 1.0 | 0.15 | 0.0001 |

### 2.4 Transient Shaper

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step |
|---|-------------|-------------|------|-----|-----|---------|------|
| 29 | `transient_attack` | Transient Attack | Float | -1.0 | 1.0 | 0.10 | 0.0001 |
| 30 | `transient_sustain` | Transient Sustain | Float | -1.0 | 1.0 | 0.0 | 0.0001 |
| 31 | `transient_mix` | Transient Mix | Float | 0.0 | 1.0 | 0.4 | 0.0001 |

### 2.5 Compresseur

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step |
|---|-------------|-------------|------|-----|-----|---------|------|
| 32 | `comp_threshold` | Comp Threshold | Float | -60.0 dB | 0.0 dB | -19.0 | 0.01 |
| 33 | `comp_ratio` | Comp Ratio | Float | 1.0 | 20.0 | 3.0 | 0.01 |
| 34 | `comp_attack` | Comp Attack | Float | 0.1 ms | 100.0 ms | 10.0 | 0.01 |
| 35 | `comp_release` | Comp Release | Float | 5.0 ms | 500.0 ms | 120.0 | 0.01 |
| 36 | `comp_makeup` | Comp Makeup | Float | 0.0 dB | 24.0 dB | 0.0 | 0.01 |
| 37 | `comp_mix` | Comp Mix | Float | 0.0 | 1.0 | 1.0 | 0.0001 |

### 2.6 EQ Parametrique 3 bandes

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step | Skew |
|---|-------------|-------------|------|-----|-----|---------|------|------|
| 38 | `eq_low_freq` | EQ Low Freq | Float | 40.0 Hz | 800.0 Hz | 80.0 | 0.01 | 0.4 |
| 39 | `eq_low_gain` | EQ Low Gain | Float | -12.0 dB | 12.0 dB | 0.0 | 0.01 | - |
| 40 | `eq_mid_freq` | EQ Mid Freq | Float | 200.0 Hz | 8000.0 Hz | 600.0 | 0.01 | 0.4 |
| 41 | `eq_mid_gain` | EQ Mid Gain | Float | -12.0 dB | 12.0 dB | 0.0 | 0.01 | - |
| 42 | `eq_mid_q` | EQ Mid Q | Float | 0.1 | 10.0 | 1.0 | 0.01 | 0.5 |
| 43 | `eq_high_freq` | EQ High Freq | Float | 1000.0 Hz | 16000.0 Hz | 3500.0 | 0.01 | 0.4 |
| 44 | `eq_high_gain` | EQ High Gain | Float | -12.0 dB | 12.0 dB | 0.0 | 0.01 | - |

### 2.7 Chorus

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step |
|---|-------------|-------------|------|-----|-----|---------|------|
| 45 | `chorus_rate` | Chorus Rate | Float | 0.1 Hz | 8.0 Hz | 0.8 | 0.01 |
| 46 | `chorus_depth` | Chorus Depth | Float | 0.0 | 1.0 | 0.4 | 0.0001 |
| 47 | `chorus_mix` | Chorus Mix | Float | 0.0 | 1.0 | 0.0 | 0.0001 |

### 2.8 Delay

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step | Skew |
|---|-------------|-------------|------|-----|-----|---------|------|------|
| 48 | `delay_time` | Delay Time | Float | 10.0 ms | 1500.0 ms | 300.0 | 0.01 | 0.4 |
| 49 | `delay_feedback` | Delay Feedback | Float | 0.0 | 0.95 | 0.25 | 0.0001 | - |
| 50 | `delay_mix` | Delay Mix | Float | 0.0 | 1.0 | 0.0 | 0.0001 | - |
| 51 | `delay_sync` | Delay Sync | Bool | false | true | false | - | - |
| 52 | `delay_note_div` | Delay Note Div | Choice | - | - | 0 (1/4) | - | - |
|    | | | | Choix: 1/4, 1/8, 1/16, 1/8 Dot, 1/8 Trip | | | | |

### 2.9 Reverb

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step |
|---|-------------|-------------|------|-----|-----|---------|------|
| 53 | `reverb_size` | Reverb Size | Float | 0.0 | 1.0 | 0.40 | 0.0001 |
| 54 | `reverb_damping` | Reverb Damping | Float | 0.0 | 1.0 | 0.55 | 0.0001 |
| 55 | `reverb_width` | Reverb Width | Float | 0.0 | 1.0 | 0.60 | 0.0001 |
| 56 | `reverb_mix` | Reverb Mix | Float | 0.0 | 1.0 | 0.0 | 0.0001 |

### 2.10 Limiter

| # | Parameter ID | Nom Affiche | Type | Min | Max | Default | Step |
|---|-------------|-------------|------|-----|-----|---------|------|
| 57 | `limiter_threshold` | Limiter Threshold | Float | -12.0 dB | 0.0 dB | -0.3 | 0.01 |
| 58 | `limiter_release` | Limiter Release | Float | 1.0 ms | 200.0 ms | 50.0 | 0.01 |

---

## 3. MOD MATRIX HOTE

### Schema de nommage

```cpp
mod_<slot>_source
mod_<slot>_dest
mod_<slot>_amount
```

### Details

- Nombre de slots: 8
- Sources: None, LFO1, LFO2, Envelope, Velocity, Mod Wheel, Aftertouch, Pitch Bend
- Destinations: None, Cutoff, Resonance, Pan, Level, Pitch, Attack, Decay, LFO Rate, EQ Mid Freq, EQ Mid Gain

### Parametres par slot

| Champ | Type | Range / Choix | Default |
|------|------|----------------|---------|
| `mod_<slot>_source` | Choice | 0..7 | 0 (None) |
| `mod_<slot>_dest` | Choice | 0..10 | 0 (None) |
| `mod_<slot>_amount` | Float | -1.0..1.0 | 0.0 |

### Total

- 8 slots x 3 params = **24 parametres**

---

## 4. PARAMETRES PAR INSTRUMENT

### Schema de nommage

`bass_{INDEX}_{SUFFIX}`

### Catalogue

| # | Suffix | Nom Affiche | Type | Min | Max | Step | Skew | Description |
|---|--------|-------------|------|-----|-----|------|------|-------------|
| 1 | `level` | {Nom} Level | Float | 0.0 | 1.0 | 0.0001 | - | Volume |
| 2 | `tune` | {Nom} Tune | Float | -24.0 | 24.0 semi | 0.01 | - | Accordage |
| 3 | `brightness` | {Nom} Brightness | Float | 0.0 | 1.0 | 0.0001 | - | Contenu harmonique |
| 4 | `attack` | {Nom} Attack | Float | 0.0 s | 2.0 s | 0.0001 | - | Attaque ADSR |
| 5 | `decay` | {Nom} Decay | Float | 0.1 s | 10.0 s | 0.001 | - | Declin ADSR |
| 6 | `sustain` | {Nom} Sustain | Float | 0.0 | 1.0 | 0.0001 | - | Maintien ADSR |
| 7 | `release` | {Nom} Release | Float | 0.01 s | 5.0 s | 0.0001 | - | Relachement ADSR |
| 8 | `body` | {Nom} Body | Float | 0.0 | 1.0 | 0.0001 | - | Resonance corps/comb |
| 9 | `drive` | {Nom} Drive | Float | 0.0 | 1.0 | 0.0001 | - | Saturation/distorsion |
| 10 | `pitch_env` | {Nom} Pitch Env | Float | 0.0 | 1.0 | 0.0001 | - | Intensite env. pitch |
| 11 | `sub` | {Nom} Sub | Float | 0.0 | 1.0 | 0.0001 | - | Niveau sous-harmonique |
| 12 | `character` | {Nom} Character | Float | 0.0 | 1.0 | 0.0001 | - | Caractere specifique |
| 13 | `cutoff` | {Nom} Cutoff | Float | 120.0 Hz | 12000.0 Hz | 0.0 | 0.28 | Filtre passe-bas |
| 14 | `pan` | {Nom} Pan | Float | -1.0 | 1.0 | 0.001 | - | Panoramique |
| 15 | `resonance` | {Nom} Resonance | Float | 0.0 | 1.0 | 0.0001 | - | Resonance filtre |
| 16 | `output` | {Nom} Output | Choice | - | - | 0 | - | Bus de sortie |

### Total

- 16 params par instrument x 9 instruments = **144 parametres**

---

## 5. VALEURS PAR DEFAUT PAR INSTRUMENT

| Instrument | level | tune | bright | attack | decay | sustain | release | body | drive | pitch_env | sub | character | cutoff | pan | resonance |
|------------|-------|------|--------|--------|-------|---------|---------|------|-------|-----------|-----|-----------|--------|-----|-----------|
| Contrebasse | 0.82 | 0.0 | 0.45 | 0.005 | 3.0 | 0.22 | 0.35 | 0.65 | 0.0 | 0.0 | 0.40 | 0.50 | 3000 | 0.0 | 0.20 |
| Basse Fingered | 0.84 | 0.0 | 0.50 | 0.004 | 2.5 | 0.25 | 0.25 | 0.45 | 0.0 | 0.0 | 0.50 | 0.45 | 3500 | 0.0 | 0.22 |
| Basse Slap | 0.85 | 0.0 | 0.65 | 0.001 | 1.5 | 0.15 | 0.15 | 0.35 | 0.10 | 0.0 | 0.45 | 0.60 | 5000 | 0.0 | 0.25 |
| Sub 808 | 0.90 | 0.0 | 0.20 | 0.002 | 5.0 | 0.35 | 0.30 | 0.0 | 0.0 | 0.80 | 0.70 | 0.50 | 800 | 0.0 | 0.15 |
| Boom 808 | 0.88 | 0.0 | 0.30 | 0.003 | 6.0 | 0.40 | 0.35 | 0.0 | 0.15 | 0.65 | 0.75 | 0.55 | 1200 | 0.0 | 0.18 |
| Distorted 808 | 0.85 | 0.0 | 0.35 | 0.001 | 3.0 | 0.20 | 0.20 | 0.0 | 0.55 | 0.90 | 0.60 | 0.65 | 1500 | 0.0 | 0.30 |
| Moog Bass | 0.82 | 0.0 | 0.55 | 0.003 | 2.5 | 0.30 | 0.25 | 0.0 | 0.20 | 0.0 | 0.35 | 0.50 | 2000 | 0.0 | 0.45 |
| Reese Bass | 0.80 | 0.0 | 0.40 | 0.005 | 4.0 | 0.35 | 0.50 | 0.0 | 0.10 | 0.0 | 0.50 | 0.60 | 1200 | 0.0 | 0.25 |
| Acid Bass | 0.84 | 0.0 | 0.60 | 0.001 | 1.5 | 0.20 | 0.12 | 0.0 | 0.15 | 0.0 | 0.30 | 0.55 | 2500 | 0.0 | 0.55 |

---

## 6. DISPONIBILITE FX PAR INSTRUMENT

| Instrument | Sat | Trans | Comp | EQ | Chorus | Delay | Reverb | Limiter |
|------------|-----|-------|------|-----|--------|-------|--------|---------|
| Contrebasse | - | OUI | OUI | OUI | - | OUI | OUI | OUI |
| Basse Fingered | - | OUI | OUI | OUI | OUI | OUI | OUI | OUI |
| Basse Slap | OUI | OUI | OUI | OUI | - | OUI | OUI | OUI |
| Sub 808 | OUI | - | OUI | OUI | - | - | - | OUI |
| Boom 808 | OUI | OUI | OUI | OUI | - | - | - | OUI |
| Distorted 808 | OUI | - | OUI | OUI | - | - | - | OUI |
| Moog Bass | OUI | OUI | OUI | OUI | OUI | OUI | OUI | OUI |
| Reese Bass | OUI | - | OUI | OUI | OUI | OUI | OUI | OUI |
| Acid Bass | OUI | OUI | OUI | OUI | OUI | OUI | OUI | OUI |

---

## 7. COMPORTEMENT DES PRESETS BASS

### Schema

- Format actuel: `format_version = 2`
- Racines XML:
  - preset user: `BassPreset`
  - factory override: `FactoryPreset`
- La mod matrix continue a etre exportee avec un noeud XML `ModMatrix` pour compatibilite legacy.

### Rappeles par preset bass

- tous les params `bass_<n>_*`, y compris `output`
- `glide_time`
- `mono_mode`
- `lfo_rate`, `lfo_depth`, `lfo_wave`, `lfo_dest`
- `macro_fatness`, `macro_brillance`, `macro_punch`, `macro_depth`
- `mod_wheel_target`
- `pitch_bend_range`
- `mod_lfo2_rate`, `mod_lfo2_wave`
- tous les params FX, y compris `delay_sync`, `delay_note_div`
- `fx_tab0_en` a `fx_tab7_en`
- `mod_<slot>_source`, `mod_<slot>_dest`, `mod_<slot>_amount`

### Restent globaux au plugin

- `selected_bass`
- `output_gain`
- `velocity_curve`
- `fx_lock`

### Compatibilite legacy

- Lors du restore d'etat plugin, l'ancien noeud `ModMatrix` est migre vers les nouveaux params APVTS.
- Les presets ou etats legacy sans nouveaux params restent chargeables avec sanitation.

---

## 8. MAPPING MIDI CC (FLkey Mini - 7 pages)

### Navigation

- CC `102`: page precedente
- CC `103`: page suivante
- CC `44` a `50`: selection directe des pages `0` a `6`
- CC `21` a `28`: 8 knobs de la page courante
- CC `1`: si `mod_wheel_target != Off`, le mod wheel pilote `macro_punch`

### Pages

| Page | Nom | Knob 1 | Knob 2 | Knob 3 | Knob 4 | Knob 5 | Knob 6 | Knob 7 | Knob 8 |
|------|-----|--------|--------|--------|--------|--------|--------|--------|--------|
| 0 | MACROS | `macro_fatness` via alias `macro_grosseur` | `macro_brillance` | `macro_punch` | `macro_depth` via alias `macro_profondeur` | `lfo_rate` | `lfo_depth` | `reverb_mix` | `output_gain` |
| 1 | ENVELOPE | `attack` | `decay` | `sustain` | `release` | `level` | `tune` | `pitch_env` | `sub` |
| 2 | TONE | `brightness` | `body` | `drive` | `character` | `cutoff` | `resonance` | `pan` | `output_gain` |
| 3 | COMP/SAT | `comp_threshold` | `comp_ratio` | `comp_attack` | `comp_release` | `comp_makeup` | `comp_mix` | `sat_drive` | `sat_mix` |
| 4 | EQ | `eq_low_freq` | `eq_low_gain` | `eq_mid_freq` | `eq_mid_gain` | `eq_mid_q` | `eq_high_freq` | `eq_high_gain` | `output_gain` |
| 5 | CHORUS/TRANS | `chorus_rate` | `chorus_depth` | `chorus_mix` | `transient_attack` | `transient_sustain` | `transient_mix` | `limiter_threshold` | `limiter_release` |
| 6 | DELAY/REVERB | `delay_time` | `delay_feedback` | `delay_mix` | `reverb_size` | `reverb_damping` | `reverb_width` | `reverb_mix` | `output_gain` |

---

## 9. RESUME

| Categorie | Nombre |
|-----------|--------|
| Parametres globaux hote | 18 |
| Parametres FX globaux | 40 |
| Parametres mod matrix | 24 |
| Parametres par instrument | 144 |
| **TOTAL PARAMETRES APVTS** | **226** |
| Instruments | 9 |
| Familles | 3 |
| Pages MIDI CC | 7 |
| Slots FX | 8 |
| Slots Modulation | 8 |

