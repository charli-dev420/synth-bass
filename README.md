# synth-bass

Individually reproducible split repository generated from the main `musique/synth` workspace.

## Layout
- the single top-level project directory contains the JUCE/CMake project
- `Shared/` contains the shared runtime code copied from the main workspace
- `new composants/` contains the shared UI component snapshot used by the export
- `qa/` contains the exported QA baselines and release checklists
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
- Export-specific QA and inventory notes live in `qa/` and `UWdeVST_Bass_Knobs.md`.
