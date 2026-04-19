param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [string]$JuceDir = "",
    [switch]$BootstrapJuce,
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSCommandPath
$projectDir = Get-ChildItem -LiteralPath $repoRoot -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName "CMakeLists.txt") } |
    Sort-Object Name |
    Select-Object -First 1

if (-not $projectDir) {
    throw "No top-level project directory with CMakeLists.txt was found under $repoRoot."
}

$projectCMake = Join-Path $projectDir.FullName "CMakeLists.txt"
$projectMatch = Select-String -Path $projectCMake -Pattern '^\s*project\(([^ )]+)' | Select-Object -First 1
if (-not $projectMatch) {
    throw "Unable to detect the main CMake project name from $projectCMake."
}

$pluginTarget = $projectMatch.Matches[0].Groups[1].Value
$consoleTargets = Select-String -Path $projectCMake -Pattern '^\s*juce_add_console_app\(([^ )]+)' |
    ForEach-Object { $_.Matches[0].Groups[1].Value }
$targets = @($pluginTarget) + $consoleTargets | Select-Object -Unique

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
$cmakeArgs = @(
    "-S", $projectDir.FullName,
    "-B", $buildPath,
    "-DUWDEVST_SHARED_ASSETS_DIR=$assetsDir"
)

if ($resolvedJuceDir) {
    $cmakeArgs += "-DUWDEVST_JUCE_DIR=$resolvedJuceDir"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

$standaloneTarget = "${pluginTarget}_Standalone"
$standaloneProjectFile = Join-Path $buildPath "$standaloneTarget.vcxproj"
if (Test-Path $standaloneProjectFile) {
    $targets = @($pluginTarget, $standaloneTarget) + $consoleTargets | Select-Object -Unique
}

foreach ($target in $targets) {
    & cmake --build $buildPath --config $Configuration --target $target
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for target '$target'."
    }
}

if ($targets -contains $standaloneTarget) {
    $standaloneDir = Join-Path $buildPath "${pluginTarget}_artefacts\$Configuration\Standalone"
    $standaloneExe = Get-ChildItem $standaloneDir -File -Filter *.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if (-not $standaloneExe) {
        throw "Standalone build target '$standaloneTarget' completed but no standalone executable was found."
    }
}

if ($RunTests) {
    $testTarget = $targets | Where-Object { $_ -match '(^|_)tests$' } | Select-Object -First 1
    if (-not $testTarget) {
        $testTarget = $targets | Where-Object { $_ -like '*tests*' } | Select-Object -First 1
    }

    if ($testTarget) {
        $testExe = Join-Path $buildPath "$($testTarget)_artefacts\$Configuration\$testTarget.exe"
        if (-not (Test-Path $testExe)) {
            throw "Test executable not found at $testExe."
        }
        & $testExe
        if ($LASTEXITCODE -ne 0) {
            throw "Tests failed for '$testTarget'."
        }
    } else {
        Write-Host "No console test target detected. Skipping test execution."
    }
}
