[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Invoke-Checked {
    param([Parameter(Mandatory)][string[]]$Command)
    Write-Host ('+ ' + ($Command -join ' '))
    $executable = $Command[0]
    $arguments = $Command[1..($Command.Count - 1)]
    & $executable @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

Push-Location $repoRoot
try {
    Write-Host '+ python scripts/check_cubemx_production.py --list-f407-presets'
    $presets = @(& python scripts/check_cubemx_production.py --list-f407-presets)
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to enumerate the reviewed F407 preset set"
    }
    if ($presets.Count -eq 0) {
        throw "No F407 presets were discovered"
    }
    foreach ($preset in $presets) {
        Invoke-Checked @('cmake', '--fresh', '--preset', $preset)
        Invoke-Checked @('cmake', '--build', '--preset', $preset)
    }
}
finally {
    Pop-Location
}
