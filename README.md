# uwdevst_bass

Free Windows x64 bass synthesizer from the UWdeVST collection.

`uwdevst_bass` is a lightweight synthetic bass instrument designed for production, sketching and low-end sound design. It is not a sampled bass library or a physical emulation of a specific commercial instrument.

## Features

- 9 bass instruments across 3 families
- 18 factory presets (`Reference` and `Signature`)
- Standalone application and VST3 plugin
- Windows x64
- JUCE 8.0.4 / CMake project

## Download

Ready-to-use builds are distributed through the repository **Releases** page. Use the release installer/package instead of downloading build artefacts from the source tree.

## Build from source

Requirements: Windows x64, CMake 3.22+, Visual Studio 2022 with the C++ desktop workload, Git and PowerShell.

```powershell
.\_build_all.ps1 -Configuration Release -BootstrapJuce
```

You can also point the script to an existing JUCE 8.0.4 checkout:

```powershell
.\_build_all.ps1 -Configuration Release -JuceDir C:\Dev\JUCE
```

The public build only contains the product targets required for the Standalone and VST3 versions. Internal renderers, QA tools and production-test targets are intentionally not part of this repository.

## Repository layout

- `Source/` — plugin and synthesis engine
- `Shared/` — shared runtime code required by this standalone repository
- `assets versions png/` — UI assets required by the build
- `new composants/` — shared UI components required by the build

## License

The plugin is free to download and use. The source code is **source-available**, not open source. Local inspection, personal modification and personal builds are permitted under [LICENSE.md](LICENSE.md). Redistribution, repackaging and commercial reuse of the source require prior permission.

JUCE is not included in this repository and remains subject to its own licence terms.

Copyright © 2026 Charli Billabert / unicorn who dev.
