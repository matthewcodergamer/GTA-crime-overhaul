# Build, Install and Release Guide

## Supported runtime target

- Windows x64
- Grand Theft Auto V Story Mode / offline
- GTA V Legacy and Enhanced where supported by the installed Script Hook V runtime
- Native C++ ASI

The official Script Hook V page is the source of truth for current supported GTA patches.

## Runtime dependencies

Users install separately:

1. GTA V.
2. Current compatible Script Hook V.
3. ASI loader (`dinput8.dll`) supplied with Script Hook V or a compatible loader.

This repository does **not** redistribute `ScriptHookV.dll` or `dinput8.dll`.

## Developer dependency

Download the official **Script Hook V SDK** from Alexander Blade's developer link.

Set:

```text
SCRIPTHOOKV_SDK_DIR=C:\path\to\ScriptHookV_SDK
```

The directory must contain:

```text
ScriptHookV_SDK/
├─ inc/
│  └─ main.h
└─ lib/
   └─ ScriptHookV.lib
```

## Visual Studio / CMake build

From a Developer PowerShell:

```powershell
cmake -S . -B build -A x64 -DSCRIPTHOOKV_SDK_DIR="C:\path\to\ScriptHookV_SDK"
cmake --build build --config Release
```

Expected output:

```text
build/bin/Release/GTA_Crime_Overhaul.asi
```

Depending on the generator, the configuration directory may vary, but the file suffix is always `.asi`.

## Install/package layout

```text
Grand Theft Auto V/
├─ GTA5.exe
├─ ScriptHookV.dll                 # user installs separately
├─ dinput8.dll                     # user installs separately
├─ GTA_Crime_Overhaul.asi
└─ GTA_Crime_Overhaul/
   ├─ GTA_Crime_Overhaul.ini
   ├─ data/
   │  ├─ businesses.json
   │  ├─ economy.json
   │  └─ dialogue/
   │     └─ base.json
   ├─ saves/                       # created automatically
   └─ logs/                        # created automatically
```

On first successful startup the ASI creates:

```text
GTA_Crime_Overhaul/saves/world.json
GTA_Crime_Overhaul/logs/GTA_Crime_Overhaul.log
```

## Phase-0 smoke test

1. Back up your modded GTA V installation/save configuration.
2. Confirm current Script Hook V supports your game build.
3. Copy the ASI and `GTA_Crime_Overhaul/` folder to the GTA V root.
4. Launch **Story Mode** only.
5. Wait until the player is controllable.
6. Exit the game.
7. Confirm `GTA_Crime_Overhaul.log` contains:

```text
GTA Crime Overhaul starting.
Native ASI runtime initialized successfully.
```

8. Confirm `world.json` exists and has `schemaVersion: 1`.
9. If `DebugLogging=true`, a heartbeat is written periodically.

No Phase-0 gameplay is expected yet; the objective is loader/runtime/persistence stability.

## CI

`.github/workflows/build.yml` builds on a Windows GitHub runner.

The workflow:

1. checks out source,
2. downloads the Script Hook V developer SDK from the official developer download URL,
3. locates `inc/main.h` and `lib/ScriptHookV.lib`,
4. configures CMake for x64,
5. builds Release,
6. stages the ASI + this project's config/data only,
7. uploads a `GTA-Crime-Overhaul` ZIP/artifact.

If the official developer download URL changes, update the workflow variable; do not solve this by committing `ScriptHookV.dll`.

## Release contents

A public release should contain only project-owned files such as:

```text
GTA_Crime_Overhaul.asi
GTA_Crime_Overhaul/GTA_Crime_Overhaul.ini
GTA_Crime_Overhaul/data/...
README.md
CHANGELOG.md
```

Do not package:

```text
ScriptHookV.dll
dinput8.dll
GTA5.exe
Rockstar assets extracted from game archives
RDR2 assets
```

## Legacy / Enhanced testing

Script Hook V states that legacy ASI plugins are generally compatible with Enhanced, but each Crime Overhaul feature still receives build validation because:

- interior activation can differ,
- native behavior can have patch-specific edge cases,
- DLC model availability differs,
- mission/streaming behavior may differ,
- future engine updates can break assumptions.

Avoid raw memory offsets unless a feature is impossible through stable natives. If offsets ever become necessary, isolate them behind per-build adapters and never make them the default design path.

## Troubleshooting order

### ASI does not load

1. Check Script Hook V supports the installed GTA build.
2. Check `ScriptHookV.dll` and ASI loader installation.
3. Check `GTA_Crime_Overhaul.asi` is in the GTA root.
4. Check Windows security/quarantine.
5. Check other ASI conflicts.

### Log exists but save does not

Check write permissions for the GTA folder and inspect the log for `Unable to initialize world save state.`

### Game updated

Update Script Hook V first. The plugin is intentionally built to depend on Script Hook V rather than patch-specific memory hooks wherever possible.

### Gameplay conflict with a Rockstar mission

This is a bug to handle in `MissionCompatibilityGate`; do not fix it by globally disabling mission detection.
