# Phase 0 - Locked Backlog

This file freezes what the product team is allowed to do next.

## Immediate blockers

1. Fix the misleading `Sub` control behavior.
2. Resolve the 808 LFO bypass mismatch.
3. Remove or repair exposed inert controls:
   - `Pitch Env`
   - `Body`
4. Add bass-specific regression checks:
   - true sub behavior
   - mono safety
   - repeated-note clarity
   - glide landing

## High priority after blockers

1. Add a dedicated filter envelope path for synth-family credibility.
2. Decouple `Brightness` from filter envelope depth.
3. Improve Reese identity without breaking mono.
4. Extend glide range to a musically useful ceiling.
5. Recalibrate preset bank by role and functional loudness.

## Frozen until blockers are closed

- New bass models
- New FX modules
- More presets
- More macro complexity
- Any marketing claim expansion

## Simplify instead of expand

- Keep 9 models.
- Do not widen scope into FM, hybrid or cinematic feature work.
- Prefer hiding or relabeling invalid controls over inventing partial behavior.
- Prefer fewer strong presets over more weak presets.

## Remove if still unsupported after phase 1

- Family-wide exposure of `Pitch Env` if non-808 models remain inactive
- Family-wide exposure of `Body` if non-acoustic models remain inactive
- Any preset naming that implies behavior the engine does not yet deliver

## Protect absolutely

- Fundamental clarity
- Low-end stability
- Mono compatibility
- Rhythmic readability
- Honest browser experience
- Maintainable DSP logic

## Definition of phase 0 done

Phase 0 is complete when the repo contains:
- one truth matrix for exposed controls
- one locked preset reference list for audio validation
- one bass-specific QA contract
- one frozen backlog separating blockers from optional work

Those artefacts are now:
- `synth-bass/PHASE0_CONTROL_TRUTH_MATRIX.md`
- `qa/bass_phase0_reference_presets.csv`
- `qa/bass_phase0_qa_contract.md`
- `synth-bass/PHASE0_BACKLOG.md`
