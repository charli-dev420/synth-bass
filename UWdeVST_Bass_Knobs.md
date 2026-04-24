# UWdeVST_Bass Knob Inventory

Ce document decrit l'etat reel des controles exposes par le synth bass, leurs IDs APVTS, et le comportement de rappel des presets.

## Notes

- Les controles instrument utilisent le pattern `bass_<bassIndex>_<suffix>`.
- `bassIndex` va de `0` a `8`.
- Les slots de modulation utilisent le pattern `mod_<slot>_<field>` avec `slot` de `0` a `7` et `field` dans `source`, `dest`, `amount`.
- Le schema de preset bass courant est `format_version = 2`.
- Les presets bass rappellent le son de la basse courante, les FX, les macros/performance, et la mod matrix.
- Les presets bass ne rappellent pas `selected_bass`, `output_gain`, `velocity_curve` ni `fx_lock`.

## Instruments

| Index | Nom | Famille |
| --- | --- | --- |
| 0 | Contrebasse | Acoustic |
| 1 | Basse Fingered | Acoustic |
| 2 | Basse Slap | Acoustic |
| 3 | Sub 808 | 808 |
| 4 | Boom 808 | 808 |
| 5 | Distorted 808 | 808 |
| 6 | Moog Bass | Synth |
| 7 | Reese Bass | Synth |
| 8 | Acid Bass | Synth |

## Disponibilite FX par instrument

| Instrument | Sat | Trans | Comp | EQ | Chorus | Delay | Reverb | Limiter |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 Contrebasse | - | oui | oui | oui | - | oui | oui | oui |
| 1 Basse Fingered | - | oui | oui | oui | oui | oui | oui | oui |
| 2 Basse Slap | oui | oui | oui | oui | - | oui | oui | oui |
| 3 Sub 808 | oui | - | oui | oui | - | - | - | oui |
| 4 Boom 808 | oui | oui | oui | oui | - | - | - | oui |
| 5 Distorted 808 | oui | - | oui | oui | - | - | - | oui |
| 6 Moog Bass | oui | oui | oui | oui | oui | oui | oui | oui |
| 7 Reese Bass | oui | - | oui | oui | oui | oui | oui | oui |
| 8 Acid Bass | oui | oui | oui | oui | oui | oui | oui | oui |

## Labels macro par famille

| Param ID | Acoustic | 808 | Synth |
| --- | --- | --- | --- |
| `macro_fatness` | Boom | Weight | Drive |
| `macro_brillance` | Power | Snap | Edge |
| `macro_punch` | Punch | Punch | Punch |
| `macro_depth` | Profondeur | SubSpace | Depth |

## Panneau 1: Source / Envelope

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Level | `bass_<n>_level` | Niveau de sortie de la basse active. |
| Tune | `bass_<n>_tune` | Transposition en demi-tons. |
| Brightness | `bass_<n>_brightness` | Ouvre ou assombrit le spectre. |
| Attack | `bass_<n>_attack` | Temps d'attaque ADSR. |
| Decay | `bass_<n>_decay` | Temps de descente ADSR. |
| Sustain | `bass_<n>_sustain` | Niveau de maintien ADSR. |
| Release | `bass_<n>_release` | Temps de relachement ADSR. |

## Panneau 2: Tone Shaping

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Body | `bass_<n>_body` | Renforce le corps et la resonance. |
| Drive | `bass_<n>_drive` | Saturation interne du modele. |
| Pitch Env | `bass_<n>_pitch_env` | Quantite d'enveloppe appliquee a la hauteur. |
| Sub | `bass_<n>_sub` | Niveau de la composante sub. |
| Character | `bass_<n>_character` | Couleur specifique au modele. |
| Cutoff | `bass_<n>_cutoff` | Frequence de coupure principale. |
| Pan | `bass_<n>_pan` | Placement stereo. |
| Resonance | `bass_<n>_resonance` | Resonance du filtre principal. |

## Panneau 3: MACRO+LFO

### Performance

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Mono Mode | `mono_mode` | Mode Poly, Mono ou Legato. |
| Glide Time | `glide_time` | Temps de glide global du patch bass. |
| Velocity Curve | `velocity_curve` | Preference de reponse velocity globale, hors preset bass. |
| Bend Range | `pitch_bend_range` | Portee du pitch bend en demi-tons. |

### Macros

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Label variant famille 1 | `macro_fatness` | Masse, poids ou drive global du patch. |
| Label variant famille 2 | `macro_brillance` | Presence, snap ou edge global. |
| Punch | `macro_punch` | Impact et fermete. |
| Label variant famille 4 | `macro_depth` | Profondeur, sub-space ou depth. |

### LFO principal

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Target | `lfo_dest` | Trem/Pan, Cutoff ou Both. |
| Wave | `lfo_wave` | Sine, Triangle, Saw ou Square. |
| Rate | `lfo_rate` | Vitesse du LFO principal. |
| Depth | `lfo_depth` | Profondeur du LFO principal. |
| Mod Wheel Target | `mod_wheel_target` | `Off`, `Punch` ou `Cutoff`. Quand actif, le CC1 agit soit sur `macro_punch`, soit sur le cutoff du bass actif. |

## Panneau 3: MOD MATRIX

### Controles LFO2 de modulation

| UI Label | Param ID | Impact |
| --- | --- | --- |
| LFO2 Rate | `mod_lfo2_rate` | Vitesse du LFO2 utilise par la mod matrix. |
| LFO2 Wave | `mod_lfo2_wave` | Forme d'onde du LFO2 de modulation. |

