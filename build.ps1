#Requires -Version 5.1
<#
.SYNOPSIS
    Interactive builder for the IOBuilderLiveLink Unreal Engine plugin.
.DESCRIPTION
    Detects installed Unreal Engine versions, lets you pick one, builds the
    plugin with UAT, zips the result with the plugin and engine version in
    the filename, and optionally installs it as an engine-wide plugin
    (elevating via UAC when required).
#>

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$RepoRoot      = $PSScriptRoot
$UpluginPath   = Join-Path $RepoRoot 'IOBuilderLiveLink.uplugin'
$PluginName    = 'IOBuilderLiveLink'
$OutputRoot    = Join-Path $env:USERPROFILE 'Documents\IOBuilderLiveLink_Builds'

# ----- helpers ---------------------------------------------------------------

function Write-Section($text) {
    Write-Host ''
    Write-Host ('=' * 64) -ForegroundColor DarkCyan
    Write-Host "  $text"  -ForegroundColor Cyan
    Write-Host ('=' * 64) -ForegroundColor DarkCyan
}

function Get-PluginVersion {
    $u = Get-Content $UpluginPath -Raw | ConvertFrom-Json
    return $u.VersionName
}

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p  = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# ----- engine discovery ------------------------------------------------------

function Find-UnrealEngines {
    $engines = @{}

    # 1) Epic Launcher manifest (authoritative for launcher installs)
    $manifest = 'C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path $manifest) {
        try {
            $data = Get-Content $manifest -Raw | ConvertFrom-Json
            foreach ($item in $data.InstallationList) {
                if ($item.AppName -match '^UE_(\d+\.\d+)$' -and (Test-Path $item.InstallLocation)) {
                    $key = $item.InstallLocation.TrimEnd('\')
                    $engines[$key] = [PSCustomObject]@{
                        Version = $matches[1]
                        Path    = $key
                        Source  = 'Launcher'
                    }
                }
            }
        } catch {
            Write-Verbose "Failed to parse launcher manifest: $_"
        }
    }

    # 2) Filesystem scan of common install roots (catches manual installs)
    $commonRoots = @(
        'C:\Program Files\Epic Games',
        'D:\Program Files\Epic Games',
        'D:\Epic Games',
        'E:\Epic Games'
    )
    foreach ($root in $commonRoots) {
        if (-not (Test-Path $root)) { continue }
        Get-ChildItem -Path $root -Directory -Filter 'UE_*' -ErrorAction SilentlyContinue | ForEach-Object {
            if ($_.Name -notmatch '^UE_(\d+\.\d+)$') { return }
            $version = $matches[1]
            $key     = $_.FullName.TrimEnd('\')
            if ($engines.ContainsKey($key)) { return }
            if (-not (Test-Path (Join-Path $key 'Engine\Build\BatchFiles\RunUAT.bat'))) { return }
            $engines[$key] = [PSCustomObject]@{
                Version = $version
                Path    = $key
                Source  = 'Filesystem'
            }
        }
    }

    return @($engines.Values | Sort-Object { [version]$_.Version })
}

function Select-Engine($engines) {
    Write-Host ''
    Write-Host 'Installed Unreal Engine versions:' -ForegroundColor Cyan
    for ($i = 0; $i -lt $engines.Count; $i++) {
        $e = $engines[$i]
        Write-Host ("  [{0}] UE {1,-5} {2}  ({3})" -f ($i + 1), $e.Version, $e.Path, $e.Source)
    }
    Write-Host ''
    while ($true) {
        $sel = Read-Host 'Select engine (number)'
        if ($sel -match '^\d+$') {
            $idx = [int]$sel - 1
            if ($idx -ge 0 -and $idx -lt $engines.Count) {
                return $engines[$idx]
            }
        }
        Write-Host 'Invalid selection.' -ForegroundColor Yellow
    }
}

# ----- build / package -------------------------------------------------------

function Invoke-PluginBuild {
    param($Engine, $StagingDir)

    $runUAT = Join-Path $Engine.Path 'Engine\Build\BatchFiles\RunUAT.bat'
    if (-not (Test-Path $runUAT)) { throw "RunUAT.bat not found at $runUAT" }

    Write-Section "Building against UE $($Engine.Version)"
    Write-Host "Engine:  $($Engine.Path)"
    Write-Host "Staging: $StagingDir"
    Write-Host ''

    & $runUAT BuildPlugin "-plugin=$UpluginPath" "-package=$StagingDir"
    if ($LASTEXITCODE -ne 0) { throw "Build failed (RunUAT exit $LASTEXITCODE)" }
}

function Set-OutputEngineVersion {
    param($StagingDir, $EngineVersion)
    $outUplugin = Join-Path $StagingDir "$PluginName.uplugin"
    if (-not (Test-Path $outUplugin)) { return }
    $content = Get-Content $outUplugin -Raw
    $target  = "$EngineVersion.0"
    $updated = $content -replace '"EngineVersion"\s*:\s*"[^"]*"', "`"EngineVersion`": `"$target`""
    if ($updated -ne $content) {
        Set-Content -Path $outUplugin -Value $updated -Encoding ascii -NoNewline
        Write-Host "Patched EngineVersion in output .uplugin -> $target"
    }
}

function New-PluginZip {
    param($StagingDir, $ZipPath)
    if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
    # Path is a directory => archive contains that directory as the root entry,
    # so the resulting zip extracts to a top-level IOBuilderLiveLink folder.
    Compress-Archive -Path $StagingDir -DestinationPath $ZipPath -CompressionLevel Optimal
}

# ----- install as engine plugin (UAC) ---------------------------------------

function Get-ExistingEngineInstalls {
    param($Engine)
    @(
        (Join-Path $Engine.Path "Engine\Plugins\$PluginName"),
        (Join-Path $Engine.Path "Engine\Plugins\Marketplace\$PluginName"),
        (Join-Path $Engine.Path "Engine\Plugins\Runtime\$PluginName")
    ) | Where-Object { Test-Path $_ }
}

function Install-EnginePlugin {
    param($Engine, $StagingDir)

    $targetParent = Join-Path $Engine.Path 'Engine\Plugins\Marketplace'
    $targetPath   = Join-Path $targetParent $PluginName
    $existing     = @(Get-ExistingEngineInstalls -Engine $Engine)

    Write-Section "Install into UE $($Engine.Version)"
    if ($existing.Count -gt 0) {
        Write-Host 'Existing installation(s) will be removed first:' -ForegroundColor Yellow
        $existing | ForEach-Object { Write-Host "  - $_" }
    }
    Write-Host "Install target: $targetPath"

    if (Test-IsAdmin) {
        Invoke-LocalInstall -StagingDir $StagingDir -TargetParent $targetParent `
                            -TargetPath $targetPath -Existing $existing
        return
    }

    Write-Host ''
    Write-Host 'Administrator rights required - you will see a UAC prompt.' -ForegroundColor Yellow

    $logFile = Join-Path ([IO.Path]::GetTempPath()) ("iobl_install_{0}.log" -f ([guid]::NewGuid().ToString('N')))
    $tmpScript = Join-Path ([IO.Path]::GetTempPath()) ("iobl_install_{0}.ps1" -f ([guid]::NewGuid().ToString('N')))

    $esc = { param($s) $s -replace "'", "''" }
    $existingLiteral = if ($existing.Count -eq 0) {
        '@()'
    } else {
        '@(' + (($existing | ForEach-Object { "'" + (& $esc $_) + "'" }) -join ', ') + ')'
    }

    $body = @"
`$ErrorActionPreference = 'Stop'
`$log = '$(& $esc $logFile)'
function Log(`$m) { `$m | Out-File -FilePath `$log -Append -Encoding utf8 }
try {
    foreach (`$p in $existingLiteral) {
        if (Test-Path `$p) { Log "Removing `$p"; Remove-Item `$p -Recurse -Force }
    }
    `$parent = '$(& $esc $targetParent)'
    `$target = '$(& $esc $targetPath)'
    `$source = '$(& $esc $StagingDir)'
    if (-not (Test-Path `$parent)) { New-Item -ItemType Directory -Path `$parent -Force | Out-Null }
    New-Item -ItemType Directory -Path `$target -Force | Out-Null
    Log "Copying `$source\* -> `$target"
    Copy-Item -Path (Join-Path `$source '*') -Destination `$target -Recurse -Force
    Log 'OK'
    exit 0
} catch {
    Log "ERROR: `$_"
    exit 1
}
"@
    Set-Content -Path $tmpScript -Value $body -Encoding utf8

    try {
        $proc = Start-Process -FilePath 'powershell.exe' `
            -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', $tmpScript) `
            -Verb RunAs -PassThru -Wait -ErrorAction Stop

        if (Test-Path $logFile) {
            Get-Content $logFile | ForEach-Object { Write-Host "  $_" }
        }
        if ($proc.ExitCode -ne 0) {
            throw "Elevated install failed (exit $($proc.ExitCode))"
        }
        Write-Host 'Installation complete.' -ForegroundColor Green
    } catch [System.ComponentModel.Win32Exception] {
        throw 'UAC prompt was cancelled - nothing was installed.'
    } finally {
        Remove-Item $tmpScript -Force -ErrorAction SilentlyContinue
        Remove-Item $logFile   -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-LocalInstall {
    param($StagingDir, $TargetParent, $TargetPath, $Existing)
    foreach ($p in $Existing) {
        Write-Host "Removing $p"
        Remove-Item $p -Recurse -Force
    }
    if (-not (Test-Path $TargetParent)) { New-Item -ItemType Directory -Path $TargetParent -Force | Out-Null }
    New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
    Write-Host "Copying to $TargetPath"
    Copy-Item -Path (Join-Path $StagingDir '*') -Destination $TargetPath -Recurse -Force
    Write-Host 'Installation complete.' -ForegroundColor Green
}

# ----- main ------------------------------------------------------------------

try {
    Write-Section 'IOBuilderLiveLink Plugin Builder'

    $pluginVersion = Get-PluginVersion
    Write-Host "Plugin version: $pluginVersion"

    Write-Host 'Searching for Unreal Engine installations...'
    $engines = Find-UnrealEngines
    if ($engines.Count -eq 0) {
        throw 'No Unreal Engine installations found.'
    }

    $selected = Select-Engine $engines
    Write-Host ''
    Write-Host "Selected: UE $($selected.Version) - $($selected.Path)" -ForegroundColor Green

    if (-not (Test-Path $OutputRoot)) { New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null }
    $jobName       = "${PluginName}_v${pluginVersion}_UE$($selected.Version)"
    $stagingParent = Join-Path $OutputRoot $jobName
    $stagingDir    = Join-Path $stagingParent $PluginName
    $zipPath       = Join-Path $OutputRoot "$jobName.zip"

    if (Test-Path $stagingParent) {
        Write-Host "Cleaning previous staging: $stagingParent"
        Remove-Item $stagingParent -Recurse -Force
    }
    New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

    Invoke-PluginBuild      -Engine $selected -StagingDir $stagingDir
    Set-OutputEngineVersion -StagingDir $stagingDir -EngineVersion $selected.Version

    Write-Section 'Packaging zip'
    Write-Host "Zip: $zipPath"
    New-PluginZip -StagingDir $stagingDir -ZipPath $zipPath
    Write-Host 'Zip created.' -ForegroundColor Green

    Write-Host ''
    $ans = Read-Host "Install as engine plugin into UE $($selected.Version)? (requires admin) [y/N]"
    if ($ans -match '^(y|yes)$') {
        Install-EnginePlugin -Engine $selected -StagingDir $stagingDir
    } else {
        Write-Host 'Skipping engine plugin installation.'
    }

    Write-Section 'Done'
    Write-Host "Build directory: $stagingDir"
    Write-Host "Zip artifact:    $zipPath"
    exit 0
} catch {
    Write-Host ''
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit 1
}
