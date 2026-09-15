# Build, Install and Release Guide

## Supported runtime target

- Windows x64
- Grand Theft Auto V Story Mode / offline
- GTA V Legacy and Enhanced where supported by the installed Script Hook V runtime
- Native C++20 ASI
- Authoritative binary: `GTA_Crime_Overhaul.asi`

The official Script Hook V page is the source of truth for current supported GTA patches.

## Runtime dependencies

Users install separately:

1. GTA V.
2. Current compatible Script Hook V.
3. ASI loader (`dinput8.dll`) supplied with Script Hook V or a compatible loader.

This repository and its CI artifacts do **not** redistribute `ScriptHookV.dll`, `dinput8.dll`, GTA executables, trainers, or extracted Rockstar assets.

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
cmake -S . -B build -A x64 -DBUILD_TESTING=ON -DSCRIPTHOOKV_SDK_DIR="C:\path\to\ScriptHookV_SDK"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Expected native output:

```text
build/bin/Release/GTA_Crime_Overhaul.asi
```

The generator may place configuration files differently, but the authoritative binary name/suffix is always `GTA_Crime_Overhaul.asi`. CMake rejects non-Windows and non-x64 configurations. Multi-config generators are restricted to `Debug` and `Release`.

Build metadata compiled into the runtime includes:

- project version;
- configuration (`Debug` / `Release`);
- repository commit SHA when Git is available;
- save schema version.

The same provenance is written to startup logging, and CI adds `BUILD_INFO.txt` to its package.

## Install/package layout

```text
Grand Theft Auto V/
├─ GTA5.exe                         # already installed by the user
├─ ScriptHookV.dll                  # user installs separately
├─ dinput8.dll                      # user installs separately
├─ GTA_Crime_Overhaul.asi
└─ GTA_Crime_Overhaul/
   ├─ GTA_Crime_Overhaul.ini
   ├─ data/
   │  ├─ businesses.json
   │  ├─ economy.json
   │  ├─ research/
   │  └─ dialogue/
   │     ├─ base.json
   │     ├─ events.json
   │     ├─ fallbacks.json
   │     └─ role packs...
   ├─ saves/                       # created automatically
   └─ logs/                        # created automatically
```

On first successful startup the ASI creates:

```text
GTA_Crime_Overhaul/saves/world.json
GTA_Crime_Overhaul/logs/GTA_Crime_Overhaul.log
```

The save contains an explicit `schemaVersion`, project version, logical-ID counters and the Stage-0 domain sections. Raw GTA entity handles are rejected by persistence validation.

## Persistence safety

`WorldStateStore` follows this startup/write policy:

1. validate the primary `world.json` before accepting it;
2. require the supported schema version and required top-level sections;
3. reject known raw-handle persistence keys;
4. stage writes through `world.tmp.json`;
5. validate the staged document before replacing the primary;
6. preserve only a validated previous primary as `world.backup.json`;
7. use Windows replace/write-through semantics for the final swap;
8. when the primary is corrupt, quarantine a copy as `world.corrupt-<timestamp>.json`;
9. restore a validated backup when available;
10. if both primary and backup are unusable, create a clean schema-versioned save instead of crashing.

Later schema changes must add explicit migration logic instead of silently accepting an unknown version.

## Stage-0 smoke test

1. Back up the modded GTA V installation/save configuration.
2. Confirm the installed Script Hook V supports that GTA V build.
3. Copy the ASI and `GTA_Crime_Overhaul/` folder to the GTA V root.
4. Launch **Story Mode only**.
5. Wait until the player is controllable.
6. Confirm the game remains stable for at least one minute.
7. With `DebugHotkeys=true`, press **F9** and verify a diagnostics record is appended to the log.
8. Press **F10** and verify the debug overlay toggles on/off.
9. Press **F11** and verify the log records `Debug save validation PASS`.
10. Enter a Rockstar mission or cutscene and confirm diagnostics/overlay report the compatibility gate as restricted/suspended rather than continuing gameplay-facing sampling.
11. Return to free roam and confirm the gate returns to `Normal` and sampling resumes.
12. Exit the game normally.
13. Confirm `GTA_Crime_Overhaul.log` contains startup/build/persistence information and no unhandled exception.
14. Confirm `world.json` exists with `schemaVersion: 1` and `nextIds`.
15. Relaunch Story Mode and confirm the log reports `LoadedPrimary` (or a diagnosed recovery state) instead of recreating the save every launch.