### Slots de modulation

8 slots de modulation sont exposes.

**Sources:** None, LFO1, LFO2, Envelope, Velocity, Mod Wheel, Aftertouch, Pitch Bend.

**Destinations:** None, Cutoff, Resonance, Pan, Level, Pitch, Attack, Decay, LFO Rate, EQ Mid Freq, EQ Mid Gain.

| Param ID | Role |
| --- | --- |
| `mod_<s>_source` | Source du slot `s`. |
| `mod_<s>_dest` | Destination du slot `s`. |
| `mod_<s>_amount` | Quantite de modulation de `-1.0` a `+1.0`. |

## Panneau 3: FX

### FX lock

| UI Label | Param ID | Impact |
| --- | --- | --- |
| FX LOCK | `fx_lock` | Empeche le rappel des FX lors du chargement d'un preset bass ou d'un factory preset. Ce flag reste global au plugin. |

### Saturator

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Drive | `sat_drive` | Quantite de saturation harmonique. |
| Mix | `sat_mix` | Balance dry/wet. |

### Transient

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Attack | `transient_attack` | Accentue ou reduit l'attaque. |
| Sustain | `transient_sustain` | Accentue ou reduit la tenue. |
| Mix | `transient_mix` | Balance dry/wet. |

### Compressor

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Threshold | `comp_threshold` | Seuil de declenchement. |
| Ratio | `comp_ratio` | Intensite de compression. |
| Attack | `comp_attack` | Vitesse de reaction. |
| Release | `comp_release` | Temps de retour. |
| Makeup | `comp_makeup` | Gain de compensation. |
| Mix | `comp_mix` | Balance dry/wet. |

### EQ

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Low Freq | `eq_low_freq` | Frequence de la bande grave. |
| Low Gain | `eq_low_gain` | Gain de la bande grave. |
| Mid Freq | `eq_mid_freq` | Frequence centrale de la bande medium. |
| Mid Gain | `eq_mid_gain` | Gain de la bande medium. |
| Mid Q | `eq_mid_q` | Facteur Q medium. |
| High Freq | `eq_high_freq` | Frequence de la bande aigue. |
| High Gain | `eq_high_gain` | Gain de la bande aigue. |

### Chorus

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Rate | `chorus_rate` | Vitesse de modulation. |
| Depth | `chorus_depth` | Profondeur de modulation. |
| Mix | `chorus_mix` | Balance dry/wet. |

### Delay

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Time | `delay_time` | Temps de delay libre. |
| Feedback | `delay_feedback` | Quantite de repetitions. |
| Mix | `delay_mix` | Balance dry/wet. |
| Sync Host | `delay_sync` | Active la synchro tempo hote. |
| Division | `delay_division` | Choix rythmique quand la synchro est active. |

### Reverb

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Size | `reverb_size` | Taille virtuelle. |
| Damping | `reverb_damping` | Absorption des aigus. |
| Width | `reverb_width` | Largeur stereo. |
| Mix | `reverb_mix` | Balance dry/wet. |

### Limiter

| UI Label | Param ID | Impact |
| --- | --- | --- |
| Threshold | `limiter_threshold` | Seuil de limitation. |
| Release | `limiter_release` | Temps de relachement. |

## Toggles de bypass FX

| FX UI Slot | Toggle Param ID | Impact |
| --- | --- | --- |
| SAT | `fx_tab0_en` | Active ou bypass le saturator. |
| TRANS | `fx_tab1_en` | Active ou bypass le transient shaper. |
| COMP | `fx_tab2_en` | Active ou bypass le compresseur. |
| EQ | `fx_tab3_en` | Active ou bypass l'EQ. |
| CHORUS | `fx_tab4_en` | Active ou bypass le chorus. |
| DELAY | `fx_tab5_en` | Active ou bypass le delay. |
| REVERB | `fx_tab6_en` | Active ou bypass la reverb. |
| LIMITER | `fx_tab7_en` | Active ou bypass le limiteur. |

## Controles complementaires

| UI Control | Param ID | Role |
| --- | --- | --- |
| Bass selector | `selected_bass` | Choisit la basse active pour les params `bass_<n>_*` et la banque factory. |
| Output Gain | `output_gain` | Gain master global du plugin. |
| Output routing | `bass_<n>_output` | Assigne la basse active a une sortie plugin specifique. |

## Scope des presets bass

### Rappeles par preset user ou factory override bass

- `bass_<n>_level`, `tune`, `brightness`, `attack`, `decay`, `sustain`, `release`, `body`, `drive`, `pitch_env`, `sub`, `character`, `cutoff`, `pan`, `resonance`, `output`
- `glide_time`
- `mono_mode`
- `lfo_rate`, `lfo_depth`, `lfo_wave`, `lfo_dest`
- `macro_fatness`, `macro_brillance`, `macro_punch`, `macro_depth`
- `mod_wheel_target`
- `pitch_bend_range`
- tous les parametres FX, y compris `delay_sync`, `delay_division` et `fx_tab0_en` a `fx_tab7_en`
- `mod_lfo2_rate`, `mod_lfo2_wave`
- `mod_<s>_source`, `mod_<s>_dest`, `mod_<s>_amount`

### Restent globaux au plugin

- `selected_bass`
- `output_gain`
- `velocity_curve`
- `fx_lock`

### Compatibilite legacy

- Le plugin continue a importer l'ancien noeud XML `ModMatrix`.
- Lors du restore d'etat ou du chargement d'un preset legacy, les donnees legacy sont migrees vers les nouveaux parametres APVTS de la mod matrix.

