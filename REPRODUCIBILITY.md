# Reproducibility

## Snapshot Provenance
- Export source workspace: `D:\Dev\Projects\musique\synth`
- Export snapshot date: `2026-04-19`
- Export type: split repository snapshot intended to build from the repository root

## Included Material
- the single top-level JUCE/CMake project directory
- `Shared/`
- `new composants/` when referenced by the exported project
- `qa/`
- a minimal `assets versions png/` subset matching the files referenced by the exported `CMakeLists.txt`

## External Prerequisites
- CMake `3.22+`
- Visual Studio 2022 / MSVC on Windows
- PowerShell
- either:
  - an existing JUCE `8.0.4` checkout passed with `-JuceDir`
  - a local `JUCE/` folder at the repo root
  - or internet access for `.\_build_all.ps1 -BootstrapJuce`

## Build
```powershell
.\_build_all.ps1 -Configuration Release
.\_build_all.ps1 -Configuration Release -RunTests
.\_build_all.ps1 -Configuration Release -BootstrapJuce
```

## Guarantees Of This Export
- the repo root now contains the asset files required by the exported `CMakeLists.txt`
- the build script configures CMake from the repo root and can point CMake at a vendored or external JUCE checkout
- no sibling folders from the original monorepo are required for assets or shared runtime code

## Non-Goals
- build artefacts are not versioned
- JUCE is not vendored by default, to keep repository size bounded
