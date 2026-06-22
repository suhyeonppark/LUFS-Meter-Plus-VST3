# LUFS Meter Plus - Build and Package Script
# This script builds the VST3 plugin and creates an installer

param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    
    [switch]$NoInstaller = $false
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build"
$InstallerScript = Join-Path $ProjectRoot "installer" "installer.iss"
$InstallerDir = Join-Path $ProjectRoot "installer"

Write-Host "╔════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  LUFS Meter Plus - Build & Package Tool   ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check for required tools
Write-Host "Checking prerequisites..." -ForegroundColor Yellow

# Check cmake
try {
    $cmake = Get-Command cmake -ErrorAction Stop
    Write-Host "✓ CMake found: $($cmake.Source)" -ForegroundColor Green
} catch {
    Write-Host "✗ CMake not found. Please install CMake." -ForegroundColor Red
    exit 1
}

# Check JUCE
$JucePath = Resolve-Path "$ProjectRoot\..\JUCE" -ErrorAction SilentlyContinue
if (-not $JucePath) {
    Write-Host "⚠ JUCE not found at ../JUCE. Checking alternative paths..." -ForegroundColor Yellow
    $JucePath = Resolve-Path "C:\Users\$env:USERNAME\OneDrive\개인개발\JUCE" -ErrorAction SilentlyContinue
}

if ($JucePath) {
    Write-Host "✓ JUCE found: $JucePath" -ForegroundColor Green
} else {
    Write-Host "✗ JUCE not found. Set JUCE_PATH or place JUCE in ../JUCE" -ForegroundColor Red
    Write-Host "  You can set it manually: `$env:JUCE_PATH = 'C:\path\to\JUCE'" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Yellow
Write-Host "Configuration: $Configuration" -ForegroundColor Cyan

# Configure with CMake
if ($JucePath) {
    $JucePathStr = $JucePath.Path
    Write-Host "Running: cmake --preset vs2022 -DJUCE_PATH=`"$JucePathStr`"" -ForegroundColor Gray
    & cmake --preset vs2022 -DJUCE_PATH="$JucePathStr"
} else {
    Write-Host "Running: cmake --preset vs2022" -ForegroundColor Gray
    & cmake --preset vs2022
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ CMake configure failed!" -ForegroundColor Red
    exit 1
}

# Build
$BuildPreset = if ($Configuration -eq "Release") { "win-release" } else { "win-debug" }
Write-Host "Running: cmake --build --preset $BuildPreset" -ForegroundColor Gray
& cmake --build --preset $BuildPreset

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Build completed successfully!" -ForegroundColor Green
Write-Host ""

# List build artifacts
$ArtifactsDir = Join-Path $BuildDir "vs2022\LufsMeterPlus_artefacts\$Configuration"
if (Test-Path $ArtifactsDir) {
    Write-Host "Build artifacts:" -ForegroundColor Cyan
    Get-ChildItem $ArtifactsDir -Recurse -Include "*.vst3", "*.exe" | ForEach-Object {
        Write-Host "  - $($_.FullName)" -ForegroundColor Green
    }
} else {
    Write-Host "Warning: Artifacts directory not found at $ArtifactsDir" -ForegroundColor Yellow
}

Write-Host ""

# Create installer if Inno Setup is available
if (-not $NoInstaller) {
    Write-Host "Creating installer..." -ForegroundColor Yellow
    
    # Check for Inno Setup
    $InnoSetupPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $InnoSetupPath)) {
        $InnoSetupPath = "C:\Program Files\Inno Setup 6\ISCC.exe"
    }
    
    if (Test-Path $InnoSetupPath) {
        Write-Host "✓ Inno Setup found: $InnoSetupPath" -ForegroundColor Green
        Write-Host "Compiling installer script..." -ForegroundColor Gray
        
        Push-Location $InstallerDir
        & "$InnoSetupPath" "$InstallerScript"
        $InnoExitCode = $LASTEXITCODE
        Pop-Location
        
        if ($InnoExitCode -eq 0) {
            Write-Host "✓ Installer created successfully!" -ForegroundColor Green
            Get-ChildItem $InstallerDir -Filter "*.exe" -ErrorAction SilentlyContinue | ForEach-Object {
                Write-Host "  Output: $($_.FullName)" -ForegroundColor Green
            }
        } else {
            Write-Host "⚠ Inno Setup compilation failed (exit code: $InnoExitCode)" -ForegroundColor Yellow
        }
    } else {
        Write-Host "ℹ Inno Setup not found. Install Inno Setup 6 to create installers." -ForegroundColor Cyan
        Write-Host "  Download: https://jrsoftware.org/isdl.php" -ForegroundColor Cyan
    }
}

Write-Host ""
Write-Host "╔════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           Build Complete!                 ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════╝" -ForegroundColor Cyan
