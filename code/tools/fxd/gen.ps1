<#
.Synopsis
Generates Visual Studio project files for the specified Cfx project.

.Description
This command can be used to (re)generate Visual Studio project files for a game specified on the command line.

By specifying the -Game argument, a different game can be selected.

.Parameter Game
Specifies the game to build project files for. This should be any of 'five', 'ny', 'server' or 'rdr3'.

.Example
fxd gen

.Example
fxd gen -game server
#>
[CmdletBinding(PositionalBinding=$false)]
param(
	[string]$Game = 'five',
	[string]$Args = ''
)

$vswhereVersion = (& "$PSScriptRoot\..\ci\vswhere.exe" -prerelease -latest -property catalog_buildVersion -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64)
if ([string]::IsNullOrWhiteSpace($vswhereVersion)) {
	# vswhere finds no instances (e.g. broken installer cache); fall back to detecting the VS2022 toolchain on disk
	if (Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat") {
		$vswhereVersion = "17.0"
	}
}
$VSVersion = [System.Version]::Parse($vswhereVersion)
if ($VSVersion -ge [System.Version]::Parse("17.0")) {
	$VSLine = "vs2022"
} elseif ($VSVersion -ge [System.Version]::Parse("16.0")) {
	$VSLine = "vs2019"
} else {
	throw "Unknown or invalid VS version."
}

Push-Location "$PSScriptRoot\..\..\"
& "$PSScriptRoot\..\ci\premake5.exe" $VSLine "--game=$Game" $Args
Pop-Location
