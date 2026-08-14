# VMP Build Helper Scripts

These PowerShell scripts automate the steps from `docs/building.md` so you do not have to run them by hand.

## Quick start

Open a **PowerShell 7 terminal as Administrator** in the repo root (`C:\vmp`) and run:

```powershell
# 1. See what is already installed and what is missing
.\scripts\Check-Prerequisites.ps1

# 2. Install missing dependencies (Visual Studio, Python, MSYS2, Node, Yarn, ...)
.\scripts\Install-Prerequisites.ps1

# 3. Run the full setup and open Visual Studio
.\scripts\Setup-VMP.ps1
```

## Scripts

| Script | What it does |
|--------|--------------|
| `Check-Prerequisites.ps1` | Audits VS 2022 workloads, PowerShell 7, Python + `py` + setuptools, MSYS2, Node, Yarn, Git symlinks, and free disk space. |
| `Install-Prerequisites.ps1` | Uses `winget` to install whatever `Check-Prerequisites.ps1` complains about. Interactive by default; use `-AutoInstall` to skip prompts. |
| `Setup-VMP.ps1` | Runs the documented build setup end-to-end: submodules, setuptools, `fxd get-chrome`, `prebuild`, `fxd gen`, and finally `fxd vs` (or `fxd build` with `-BuildOnly`). |
| `Read-BuildDocs.ps1` | Prints the build-related sections of `README.md` and `docs/building.md`. |

## Common options

```powershell
# Build for the server instead of the FiveM client
.\scripts\Setup-VMP.ps1 -Game server

# Build from the command line instead of opening Visual Studio
.\scripts\Setup-VMP.ps1 -BuildOnly

# Stop after generating project files, then open VS yourself later
.\scripts\Setup-VMP.ps1 -StopAfter Gen
```

## Notes

* Run `Install-Prerequisites.ps1` as Administrator. Visual Studio and MSYS2 installs require elevation.
* The VMP build hard-codes `C:\msys64`. If `winget` installs MSYS2 elsewhere, move it to `C:\msys64`.
* After installing tools with `winget`, restart your terminal so new PATH entries take effect before running `Check-Prerequisites.ps1` or `Setup-VMP.ps1`.
* The repo must be cloned with symlinks enabled: `git config --global core.symlinks true`.
