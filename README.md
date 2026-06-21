# synth-bass

Individually reproducible split repository generated from the main `musique/synth` workspace.

## Layout
- the single top-level project directory contains the JUCE/CMake project
- `Shared/` contains the shared runtime code copied from the main workspace
- `new composants/` contains the shared UI component snapshot used by the export
- `qa/` contains only useful QA scripts in this source tree; current renders live in the repository root `qa/`
- `assets versions png/` contains the minimal asset subset required by this repo

## Build
From the repository root:

```powershell
.\_build_all.ps1 -Configuration Release
```

Use an existing JUCE checkout explicitly:

```powershell
.\_build_all.ps1 -Configuration Release -JuceDir D:\Dev\JUCE
```

Bootstrap JUCE locally inside the repo when needed:

```powershell
.\_build_all.ps1 -Configuration Release -BootstrapJuce
```

Add `-RunTests` to execute the exported console test target after the build.

## Notes
- JUCE is intentionally not committed in this export; `_build_all.ps1` can use an existing checkout or clone `8.0.4` into `JUCE/`.
- The repo carries the asset files referenced by its exported `CMakeLists.txt`, so no sibling monorepo folders are required.
- Current per-instrument preset notes live in the repository root `docs/`.
- Positionnement release: multi-basse synthetique de production / bass sketch / low-end design. Ne pas presenter comme une emulation realiste premium type MODO Bass, Trilian ou banque multisamplee.
- Contrat release: 9 basses, 3 familles, 18 presets factory: 2 par basse (`Reference` et `Signature`).

## QA candidate courante

```powershell
.\_build_all.ps1 -Configuration Release -RunTests
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --export-candidate-presets --output-base build\codex_audit --overwrite
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --render-xml-previews --preset-dir build\codex_audit\bass_candidate_presets --output-base qa\current_candidate_preview_renders_2026-05-31 --overwrite
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --strict-chord-audit --preset-dir build\codex_audit\bass_candidate_presets --output-base qa\current_candidate_chords_2026-05-31 --report qa\current_candidate_strict_chord_2026-05-31.csv --overwrite
.\build\UWdeVST_bass_renderer_artefacts\Release\UWdeVST_bass_renderer.exe --audit-audio --preset-dir build\codex_audit\bass_candidate_presets --output-base qa\current_candidate_audit_audio_2026-05-31 --report qa\current_candidate_audit_audio_2026-05-31.csv --overwrite
```

Le gate d'ecoute humaine reste bloquant avant diffusion externe: aucun verdict `P0-regression` ou `P1-blocker`.
