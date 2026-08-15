# VMP Agent Guide

> **Quick reference:** start with the basic-memory notes `[[VMP_QUICKSTART]]` and `[[VMP_INDEX]]` for the fastest entry. This file is the longer project-level guide.

This document helps AI agents and contributors work effectively on the VMP project.

## Quick reference

```bat
# Client build
fxd gen -game five
fxd build -game five -Configuration Release

# Server build
fxd gen -game server
fxd build -game server -Configuration Release
```

Output paths:
- Client: `code\bin\five\release\`
- Server: `code\bin\server\windows\release\`

Run client without auto-updates:

```bat
VMP.exe -noupdate +connect <ip>:30120
```

Common fixes:
- `getXState` not found → `code/client/shared/FoxApi.h`
- `ros::GetApiIdentifier` not found → `code/components/ros-patches-five/include/LegitimacyAPI.h` and `src/LegitimacyChecking.cpp`
- `v8_monolith.lib` corrupt → delete and re-download from `cdn.vmp.ir/mirrors/vendor/v8/12.4/`
- Server auth error → omit `sv_licenseKey` for standalone mode

## Project overview

VMP is a fork of the Cfx.re/FiveM codebase (`github.com/mikrowelt/vmp`, originally `v-mp/vmp`). It contains:

- A **FiveM client** for GTA V (`-game five`)
- An **FXServer** backend (`-game server`)
- A custom VMP launcher/UI backend

Knowledge base: see the `vmp` notes in the workspace basic-memory project (ARCHITECTURE, DECISIONS, API, VMP_CLIENT_BUILD, VMP_SERVER_BUILD).

## Repository layout

- `code/` — Native C++ source, components, and build tooling (`fxd`, `premake5`)
- `data/` — Runtime data and assets
- `docs/` — Developer docs (`building.md`, `layout.md`, this file)
- `vendor/` — Git submodules and prebuilt libs (V8, CEF, etc.)
- `scripts/` — PowerShell helper scripts for Windows setup

Key components agents will touch most often:

- `code/client/launcher` — `VMP.exe` bootstrapper
- `code/components/glue` — UI, NUI, Discord/Discourse identity, join flow
- `code/components/font-renderer` — In-game branding/emoji rendering
- `code/components/ros-patches-five` — ROS/entitlement/auth APIs
- `code/components/citizen-server-impl` — FXServer core, `ServerAuth.cpp`
- `code/client/shared` — Shared headers (`CnlEndpoint.h`, `FoxApi.h`)
- `code/vendor/v8-monolith.lua` — V8 library download logic

## Build commands

Windows only. Build from repo root:

```bat
fxd gen -game five
fxd build -game five -Configuration Release
```

Server:

```bat
fxd gen -game server
fxd build -game server -Configuration Release
```

Output:
- Client: `code\bin\five\release\`
- Server: `code\bin\server\windows\release\`

Run `prebuild` before `fxd gen` if native bindings are missing:

```bat
prebuild
```

## Common issues and fixes

### `getXState`: identifier not found

`code/client/shared/FoxApi.h` defines `getXState()` and `getFaceItState()`. If missing, rebuild the shared header or check include paths.

### `ros::GetApiIdentifier`: not a member

Declared in `code/components/ros-patches-five/include/LegitimacyAPI.h`, defined in `src/LegitimacyChecking.cpp`. Ensure `ros-patches-five` is enabled in `code/components/config.lua`.

### `v8_monolith.lib`: invalid or corrupt file

Delete the partial file and re-download from `https://cdn.vmp.ir/mirrors/vendor/v8/12.4/v8_monolith.lib`. The file is ~1.6 GB.

### Client tries to auto-update

Use `-noupdate` on the command line:

```bat
VMP.exe -noupdate +connect <ip>:30120
```

### Server fails with VMP auth error

Run in standalone mode by omitting `sv_licenseKey` in `server.cfg`. The patched `ServerAuth.cpp` handles this.

## Conventions for making changes

1. Keep changes minimal and focused.
2. Mirror the existing code style (tabs, naming, etc.).
3. If you add a new shared API, put the declaration in `code/client/shared` or the appropriate component `include/`.
4. If you change a build/config macro (e.g., `GAME_PATCH_URL`), document it in the knowledge base.
5. After code changes, run the validation pipeline if possible: `fxd gen` + `fxd build` for the affected target.
6. Update the basic-memory docs: ARCHITECTURE, DECISIONS, API, VMP_CLIENT_BUILD, or VMP_SERVER_BUILD as appropriate.

## VMP-specific URLs and defaults

- Update CDN: `https://cdn.vmp.ir/updates`
- Patch CDN: `https://cdn.vmp.ir/patches`
- Auth API: `https://api.vmp.ir/`
- Link protocol: `vmp://`

## Important notes

- The client links a ~1.6 GB `v8_monolith.lib`. Builds take 20–40 minutes.
- The `data\game-storage` folder is the GTA V cache and must be preserved between launches.
- Do not expose RDP or WinRM to the public internet on the build server.
- The client is distributed as a zip of `code\bin\five\release\` plus a `PlayVMP.bat` launcher.

## See also

- `docs/building.md` — upstream Cfx build instructions
- `docs/layout.md` — source layout overview
- Basic-memory knowledge base notes: ARCHITECTURE, DECISIONS, API, VMP_CLIENT_BUILD, VMP_SERVER_BUILD
