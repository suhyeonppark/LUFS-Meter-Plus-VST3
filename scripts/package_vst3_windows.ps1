param(
    [string]$Configuration = "Release",
    [string]$Version = "1.0.0",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$distDir = Join-Path $repoRoot "dist"
$stageDir = Join-Path $distDir "LUFS-Meter-Plus-$Version-Windows-VST3"
$iexpressWorkDir = Join-Path $env:TEMP ("LufsMeterPlusIExpress-" + [Guid]::NewGuid().ToString("N"))
$artifactDir = Join-Path $repoRoot "build\vs2022\LufsMeterPlus_artefacts\$Configuration\VST3"
$pluginBundle = Join-Path $artifactDir "LUFS Meter Plus.vst3"
$pluginZip = Join-Path $distDir "LUFS-Meter-Plus-$Version-Windows-VST3.zip"
$installerExe = Join-Path $distDir "LUFS-Meter-Plus-$Version-Windows-VST3-Installer.exe"
$cmakeExe = "cmake"
$vsCmakeExe = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not $SkipBuild) {
    if (-not (Get-Command $cmakeExe -ErrorAction SilentlyContinue)) {
        if (Test-Path -LiteralPath $vsCmakeExe) {
            $cmakeExe = $vsCmakeExe
        }
        else {
            throw "cmake was not found in PATH, and the Visual Studio bundled cmake was not found."
        }
    }

    & $cmakeExe --build (Join-Path $repoRoot "build\vs2022") --config $Configuration --target LufsMeterPlus_VST3
    if ($LASTEXITCODE -ne 0) {
        if (Test-Path -LiteralPath $pluginBundle) {
            Write-Warning "VST3 build returned exit code $LASTEXITCODE, but the plug-in bundle exists. Continuing with packaging."
        }
        else {
            throw "VST3 build failed and no plugin bundle exists at $pluginBundle"
        }
    }
}

if (-not (Test-Path -LiteralPath $pluginBundle)) {
    throw "Plugin bundle not found: $pluginBundle"
}

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

Copy-Item -LiteralPath $pluginBundle -Destination $stageDir -Recurse -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "install_vst3_windows.ps1") -Destination $stageDir -Force

$readme = @"
LUFS Meter Plus $Version
Windows VST3 Installation Guide

What this is
------------
LUFS Meter Plus is a VST3 loudness meter and true-peak safety limiter for
Windows hosts such as OBS, DAWs, and VST3-compatible audio tools.

Recommended installation
------------------------
Run:
  LUFS-Meter-Plus-$Version-Windows-VST3-Installer.exe

The installer copies the plug-in to:
  C:\Program Files\Common Files\VST3\LUFS Meter Plus.vst3

Windows may ask for administrator permission. This is expected because the
system VST3 folder is protected.

Manual installation
-------------------
If you do not want to use the installer, copy the whole folder:
  LUFS Meter Plus.vst3

to:
  C:\Program Files\Common Files\VST3\

Important: copy the entire .vst3 folder, not only the file inside it.

After installation
------------------
1. Restart OBS, your DAW, or the host application.
2. Run the host's plug-in scan/rescan if the plug-in does not appear.
3. Look for the plug-in name:
   LUFS Meter Plus

Troubleshooting
---------------
- If the installer does nothing, right-click it and choose "Run as administrator".
- If the plug-in does not appear, confirm this folder exists:
  C:\Program Files\Common Files\VST3\LUFS Meter Plus.vst3
- If OBS was already open during installation, close OBS completely and open it again.
- This package is for 64-bit Windows VST3 hosts.

UDP output
----------
When the plug-in is running, it can broadcast loudness values as JSON once per
second to UDP port 49152. A simple listener is included in the project source as:
  udp_listen.ps1
"@
Set-Content -LiteralPath (Join-Path $stageDir "README.txt") -Value $readme -Encoding UTF8

if (Test-Path -LiteralPath $pluginZip) {
    Remove-Item -LiteralPath $pluginZip -Force
}
Compress-Archive -LiteralPath (Join-Path $stageDir "LUFS Meter Plus.vst3"), (Join-Path $stageDir "README.txt") -DestinationPath $pluginZip -Force

$payloadZip = Join-Path $stageDir (Split-Path -Leaf $pluginZip)
Copy-Item -LiteralPath $pluginZip -Destination $payloadZip -Force

$iexpressPayloadZip = Join-Path $iexpressWorkDir (Split-Path -Leaf $pluginZip)
$iexpressInstallScript = Join-Path $iexpressWorkDir "install_vst3_windows.ps1"
$sedPath = Join-Path $iexpressWorkDir "installer.sed"
$iexpressOutputExe = Join-Path $iexpressWorkDir (Split-Path -Leaf $installerExe)
$installerName = Split-Path -Leaf $installerExe
$payloadZipName = Split-Path -Leaf $payloadZip

if (Test-Path -LiteralPath $iexpressWorkDir) {
    Remove-Item -LiteralPath $iexpressWorkDir -Recurse -Force
}
New-Item -ItemType Directory -Path $iexpressWorkDir -Force | Out-Null
Copy-Item -LiteralPath $payloadZip -Destination $iexpressPayloadZip -Force
Copy-Item -LiteralPath (Join-Path $stageDir "install_vst3_windows.ps1") -Destination $iexpressInstallScript -Force

$sed = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=0
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=
DisplayLicense=
FinishMessage=LUFS Meter Plus installer has finished.
TargetName=$iexpressOutputExe
FriendlyName=LUFS Meter Plus VST3 Installer
AppLaunched=powershell.exe -NoProfile -ExecutionPolicy Bypass -File install_vst3_windows.ps1 -NoPause
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[SourceFiles]
SourceFiles0=$iexpressWorkDir
[SourceFiles0]
install_vst3_windows.ps1=
$payloadZipName=
"@
Set-Content -LiteralPath $sedPath -Value $sed -Encoding ASCII

$iexpressExe = "$env:WINDIR\System32\iexpress.exe"
if (-not (Test-Path -LiteralPath $iexpressExe)) {
    throw "iexpress.exe was not found."
}

if (Test-Path -LiteralPath $installerExe) {
    Remove-Item -LiteralPath $installerExe -Force
}

$iexpressProcess = Start-Process -FilePath $iexpressExe -ArgumentList "/N /Q $sedPath" -Wait -PassThru
if (-not (Test-Path -LiteralPath $iexpressOutputExe)) {
    throw "IExpress failed to create $installerName (exit code $($iexpressProcess.ExitCode))"
}

Copy-Item -LiteralPath $iexpressOutputExe -Destination $installerExe -Force
Remove-Item -LiteralPath $iexpressWorkDir -Recurse -Force

Write-Host "Created:"
Write-Host "  $installerExe"
Write-Host "  $pluginZip"
