param(
    [string]$InstallDir = "$env:CommonProgramFiles\VST3",
    [switch]$NoPause
)

$ErrorActionPreference = "Stop"

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Administrator)) {
    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-InstallDir", "`"$InstallDir`""
    )

    if ($NoPause) {
        $arguments += "-NoPause"
    }

    Start-Process -FilePath "powershell.exe" -ArgumentList $arguments -Verb RunAs
    exit
}

$scriptDir = Split-Path -Parent $PSCommandPath
$pluginZip = Get-ChildItem -LiteralPath $scriptDir -Filter "LUFS-Meter-Plus-*-Windows-VST3.zip" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($null -eq $pluginZip) {
    throw "Windows VST3 zip package was not found next to the installer script."
}

$tempRoot = Join-Path $env:TEMP ("LufsMeterPlusInstall-" + [Guid]::NewGuid().ToString("N"))
$extractDir = Join-Path $tempRoot "extract"

try {
    New-Item -ItemType Directory -Path $extractDir -Force | Out-Null
    Expand-Archive -LiteralPath $pluginZip.FullName -DestinationPath $extractDir -Force

    $pluginBundle = Get-ChildItem -LiteralPath $extractDir -Directory -Filter "LUFS Meter Plus.vst3" -Recurse |
        Select-Object -First 1

    if ($null -eq $pluginBundle) {
        throw "LUFS Meter Plus.vst3 was not found inside $($pluginZip.Name)."
    }

    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null

    $destination = Join-Path $InstallDir $pluginBundle.Name
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }

    Copy-Item -LiteralPath $pluginBundle.FullName -Destination $destination -Recurse -Force

    Write-Host ""
    Write-Host "LUFS Meter Plus VST3 installed successfully."
    Write-Host "Installed to: $destination"
    Write-Host "Restart OBS before scanning/loading the plugin again."
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

if (-not $NoPause) {
    Write-Host ""
    Read-Host "Press Enter to close"
}
