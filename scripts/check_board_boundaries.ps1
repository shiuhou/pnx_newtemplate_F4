[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceExtensions = @('.c', '.cpp', '.h', '.hpp')
$bannedPattern =
    '\bHAL_[A-Za-z0-9_]+|\b(hcan|hfdcan|huart)[0-9]+\b|' +
    'PNX_BOARD_|DJI_C_BOARD|STM32F407'

Push-Location $repoRoot
try {
    $sourceFiles = Get-ChildItem -LiteralPath @(
        (Join-Path $repoRoot 'pnx_devices'),
        (Join-Path $repoRoot 'pnx_modules'),
        (Join-Path $repoRoot 'demo')
    ) -Recurse -File |
        Where-Object { $_.Extension -in $sourceExtensions }
    $violations = $sourceFiles |
        Select-String -Pattern $bannedPattern -CaseSensitive
    if ($violations) {
        $violations | ForEach-Object {
            Write-Error "$($_.Path):$($_.LineNumber):$($_.Line)"
        }
        throw 'Board boundary violation found in device/module/demo source.'
    }

    $legacyDirectories = Get-ChildItem -LiteralPath (
        Join-Path $repoRoot 'boards/dji_c_board_f407'
    ) -Recurse -Directory |
        Where-Object { $_.Name -in @('User', 'Task', 'Service') }
    if ($legacyDirectories) {
        throw 'Legacy User/Task/Service directory copied into F407 board tree.'
    }

    foreach ($generated in @(
        'configs/generated/config.hpp',
        'configs/generated/robot_config.hpp'
    )) {
        if (Test-Path -LiteralPath (Join-Path $repoRoot $generated)) {
            throw "Source-tree generated configuration exists: $generated"
        }
    }

    $trackedGenerated = & git ls-files configs/generated
    if ($LASTEXITCODE -ne 0) {
        throw 'git ls-files failed.'
    }
    if ($trackedGenerated) {
        throw 'Generated configuration is tracked in the source tree.'
    }

    Write-Host 'Board boundary checks passed.'
}
finally {
    Pop-Location
}
