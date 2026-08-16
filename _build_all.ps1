param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [string]$JuceDir = "",
    [switch]$BootstrapJuce
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSCommandPath
$projectDir = $null

if (Test-Path (Join-Path $repoRoot "CMakeLists.txt")) {
    $projectDir = Get-Item -LiteralPath $repoRoot
} else {
    $projectDir = Get-ChildItem -LiteralPath $repoRoot -Directory |
        Where-Object { Test-Path (Join-Path $_.FullName "CMakeLists.txt") } |
        Sort-Object Name |
        Select-Object -First 1
}

if (-not $projectDir) {
    throw "No project directory with CMakeLists.txt was found under $repoRoot."
}

$projectCMake = Join-Path $projectDir.FullName "CMakeLists.txt"
$projectMatch = Select-String -Path $projectCMake -Pattern '^\s*project\(([^ )]+)' | Select-Object -First 1
if (-not $projectMatch) {
    throw "Unable to detect the main CMake project name from $projectCMake."
}

$pluginTarget = $projectMatch.Matches[0].Groups[1].Value

$assetsDir = Join-Path $repoRoot "assets versions png"
$asciiAssetsDir = Join-Path $repoRoot "assets_versions_png"
if (-not (Test-Path $assetsDir) -and (Test-Path $asciiAssetsDir)) {
    $assetsDir = $asciiAssetsDir
}
if (-not (Test-Path $assetsDir)) {
    throw "Missing asset directory. Expected '$assetsDir' or '$asciiAssetsDir'."
}

$resolvedJuceDir = $null
if ($JuceDir) {
    $resolvedJuceDir = (Resolve-Path $JuceDir).Path
} else {
    $localJuceDir = Join-Path $repoRoot "JUCE"
    if (Test-Path (Join-Path $localJuceDir "CMakeLists.txt")) {
        $resolvedJuceDir = (Resolve-Path $localJuceDir).Path
    }
}

if ($BootstrapJuce -and -not $resolvedJuceDir) {
    $bootstrapTarget = Join-Path $repoRoot "JUCE"
    & git clone --depth 1 --branch 8.0.4 --recurse-submodules https://github.com/juce-framework/JUCE.git $bootstrapTarget
    if ($LASTEXITCODE -ne 0) {
        throw "JUCE bootstrap failed."
    }
    $resolvedJuceDir = (Resolve-Path $bootstrapTarget).Path
}

$buildPath = Join-Path $repoRoot $BuildDir
$cmakeCache = Join-Path $buildPath "CMakeCache.txt"
if (Test-Path $cmakeCache) {
    $cachedSource = Select-String -Path $cmakeCache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=' | Select-Object -First 1
    if ($cachedSource) {
        $cachedSourcePath = $cachedSource.Line.Substring("CMAKE_HOME_DIRECTORY:INTERNAL=".Length)
        $resolvedProjectPath = (Resolve-Path $projectDir.FullName).Path
        if ($cachedSourcePath -ne $resolvedProjectPath) {
            Remove-Item -LiteralPath $buildPath -Recurse -Force
        }
    }
}

$cmakeArgs = @(
    "-S", $projectDir.FullName,
    "-B", $buildPath,
    "-Wno-dev",
    "-DUWDEVST_SHARED_ASSETS_DIR=$assetsDir"
)
if ($resolvedJuceDir) {
    $cmakeArgs += "-DUWDEVST_JUCE_DIR=$resolvedJuceDir"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

$targets = @($pluginTarget)
$standaloneTarget = "${pluginTarget}_Standalone"
$vst3Target = "${pluginTarget}_VST3"

if (Test-Path (Join-Path $buildPath "$standaloneTarget.vcxproj")) {
    $targets += $standaloneTarget
}
if (Test-Path (Join-Path $buildPath "$vst3Target.vcxproj")) {
    $targets += $vst3Target
}
$targets = $targets | Select-Object -Unique

foreach ($target in $targets) {
    & cmake --build $buildPath --config $Configuration --target $target
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for target '$target'."
    }
}

$artifactRoot = Join-Path $buildPath "${pluginTarget}_artefacts\$Configuration"

if ($targets -contains $standaloneTarget) {
    $standaloneDir = Join-Path $artifactRoot "Standalone"
    $standaloneExe = Get-ChildItem $standaloneDir -File -Filter *.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $standaloneExe) {
        throw "Standalone build completed but no executable was found in $standaloneDir."
    }
}

if ($targets -contains $vst3Target) {
    $vst3Dir = Join-Path $artifactRoot "VST3"
    $vst3Bundle = Get-ChildItem $vst3Dir -Directory -Filter *.vst3 -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $vst3Bundle) {
        throw "VST3 build completed but no VST3 bundle was found in $vst3Dir."
    }
}

Write-Host "Build completed: $pluginTarget ($Configuration)"
