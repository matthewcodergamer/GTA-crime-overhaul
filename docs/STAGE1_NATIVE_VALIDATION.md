# Stage 1 — Shared World Abstraction Validation

Roadmap scope: **31–46**.

This file separates what is confirmed by Script Hook V/native reference data from what still requires actual GTA V Legacy/Enhanced execution. `VERIFIED_DATA` never means visual/runtime behavior has already been proven in both editions.

## Acceptance requirement

Stage 1 is complete only when game-specific native details are hidden behind adapters so robbery/law domain code consumes semantic interfaces/value snapshots rather than native signatures, control IDs or trace bitmasks, and the adapters survive invalid/deleted entities and mission/cutscene suspension.

The implementation boundary is `src/platform/`. Native GTA calls live in `PlatformAdapters.cpp`; debug-only wrapper exercise code lives in `AdapterDiagnostics.cpp`. The robbery/law domain must not include NativeDB/Script Hook V headers.

## Dependency status

| Area | GTA dependency | Status before in-game smoke test | Fallback / failure behavior |
|---|---|---|---|
| Player state | `PLAYER_PED_ID`, `PLAYER_ID`, `GET_PLAYER_WANTED_LEVEL`, `IS_PLAYER_DEAD`, `IS_PLAYER_CONTROL_ON` | VERIFIED_DATA | Invalid player ped returns no snapshot/alive=false. |
| Mission gate | `GET_MISSION_FLAG`, `IS_CUTSCENE_ACTIVE`, `IS_CUTSCENE_PLAYING` | VERIFIED_DATA | World sampling clears cached live snapshots and suspends gameplay-facing work. |
| Entity safety | `DOES_ENTITY_EXIST`, entity type checks, coords/model/heading | VERIFIED_DATA | Missing/deleted entities return false/nullopt. |
| Bounded world queries | Script Hook V `worldGetAllPeds`, `worldGetAllVehicles` | VERIFIED_DATA | Results are radius filtered, hard capped, nearest-first; zero/negative/non-finite radii and zero result limits fail closed. |
| Spatial prefilter | project `distanceSquared`, `withinRadius`, `gridCellFor` | VERIFIED_DATA + UNIT_TESTED | Invalid cell size returns neutral cell; no raycast is needed for prefilter. |
| LOS signature | `HAS_ENTITY_CLEAR_LOS_TO_ENTITY(Entity, Entity, int)` | VERIFIED_DATA | Missing source/target or unknown semantic profile returns false. |
| LOS default profile | public native reference documents collider flag mask `17` as the common GTA-script value | REFERENCE_ONLY until in-game comparison | Domain code sees `LineOfSightProfile::DefaultVisibility`, never the native bitmask. If scene behavior is wrong, change the adapter mapping only. |
| Ped snapshot | model/coords/heading, component and prop variation natives, dead/ragdoll/player state | VERIFIED_DATA | Invalid ped returns nullopt; snapshots contain values only, never handles. |
| Vehicle snapshot | model/coords/heading, colors, plate text/style | VERIFIED_DATA | Invalid vehicle returns nullopt; ambient vehicle has no project persistent ID. |
| Animation dictionary lifecycle | `DOES_ANIM_DICT_EXIST`, `REQUEST_ANIM_DICT`, `HAS_ANIM_DICT_LOADED`, `REMOVE_ANIM_DICT` | VERIFIED_DATA | Unknown dictionary fails immediately; load waits only until caller timeout. Positive asset test is RESEARCH_REQUIRED. |
| Task recovery | `STOP_ANIM_TASK`, `CLEAR_PED_TASKS`, `CLEAR_PED_SECONDARY_TASK`, `CLEAR_PED_TASKS_IMMEDIATELY` | VERIFIED_DATA | Invalid/deleted ped fails without issuing task natives. Positive cancellation is tested only in an explicit animation lab so the generic probe does not disrupt player/NPC state. |
| Prop attachment | `ATTACH_ENTITY_TO_ENTITY`, `DETACH_ENTITY`, `DELETE_OBJECT` | VERIFIED_DATA | Invalid entities fail; project-owned objects can be adopted into a tracker and are detached/deleted on explicit deletion or platform shutdown cleanup. Positive model/bone/offset test is RESEARCH_REQUIRED. |
| Interior | `GET_INTERIOR_FROM_ENTITY`, `IS_VALID_INTERIOR`, `IS_INTERIOR_READY`, `REFRESH_INTERIOR` | VERIFIED_DATA | Invalid entity/interior is ignored. Player-current interior can be queried by the debug probe. |
| Doors | door-system existence/add/remove and closest-door-state natives | VERIFIED_DATA | Zero hashes rejected; exact target doors remain RESEARCH_REQUIRED until a robbery location is surveyed. |
| Blips | add/remove/style/name natives | VERIFIED_DATA | Zero/nonexistent blip ignored and handle reset on removal. Debug probe creates/names/removes a temporary blip. |
| Subtitle/help | GTA text-command natives | VERIFIED_DATA | Empty text ignored. Debug probe emits one short subtitle for manual confirmation. |
| Ambient speech | context check + ambient-speech playback + stop-current-speech natives | VERIFIED_DATA | Invalid ped/empty semantic selection returns false. Positive speech is RESEARCH_REQUIRED until a compatible speech/voice pair is validated; Stage 1 claims no voiced content support. |
| Input | GTA PAD control layer; current semantic mappings are internal to `NativeInputAdapter` | VERIFIED_DATA | Domain code sees only `InputAction`. Invalid `InputAction::Count` fails closed instead of aliasing control 0. Hardware feel still needs manual confirmation. |
| Spatial debug | `DRAW_LINE`, `DRAW_BOX` | VERIFIED_DATA | Debug-only. Probe arms a 5-second line/box/cone display near the streamed player. |
| Models/animation clip names/speech names/interior coordinates/door hashes/bones | none required by Stage 1 production path | RESEARCH_REQUIRED / intentionally absent | Later feature stages must validate exact content rather than guessing now. |

