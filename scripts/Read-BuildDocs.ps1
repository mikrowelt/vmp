#Requires -Version 5.1
<#
.SYNOPSIS
    Extracts the build-related sections from README.md and docs/building.md.
.DESCRIPTION
    Reads the project README and developer docs, then prints only the sections
    relevant to getting started and building: prerequisites, installation,
    build instructions, and quick-start commands.
.PARAMETER OutFile
    If specified, writes the extracted content to this file instead of the console.
.EXAMPLE
    .\Read-BuildDocs.ps1
    .\Read-BuildDocs.ps1 -OutFile build-instructions.txt
#>
[CmdletBinding()]
param(
    [string]$OutFile = ''
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path "$PSScriptRoot\.."

$ReadmePath = "$RepoRoot\README.md"
$BuildingPath = "$RepoRoot\docs\building.md"

function Read-FileLines {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "File not found: $Path"
    }
    return Get-Content -Path $Path -Encoding UTF8
}

function Get-RelevantReadmeSections {
    param([string[]]$Lines)
    $output = @()
    $include = $false
    foreach ($line in $Lines) {
        # Start capturing at "Getting started"
        if ($line -match '^#{1,4}\s+(Getting started|Quick start|Prerequisites|Installation|Build)') {
            $include = $true
        }
        # Stop at the next top-level heading that is not build-related
        elseif ($include -and $line -match '^#\s+') {
            $include = $false
        }
        if ($include) {
            $output += $line
        }
    }
    return $output
}

function Get-BuildingDoc {
    param([string[]]$Lines)
    # docs/building.md is almost entirely build-related, so return the whole file
    return $Lines
}

$readme = Read-FileLines $ReadmePath
$building = Read-FileLines $BuildingPath

$readmeSections = Get-RelevantReadmeSections $readme
$buildingSections = Get-BuildingDoc $building

$buffer = New-Object System.Text.StringBuilder
[void]$buffer.AppendLine("=" * 60)
[void]$buffer.AppendLine("EXTRACTED BUILD INSTRUCTIONS FOR VMP")
[void]$buffer.AppendLine("=" * 60)
[void]$buffer.AppendLine()
[void]$buffer.AppendLine("--- README.md (getting started) ---")
[void]$buffer.AppendLine()
foreach ($line in $readmeSections) { [void]$buffer.AppendLine($line) }
[void]$buffer.AppendLine()
[void]$buffer.AppendLine("--- docs/building.md (full build instructions) ---")
[void]$buffer.AppendLine()
foreach ($line in $buildingSections) { [void]$buffer.AppendLine($line) }

if ($OutFile) {
    $buffer.ToString() | Set-Content -Path $OutFile -Encoding UTF8
    Write-Host "Wrote extracted build docs to: $OutFile" -ForegroundColor Green
} else {
    Write-Host $buffer.ToString()
}
