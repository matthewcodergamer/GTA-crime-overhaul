# Stage 1 — Shared World Abstraction Validation

Roadmap scope: **31–46**.

This file separates what is confirmed by Script Hook V/native reference data from what still requires actual GTA V Legacy/Enhanced execution. `VERIFIED_DATA` never means visual/runtime behavior has already been proven in both editions.

## Acceptance requirement

Stage 1 is complete only when game-specific native details are hidden behind adapters so robbery/law domain code can consume stable value snapshots/interfaces, and the adapters survive invalid/deleted entities and mission/cutscene suspension.

## Dependency status

| Area | GTA dependency | Status before in-game smoke test | Fallback / failure behavior |
|---|---|---|---|
| Player state | `PLAYER_PED_ID`, `PLAYER_ID`, `GET_PLAYER_WANTED_LEVEL`, `IS_PLAYER_DEAD`, `IS_PLAYER_CONTROL_ON` | VERIFIED_DATA | Invalid player ped returns no snapshot/alive=false. |
| Mission gate | `GET_MISSION_FLAG`, `IS_CUTSCENE_ACTIVE`, `IS_CUTSCENE_PLAYING` | VERIFIED_DATA | World sampling clears cached live snapshots and suspends gameplay-facing work. |
| Entity safety | `DOES_ENTITY_EXIST`, entity type checks, coords/model/heading | VERIFIED_DATA | Missing/deleted entities return false/nullopt. |
| Bounded world queries | Script Hook V `worldGetAllPeds`, `worldGetAllVehicles` | VERIFIED_DATA | Results are radius filtered, hard capped, nearest-first; zero/negative limits fail closed. |
| Spatial prefilter | project `distanceSquared`, `withinRadius`, `gridCellFor` | VERIFIED_DATA + UNIT_TESTED | Invalid cell size returns neutral cell; no raycast is needed for prefilter. |
| LOS | `HAS_ENTITY_CLEAR_LOS_TO_ENTITY` | VERIFIED_DATA | Missing source/target returns false; no omniscient fallback. |
| Ped snapshot | model/coords/heading, component and prop variation natives, dead/ragdoll/player state | VERIFIED_DATA | Invalid ped returns nullopt; snapshots contain values only, never handles. |
| Vehicle snapshot | model/coords/heading, colors, plate text/style | VERIFIED_DATA | Invalid vehicle returns nullopt; ambient vehicle has no project persistent ID. |
| Animation dictionary lifecycle | `DOES_ANIM_DICT_EXIST`, `REQUEST_ANIM_DICT`, `HAS_ANIM_DICT_LOADED`, `REMOVE_ANIM_DICT` | VERIFIED_DATA | Unknown dictionary fails immediately; load waits only until caller timeout. |
| Task recovery | `STOP_ANIM_TASK`, `CLEAR_PED_TASKS`, `CLEAR_PED_SECONDARY_TASK`, `CLEAR_PED_TASKS_IMMEDIATELY` | VERIFIED_DATA | Invalid/deleted ped fails without issuing task natives. |
| Prop attachment | `ATTACH_ENTITY_TO_ENTITY`, `DETACH_ENTITY`, `DELETE_OBJECT` | VERIFIED_DATA | Invalid entities fail; owned cleanup detaches/deletes if still valid and always zeroes runtime handle. |
| Interior | `GET_INTERIOR_FROM_ENTITY`, `IS_VALID_INTERIOR`, `IS_INTERIOR_READY`, `REFRESH_INTERIOR` | VERIFIED_DATA | Invalid entity/interior is ignored. |
| Doors | door-system existence/add/remove and closest-door-state natives | VERIFIED_DATA | Zero hashes rejected; exact target doors remain NOT-YET-RESEARCHED until a robbery location is surveyed. |
| Blips | add/remove/style/name natives | VERIFIED_DATA | Zero/nonexistent blip ignored and handle reset on removal. |
| Subtitle/help | GTA text-command natives | VERIFIED_DATA | Empty text ignored. |
| Ambient speech | ambient-speech native + stop-current-speech native | VERIFIED_DATA | Invalid ped/empty semantic selection returns false; no dialogue content is hardcoded by Stage 1. |
| Input | GTA control natives and control IDs 51/177/21/25/24/23 | VERIFIED_DATA | Uses GTA control layer so keyboard/controller remapping remains GTA-owned; mapping needs in-game confirmation. |
| Spatial debug | `DRAW_LINE`, `DRAW_BOX` | VERIFIED_DATA | Debug-only and disabled unless `DebugOverlay=true`. |
| Models/animation clip names/speech names/interior coordinates/bones | none required by Stage 1 production path | NOT-YET-RESEARCHED / intentionally absent | Later feature stages must validate exact content rather than guessing now. |