## Debug command / test paths

All probe commands are registered in `DebugCommandRegistry`; they are intentionally separate from gameplay logic.

| Command | Wrapper exercised | Expected safe behavior |
|---|---|---|
| `adapter.world` | player/entity state, bounded ped/vehicle queries, snapshots, LOS | Logs PASS/FAIL plus a manual LOS result against a nearby streamed entity. |
| `adapter.animation` | dictionary request/timeout/release and task cancellation guards | Uses a deliberately invalid sentinel to prove fail-closed behavior; positive asset path reports `RESEARCH_REQUIRED`. |
| `adapter.props` | ownership tracker, attach/detach/delete guards | Exercises invalid-handle cleanup; positive model/bone attachment reports `RESEARCH_REQUIRED`. |
| `adapter.interiors` | current interior and door guards | Reads current player interior when available; zero door/model definitions fail closed; real target door reports `RESEARCH_REQUIRED`. |
| `adapter.ui` | blip/name/remove and subtitle | Creates/removes a temporary blip and emits a short subtitle. |
| `adapter.audio` | ambient-speech validation path | Empty/unvalidated speech is rejected; positive voice/speech pair reports `RESEARCH_REQUIRED`. |
| `adapter.input` | semantic keyboard/controller actions | Arms a 10-second probe and logs semantic actions when pressed. |
| `adapter.debug_draw` | line/box/cone rendering | Arms a 5-second spatial visualization. |
| `adapter.all` | all Stage 1 wrappers | Runs the complete safe suite. **F8** invokes this command when `DebugHotkeys=true`. |

Probe result vocabulary:

- `PASS` — safe behavior automatically observed.
- `FAIL` — wrapper invariant failed.
- `SKIP` — current scene lacks a required live entity/interior; retry in an appropriate scene.
- `MANUAL_REQUIRED` — the call executed but a human must compare the result with the visible GTA scene/device input.
- `RESEARCH_REQUIRED` — exact asset/door/bone/speech content is intentionally absent and must be validated before a positive test is allowed.

The probe suite never mutates case/economy data, never starts a robbery, never guesses a door hash/bone/speech name, and is blocked by the mission compatibility gate outside normal controllable free roam.

## Manual GTA smoke test — run separately on Legacy and Enhanced

Before testing, back up the GTA V folder/save if desired, install the current compatible Script Hook V runtime/ASI loader, install this branch build, and set `DebugHotkeys=true`; optionally set `DebugOverlay=true` in `GTA_Crime_Overhaul/GTA_Crime_Overhaul.ini`.

### A. Complete adapter probe in populated free roam

1. Load Story Mode into normal controllable free roam as Franklin, Michael or Trevor.
2. Stand on a populated street with pedestrians and vehicles.
3. Press **F8** once.
4. Confirm the log contains `Adapter probe` entries for world, animation, props, interiors/doors, UI, audio, input and debug-draw paths.
5. PASS if no entry is `FAIL`.
6. `RESEARCH_REQUIRED` for positive animation/prop/door/audio content is expected at Stage 1 and is not a failure.
7. Confirm the temporary subtitle appears.
8. Confirm a short line/box/cone visualization appears near the player for about five seconds.
9. During the ten-second input window, press the mapped gameplay actions and confirm semantic names such as `Interact`, `Aim`, or `EnterVehicle` appear in the log.

### B. Bounded queries and streaming loss

1. Enable the normal debug overlay with **F10**.
2. Walk/drive between a populated street and a quieter area.
3. Confirm nearby counts update and never exceed 64 for either category.
4. Drive rapidly far enough to stream population out/in or use a normal character transition.
5. Press F8 again after the world stabilizes.

**Pass:** no crash, stale-handle behavior, runaway counts or repeated errors; streamed-away candidates become SKIP/null rather than corrupt state.

### C. Wanted/player state

1. Acquire a normal Story Mode wanted level.
2. Confirm the overlay follows GTA's current wanted value.
3. Lose/clear it and confirm it returns to zero.

**Pass:** the wrapper reflects GTA state; Stage 1 does not itself add/remove wanted stars.

### D. Mission/cutscene suspension

1. Start a Rockstar mission/cutscene in which the mission flag/cutscene/control state becomes incompatible.
2. Confirm the overlay reports the compatibility gate and nearby sampling pauses.
3. Press F8 while gated; the log should say the adapter probe is blocked rather than touching gameplay-facing wrappers.
4. Return to controllable free roam.
5. Confirm the gate returns to normal and F8 probes work again without restarting GTA.

### E. Interior/door research path

1. Run F8 outdoors: `interior.player` may legitimately be `SKIP`.
2. Enter a known normal Story Mode interior and run F8 again.
3. Confirm an interior ID is resolved and readiness is reported.
4. Do **not** promote any robbery door to production based on this alone. A real target door needs CodeWalker/in-game survey data and its exact model/hash/coordinate in the owning target data file.

### F. Asset-dependent positive paths

Animation dictionary/clip, attached prop model/bone/offset and ambient speech/voice pairs remain separate research labs. Promote an item from `RESEARCH_REQUIRED` only after recording a `VERIFIED_IN_GAME` result with edition/build/date and a cleanup/fallback result.

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

Until those are filled, Stage 1 can be build-validated and architecturally complete, but its GTA-facing behavior is not `VERIFIED_IN_GAME`.
