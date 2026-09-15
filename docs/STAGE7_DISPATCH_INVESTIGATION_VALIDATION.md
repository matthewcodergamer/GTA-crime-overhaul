# Stage 7 — Dispatch, investigation and interviews validation

Roadmap: 129–145.

## Automated / data-verified invariants

- `DispatchDirector` is independent from GTA's built-in wanted-star response.
- Response planning is driven by persisted case severity, related crime types and recorded weapon/gunshot/injury/body evidence.
- `DispatchPlan` has no hidden-player-position input. Its approach target is the recorded crime/business location.
- Player position is used only by `DispatchDirector` to decide whether a recorded scene should have live high-detail entities or remain abstract.
- Investigation interviews are generated per witness `independenceKey`; a witness cannot gain face/vehicle/plate/direction facts that are absent from that witness's recorded evidence.
- Officer interview records retain the original witness source family so repeating a witness statement cannot create artificial independent confidence.
- No GTA ped/entity handle is stored in persistent case evidence or scene reconstruction data.
- Subtitle text is the authoritative interview presentation path. Base-game speech is optional and not required for state transitions.
- Interruption or scene abstraction aborts live interview choreography without committing unfinished interview facts.

## Presentation research status

The current high-detail adapter uses GTA task/scenario candidates only as optional presentation:

- officer/witness facing: GTA task API — **automated compile validation only**
- stationary fallback: GTA stand-still task — **automated compile validation only**
- `WORLD_HUMAN_CLIPBOARD` — **REFERENCE_ONLY** until Legacy and Enhanced in-game validation
- `WORLD_HUMAN_STAND_MOBILE` — **REFERENCE_ONLY** until Legacy and Enhanced in-game validation
- `WORLD_HUMAN_COP_IDLES` — **REFERENCE_ONLY** until Legacy and Enhanced in-game validation
- `s_m_y_cop_01` response model — **REFERENCE_ONLY** until Legacy and Enhanced in-game validation
- ambient police/witness speech names — **not claimed / not required**; do not guess identifiers

If any presentation scenario fails on a model/build, the logical scene and interview continue through facing/stand-still/subtitle fallback.

## Manual Story Mode exit test

Run this separately on supported GTA V Legacy and Enhanced builds. Do not mark `VERIFIED_IN_GAME` until both results are recorded separately.

1. Start Story Mode/offline in normal controllable free roam with debug logging enabled.
2. Rob the validated prototype store and allow a witness/alarm report to complete.
3. Leave the area before the response arrives. Break visual contact and move somewhere the crime scene is no longer in high-detail range.
4. Confirm police do **not** navigate toward the hidden player's current GPS position. The case response target must remain the recorded store/crime location.
5. Wait past the logical response delay, then return to the store before scene expiry.
6. Confirm a lightweight police scene reconstructs at the store: officers approach the reported location, establish temporary perimeter positions and begin investigation tasks.
7. When corresponding recorded evidence exists, verify officers can be tasked to check interior anchors, body/injury locations, an involved/abandoned vehicle location and known exits. Missing evidence must not invent a target.
8. Verify at least one surviving witness who completed/partially completed a report can be selected for interview. Kill/despawn or otherwise remove another candidate and confirm it is not interviewed and the runtime does not crash.
9. Compare the interview against the case/witness debug record. A witness with no face evidence must not claim a face; no vehicle evidence must produce no positive vehicle claim; no plate evidence must produce no positive plate claim; direction must only be stated when recorded.
10. Confirm every spoken turn has a subtitle fallback. Disable/break optional presentation conditions if possible; logical interview progression must not require a cinematic, scenario or ambient speech line.
11. Confirm officer and witness use robust facing/stand/idle presentation. Clipboard/phone/radio scenarios may fail gracefully without freezing case progression.
12. Travel farther than the high-detail radius. Confirm project-created police peds are cleaned and the scene becomes abstract without deleting the case.
13. Return before the six-minute scene lifetime expires. Confirm the scene reconstructs at the same crime location, not at the player's return route/current coordinates.
14. Trigger a mission/cutscene/restricted-control transition while the scene is live. Confirm high-detail police/interview choreography cleans or suspends safely. Return to free roam before expiry and confirm reconstruction remains possible.
15. Let the scene expire. Confirm no project-owned police peds remain and immediate scene response becomes inactive. Confirm the later business-recovery event does not wipe unrelated case/evidence state.
16. Save/reload while the case remains eligible for investigation. Confirm the abstract scene can be derived again from the persistent case/crime location and does not require stale GTA entity handles.
17. Repeat with a more severe case (shots/injury/death/officer assault) and confirm the response tier/tactical level increases without retargeting the player.

## Required pass evidence

Record for each edition:

- game edition/build
- Script Hook V version
- response target coordinates versus case crime coordinates
- response tier and triggering case facts
- high-detail arrival/perimeter/inspection result
- interview source key and facts shown
- subtitle fallback result
- far-distance abstraction result
- return/reconstruction result
- mission/cutscene interruption result
- expiry/cleanup result
- crash/hang/leaked-ped result

Until those checks are completed, Stage 7 native presentation/streaming behavior remains **not VERIFIED_IN_GAME** even when Windows x64 CI is green.
