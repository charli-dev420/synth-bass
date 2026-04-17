# Phase 0 - Bass QA Contract

This contract defines what "better" means for `UWdeVST Bass` before any new feature work.

It is intentionally bass-specific:
- low-end truth before novelty
- perceived pitch before hype
- mono compatibility before width
- arrangement utility before solo impressiveness

## 1. Product acceptance target

The product is accepted only if it behaves like a utility-first bass instrument:
- the fundamental is easy to identify
- the low register stays stable
- presets remain coherent when browsed quickly
- 808 presets do not rely on false sub semantics
- synth presets remain readable in a small mix

## 2. Required listening and render scenarios

Every critical change in phase 1 and phase 2 must be checked against all scenarios below.

### Mandatory scenarios

1. Mono listen:
   - Render each locked reference preset in mono.
   - The tonal center must remain clear.
   - No preset may lose its bass role when collapsed.

2. Simple kick overlap:
   - Use a dry four-on-the-floor kick or simple trap kick.
   - Bass must not bloom unpredictably over the kick tail.
   - 808 tails must remain intentional, not vague.

3. Mini-mix:
   - Bass + kick + snare/clap + closed hats + one chord stab.
   - Bass must keep role without needing rescue EQ outside the instrument.

4. Held notes:
   - Test C1, E1, A1, C2.
   - Listen for low-end drift, false beating, unstable sustain or filter pumping.

5. Repeated notes:
   - 1/8 and 1/16 repeated notes at moderate tempo.
   - Attack should stay readable and level behavior should stay consistent.

6. Glide:
   - Mono and legato tests on `Sub Mono Glide` and one synth preset.
   - Transition must preserve pitch destination and not blur note identity.

7. Simple melodic line:
   - 5-note phrase across low and low-mid register.
   - Bass must stay intelligible note to note, not just as a drone.

8. Grave / medium register split:
   - Compare one octave apart.
   - Product should not only sound good at one sweet spot.

9. Inter-preset browse coherence:
   - Jump between 5 locked reference presets.
   - No large surprise in functional loudness or role mislabeling.

## 3. Failure definitions

Any of the following is a hard failure:
- The exposed control is audible in one family and silently dead in another without UI compensation.
- A preset labeled or perceived as "sub" does not generate a trustworthy low-end bass role.
- A bass sounds louder or fuller only because its tail or FX mask pitch definition.
- Mono collapse removes the intended bass function.
- A repeated-note groove becomes less readable after the change.

## 4. Locked regression protections

The following protections must be added or enforced during phase 1 and phase 2:
- true sub-octave behavior test for 808/sub control
- 808 LFO behavior test or explicit family gating test
- no-inert-control audit for `Pitch Env` and `Body`
- mono render comparison for locked reference presets
- low-register finite-output test after drive/filter/glide changes

## 5. Reference presets

Machine-readable reference list:
- `qa/bass_phase0_reference_presets.csv`

These presets are not "best sounding" by default.
They are locked because they expose product risk:
- honest utility baseline
- long-tail masking risk
- false-sub risk
- glide risk
- resonance/filter articulation risk
- width/mono risk

## 6. Phase gate rules

### No phase 1 completion without
- true sub fix validated on `Sub Pur`
- 808 no-op controls resolved or hidden
- mono listen passed on all 808 reference presets

### No phase 2 completion without
- synth family articulation improved on `Warm Classic` and `Acid Sustain`
- Reese remains usable after mono collapse
- glide test passes in mono and legato modes
