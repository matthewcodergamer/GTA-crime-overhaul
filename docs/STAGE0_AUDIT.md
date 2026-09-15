# Stage 0 Audit — Repository, Native ASI and Engineering Foundation

Scope: `docs/MASTER_PLAN.md` steps **1–30** only.

This audit distinguishes code/build completion from the final in-game exit criterion. A green CI build does not prove that Script Hook V loaded the ASI in a particular GTA V edition.

## Step audit

| Step | Requirement | Repository status after audit |
|---:|---|---|
| 1 | Repository + Story Mode/offline scope | Present in README/design locks/runtime logging. |
| 2 | `GTA_Crime_Overhaul.asi` authoritative binary | Enforced by CMake/build constants/CI. |
| 3 | `DESIGN_LOCKS.md` | Present and authoritative. |
| 4 | `FEASIBILITY_MATRIX.md` | Present. |
| 5 | Native C++20 structure | Present; CMake requires C++20. |
| 6 | Script Hook V SDK discovery | CMake path/env discovery with header/import-lib checks. |
| 7 | Never redistribute Script Hook V/ASI loader | `.gitignore`, docs and CI package-content rejection enforce this. |
| 8 | x64 Release/Debug configurations | x64 required; multi-config CMake limited to Debug/Release. |
| 9 | `.asi` suffix | CMake target suffix is `.asi`. |
| 10 | `scriptRegister`/`scriptUnregister` lifecycle | Present in `main.cpp`, with runtime stop signal before unregister. |
| 11 | `ScriptMain` + `WAIT(0)` | Runtime loop yields with `scriptWait(0)`. |
| 12 | Timestamped/severity logging | Present; logger flushes records to the project log. |
| 13 | Top-level/tick exception containment | ScriptMain guard plus per-scheduler-task guard. |
| 14 | Project/save/build constants | `BuildInfo` contains version, binary name, build config, Git SHA and schema version. |
| 15 | Paths relative to GTA executable | `RuntimePaths::discover()` derives game/mod/config/data/save/log roots from the executable directory. |
| 16 | Project config | INI with runtime/system/debug toggles. |
| 17 | JSON persistence abstraction/no raw handles | `WorldStateStore` owns persistence; validation rejects known raw-handle keys. |
| 18 | Atomic save | temp write -> validation -> write-through replace. |
| 19 | Corruption fallback/backup | validated backup, corrupt-primary quarantine, backup restore, clean-reset fallback. |
| 20 | Deterministic logical IDs | domain-prefixed 64-bit logical IDs and persisted `nextIds` counters. |
| 21 | Lightweight event bus | synchronous topic event bus with subscription tokens and unsubscribe/clear. |
| 22 | Scheduler lanes + event queue | frame/5 Hz/2 Hz/1 Hz lanes plus queued event-driven tasks; per-task exception containment. |
| 23 | Mission/cutscene compatibility gate | explicit `Normal`, `Restricted`, `Suspended` gate driven by adapter mission signals. |
| 24 | Debug commands/diagnostics | registry + F9 diagnostics, F10 overlay, F11 save validation; feature can be disabled in config. |
| 25 | Windows x64 GitHub Actions | present. |
| 26 | CI obtains SDK rather than redistributing | official download attempted first; pinned mirror fallback is build-only and never packaged. |
| 27 | Package only project files | CI stages project ASI/config/data/docs/build metadata and rejects forbidden runtime/game files. |
| 28 | GTA-root ZIP layout | CI re-opens ZIP and validates root ASI + project folder/config. |
| 29 | Install/troubleshooting docs | `BUILD_AND_RELEASE.md` updated with build, install, diagnostics, persistence recovery and edition smoke tests. |
| 30 | In-game exit criterion | **Requires manual GTA V Story Mode smoke test.** CI can prove binary/build/persistence logic but cannot prove the ASI actually loads/unloads inside the target GTA executable. |

## Automated Stage-0 acceptance

The branch is acceptable for merge only when GitHub Actions proves all of the following in `Release` x64:

- dialogue/data gates already used by the repository pass;
- CMake config succeeds with the resolved Script Hook V SDK headers/import library;
- `GTA_Crime_Overhaul.asi` compiles;
- platform/core/persistence tests pass;
- the release ZIP contains the correct root-install layout;
- the ZIP contains no `ScriptHookV.dll`, `dinput8.dll`, `GTA5.exe` or bundled trainer.

## Manual Stage-0 exit gate

Stage 0 is fully complete only after an actual target GTA V Story Mode run demonstrates:

1. Script Hook V loads `GTA_Crime_Overhaul.asi`.
2. Startup log is created with version/build/schema/provenance information.
3. Runtime ticks without an unhandled exception.
4. `world.json` is created/loaded with `schemaVersion: 1` and logical-ID state.
5. F9/F10/F11 diagnostics work when debug hotkeys are enabled.
6. Mission/cutscene transitions restrict/suspend and later resume gameplay-facing work.
7. Normal game exit/unload does not leave the runtime in a crashing/hanging state.
8. A second launch loads the existing save rather than silently replacing it.
9. Corrupt-primary recovery restores a valid backup or safely creates a clean save with a quarantined corrupt copy.

Legacy and Enhanced must be recorded separately. If only one edition is available for testing, mark the other edition unverified rather than assuming parity.
