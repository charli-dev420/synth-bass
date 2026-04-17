# Phase 0 - Control Truth Matrix

This file locks the current truth of the exposed bass controls before any DSP or preset correction work.

Scope:
- Product: `UWdeVST Bass`
- Families: `Acoustic`, `808`, `Synth`
- Code anchors:
  - `Source/Engine/BassDefs.cpp`
  - `Source/Engine/BassVoice.cpp`
  - `Source/PluginProcessor.cpp`
  - `Source/PluginEditor.cpp`

Status legend:
- `ACTIVE`: exposed control has a clear audible effect in the current family
- `LIMITED`: audible effect exists, but behavior is narrower than the UI implies
- `INERT`: control is exposed but has no meaningful audio effect in the current family
- `MISLEADING`: control is exposed, does something materially different from its label or product expectation

## Global product decision

This instrument is now framed as a utility-first bass synth:
- clear tonal center
- stable low end
- mono-safe defaults
- honest controls
- presets sorted by mix function, not demo value

The phase 1 and phase 2 backlog must use this matrix as source of truth.

## Per-control truth matrix

| Control | Acoustic | 808 | Synth | Current truth |
|---|---|---|---|---|
| `Level` | ACTIVE | ACTIVE | ACTIVE | Clean amplitude control. |
| `Tune` | ACTIVE | ACTIVE | ACTIVE | Standard semitone transpose. |
| `Brightness` | ACTIVE | ACTIVE | ACTIVE | Also drives filter envelope depth, so it is broader than a pure timbre control. |
| `Attack` | ACTIVE | ACTIVE | ACTIVE | ADSR attack is active in all families. |
| `Decay` | ACTIVE | ACTIVE | ACTIVE | ADSR decay is active in all families. |
| `Sustain` | ACTIVE | ACTIVE | ACTIVE | ADSR sustain is active in all families. |
| `Release` | ACTIVE | ACTIVE | ACTIVE | ADSR release is active in all families. |
| `Body` | ACTIVE | INERT | INERT | Body resonator is initialized only when `bodyDelayRatio > 0`, which is acoustic-only in current definitions. |
| `Drive` | ACTIVE | ACTIVE | ACTIVE | Internal saturation is active in all families. |
| `Pitch Env` | INERT | ACTIVE | INERT | Only 808 models define non-zero `pitchEnvSemitones`. |
| `Sub` | MISLEADING | MISLEADING | MISLEADING | Sub oscillator runs at fundamental, not at a sub octave. |
| `Character` | ACTIVE | LIMITED | ACTIVE | On 808 it mostly behaves like extra saturation, not a strong model-defining control. |
| `Cutoff` | ACTIVE | ACTIVE | ACTIVE | LP filter cutoff is active in all families. |
| `Pan` | ACTIVE | ACTIVE | ACTIVE | Voice pan is active in all families. |
| `Resonance` | ACTIVE | ACTIVE | ACTIVE | Filter resonance is active in all families. |
| `Mono Mode` | ACTIVE | ACTIVE | ACTIVE | Poly/Mono/Legato are active globally. |
| `Glide Time` | LIMITED | LIMITED | LIMITED | Works globally, but is capped to `0.5 s`, which is too short for some valid bass use cases. |
| `LFO Rate` | ACTIVE | INERT | ACTIVE | Entire global LFO path is bypassed for 808 family. |
| `LFO Depth` | ACTIVE | INERT | ACTIVE | Entire global LFO path is bypassed for 808 family. |
| `LFO Wave` | ACTIVE | INERT | ACTIVE | Entire global LFO path is bypassed for 808 family. |
| `LFO Dest` | ACTIVE | INERT | ACTIVE | Entire global LFO path is bypassed for 808 family. |
| `Macro Fatness` | ACTIVE | LIMITED | ACTIVE | On 808 it modifies controls that include inert or misleading paths. |
| `Macro Brillance` | ACTIVE | LIMITED | ACTIVE | On 808 it remains audible but narrower than implied. |
| `Macro Punch` | ACTIVE | ACTIVE | ACTIVE | Mostly honest, but depends on family-specific control validity. |
| `Macro Depth` | ACTIVE | LIMITED | ACTIVE | On 808 it partly depends on the misleading sub path. |

## Family-specific product truth

### Acoustic

What is currently honest:
- additive harmonic body
- pluck transient
- body resonator
- stable mono bass foundation

What is currently dishonest or weak:
- `Pitch Env` is visible but inert
- `Sub` is labeled as sub reinforcement but is not a real octave-below component

### 808

What is currently honest:
- pitch drop behavior
- clean low-passed sine/additive low-end family
- mono-stable central image

What is currently dishonest or weak:
- `Sub` is not a true sub-octave
- full LFO panel is visible while audio path is bypassed
- `Body` is visible while inert
- `Character` is weaker than implied

### Synth

What is currently honest:
- cutoff, resonance, drive and core oscillator tone all work
- mono/legato workflow is valid

What is currently dishonest or weak:
- `Pitch Env` is visible while inert
- `Body` is visible while inert
- Reese identity depends too much on `Character`
- there is no dedicated filter envelope despite Moog/Acid naming

## Locked product rules

The next implementation phases must follow these rules:

1. No exposed control may remain `INERT` or `MISLEADING`.
2. Any control that cannot be made musically valid must be hidden or relabeled by family.
3. Low-end corrections take precedence over adding range, flavor or FX.
4. Preset names must not imply capabilities the engine does not currently deliver.
5. A bass control is considered valid only if it survives:
   - mono listen
   - repeated-note groove
   - simple kick overlap
   - low-register held-note test

## Phase 1 handoff checklist

- Fix the `Sub` control semantics.
- Remove 808 LFO no-op behavior or hide unsupported destinations.
- Resolve exposed inert controls for `Pitch Env` and `Body`.
- Add tests that fail when low-end truth is broken again.