## Manual GTA smoke test — run separately on Legacy and Enhanced

Before testing, back up the GTA V folder/save if desired, install the current compatible Script Hook V runtime/ASI loader, install this branch build, and enable `DebugOverlay=true` in `GTA_Crime_Overhaul/GTA_Crime_Overhaul.ini`.

### A. Normal free roam

1. Load Story Mode into normal controllable free roam as Franklin, Michael or Trevor.
2. Confirm `GTA_Crime_Overhaul.log` reports successful initialization and no repeating exception/error.
3. Confirm the help text shows `GCO Stage 1`, current wanted level, nearby ped count and nearby vehicle count.
4. Walk/drive between a populated street and a quieter area.
5. Confirm counts update without visible stutter and never exceed 64 for either category.
6. Confirm a white debug FOV triangle/cone follows player position/heading while the overlay is enabled.
7. Disable `DebugOverlay`; confirm help text/debug geometry disappears while the ASI remains loaded.

**Pass:** adapter diagnostics update, remain bounded, and disabling debug presentation removes presentation only.

### B. Wanted/player state

1. Enable the overlay again.
2. Acquire a normal Story Mode wanted level.
3. Confirm displayed wanted value follows GTA.
4. Lose/clear the wanted level and confirm it returns to zero.

**Pass:** the wrapper reflects GTA state; Stage 1 must not itself add/remove wanted stars.

### C. Mission/cutscene suspension

1. Start a Rockstar mission or cutscene in which the mission flag/cutscene state becomes active.
2. Confirm the overlay changes to `SUSPENDED (mission/cutscene)` and nearby counts are no longer sampled.
3. Finish/exit the mission/cutscene and return to controllable free roam.
4. Confirm the log records suspension then resume and sampling returns without restarting GTA.

**Pass:** Crime Overhaul yields during restricted state and safely reacquires player/world state afterward.

### D. Streaming/entity-loss failure path

1. In populated free roam, use normal fast travel/character transition or drive rapidly far enough to stream nearby population out/in.
2. Observe the overlay/log through the transition.
3. Optionally use an offline trainer only as a robustness aid to teleport a large distance; this is not a runtime dependency.

**Pass:** no crash, stale handles, runaway counts or repeated errors; snapshots recover after streaming.

### E. Input/controller smoke test

The Stage 1 input adapter has no gameplay action wired yet, so use a temporary debugger/breakpoint or the next interaction stage to confirm mappings before relying on them. The numeric IDs are `VERIFIED_DATA`, but controller/keyboard feel remains pending `VERIFIED_IN_GAME` status.

## Edition result fields

Fill these only after real testing:

```text
Legacy build/version: ____________________
Date: ___________________________________
Result: PASS / FAIL
Notes: __________________________________

Enhanced build/version: _________________
Date: ___________________________________
Result: PASS / FAIL
Notes: __________________________________
```

Until those are filled, Stage 1 code may be build-validated but its GTA-facing dependencies remain **not VERIFIED_IN_GAME**.