No Stage-0 robbery gameplay is expected. The objective is loader/runtime/lifecycle/scheduler/persistence stability.

### Corruption recovery test

After making a backup of the test save:

1. launch once so a valid `world.json` exists;
2. make a second valid write/run so `world.backup.json` exists;
3. exit GTA;
4. deliberately corrupt the test `world.json` text;
5. relaunch Story Mode;
6. PASS if the runtime does not crash, logs the recovery, restores the validated backup and preserves a `world.corrupt-*.json` diagnostic copy.

Do this only with a test mod save, never with unrelated Rockstar save files.

## CI

`.github/workflows/build.yml` builds on a Windows x64 GitHub runner.

The workflow:

1. checks out source;
2. validates dialogue data/policy tests already present in the repository;
3. attempts the official Script Hook V developer SDK download first;
4. if that endpoint is unavailable to GitHub Actions, checks out the existing public SDK build mirror at a **pinned immutable commit** for headers/import library only;
5. logs SDK provenance;
6. configures CMake explicitly for x64;
7. builds `Release` and all engine-independent tests;
8. runs CTest;
9. verifies that `GTA_Crime_Overhaul.asi` was produced;
10. stages only project-owned ASI/config/data/docs plus build metadata;
11. rejects forbidden files such as `ScriptHookV.dll`, `dinput8.dll`, `GTA5.exe` and `NativeTrainer.asi`;
12. creates and re-opens the ZIP to verify the GTA-root installation layout;
13. uploads the ZIP, standalone ASI and build metadata.

The fallback SDK repository is not packaged and is not a runtime dependency. Do not solve SDK availability by committing or releasing Script Hook V runtime binaries.

## Release contents

A CI/public release contains project-owned files such as:

```text
GTA_Crime_Overhaul.asi
GTA_Crime_Overhaul/GTA_Crime_Overhaul.ini
GTA_Crime_Overhaul/data/...
README.md
BUILD_AND_RELEASE.md
BUILD_INFO.txt
```

Do not package:

```text
ScriptHookV.dll
dinput8.dll
GTA5.exe
NativeTrainer.asi
Rockstar assets extracted from game archives
RDR2 assets
```

## Legacy / Enhanced testing

A successful x64 build proves compiler/SDK compatibility, not in-game compatibility. Stage 0 must be smoke-tested separately on every target edition available to the tester.

Check especially:

- plugin load/unload;
- logging/path discovery;
- mission/cutscene gate behavior;
- native calls used by the current adapter layer;
- save creation and recovery;
- clean normal exit.

Avoid raw memory offsets unless a future feature is impossible through stable natives. If offsets ever become necessary, isolate them behind per-build adapters and never make them the default design path.

## Troubleshooting order

### ASI does not load

1. Check Script Hook V supports the installed GTA build.
2. Check `ScriptHookV.dll` and the ASI loader installation.
3. Check `GTA_Crime_Overhaul.asi` is in the GTA root.
4. Check Windows security/quarantine.
5. Check other ASI conflicts.

### Log exists but save does not

Check write permissions for the GTA folder and inspect the log for `Unable to initialize world save state` or `Unable to restore logical ID state`.

### Save is reported corrupt

Look in `GTA_Crime_Overhaul/saves/` for:

- `world.backup.json`;
- `world.corrupt-<timestamp>.json`.

Do not delete the diagnostic copy until the cause is understood.

### Game updated

Update Script Hook V first. The plugin intentionally depends on Script Hook V/native APIs rather than patch-specific memory hooks wherever possible.

### Gameplay conflict with a Rockstar mission

This is a bug to handle in `MissionCompatibilityGate`; do not fix it by globally disabling mission detection.
