[CmdletBinding()]
param(
    [string]$CxxCompiler = ''
)

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

function Resolve-Compiler {
    param([string]$Requested)

    if (-not [string]::IsNullOrWhiteSpace($Requested)) {
        if (Test-Path -LiteralPath $Requested -PathType Leaf) {
            return (Resolve-Path -LiteralPath $Requested).Path
        }
        $requestedCommand = Get-Command $Requested -ErrorAction SilentlyContinue
        if ($null -ne $requestedCommand) {
            return $requestedCommand.Source
        }
        throw "Requested host C++ compiler was not found: $Requested"
    }

    $mingwCommand = Get-Command 'x86_64-w64-mingw32-clang++.exe' `
        -ErrorAction SilentlyContinue
    if ($null -ne $mingwCommand) {
        return $mingwCommand.Source
    }

    $wingetRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    if (Test-Path -LiteralPath $wingetRoot -PathType Container) {
        $portableCompiler = Get-ChildItem -LiteralPath $wingetRoot `
                -Directory -Filter 'MartinStorsjo.LLVM-MinGW.*' |
            ForEach-Object {
                Get-ChildItem -Path (
                    Join-Path $_.FullName `
                        'llvm-mingw-*\bin\x86_64-w64-mingw32-clang++.exe'
                ) -File -ErrorAction SilentlyContinue
            } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($null -ne $portableCompiler) {
            return $portableCompiler.FullName
        }
    }

    foreach ($name in @('g++.exe', 'clang++.exe')) {
        $candidate = Get-Command $name -ErrorAction SilentlyContinue
        if ($null -ne $candidate) {
            return $candidate.Source
        }
    }
    throw 'No native C++17 compiler found. Set PNX_HOST_CXX or use -CxxCompiler.'
}

if ([string]::IsNullOrWhiteSpace($CxxCompiler)) {
    $CxxCompiler = $env:PNX_HOST_CXX
}
$resolvedCompiler = Resolve-Compiler $CxxCompiler
$compilerBin = Split-Path -Parent $resolvedCompiler
$previousPath = $env:PATH

Push-Location $repoRoot
try {
    $env:PATH = $compilerBin + [IO.Path]::PathSeparator + $previousPath
    Write-Host "Host C++ compiler: $resolvedCompiler"
    Invoke-Checked @(
        'cmake', '--fresh', '--preset', 'host-tests',
        "-DCMAKE_CXX_COMPILER=$resolvedCompiler"
    )
    Invoke-Checked @('cmake', '--build', '--preset', 'host-tests')
    Invoke-Checked @('ctest', '--preset', 'host-tests', '--output-on-failure')
}
finally {
    $env:PATH = $previousPath
    Pop-Location
}
