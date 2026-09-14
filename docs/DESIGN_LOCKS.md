# GTA Crime Overhaul — Design Locks

These are project-level decisions. Change them deliberately; do not let later implementation shortcuts silently replace them.

## 1. Product scope

1. This is a **single-player / Story Mode** GTA V overhaul.
2. The project does not target GTA Online and must not attempt to bypass Script Hook V's multiplayer protections.
3. The finished runtime is a native x64 C++ `.asi` named `GTA_Crime_Overhaul.asi`.
4. GTA V Legacy and Enhanced are both compatibility targets where Script Hook V supports them.
5. LSPDFR/RAGEPluginHook may receive optional adapters later but are not required dependencies.

## 2. Core fantasy

The canonical loop is:

**Prepare → disguise → choose owned vehicle → choose carried/trunk equipment → case target → commit crime → physically collect loot → witness/report/investigation → pursuit/search → identity/vehicle countermeasures → secure loot → spend/reinvest → larger crime.**

A robbery is not complete when wanted stars vanish. A crime may continue as a persisted case.

## 3. Crime simulation

- All supported crimes feed one `CrimeDirector`/case pipeline.
- The law system distinguishes **immediate GTA wanted response** from **persistent case/warrant state**.
- Police do not receive perfect player GPS from our simulation.
- Reports contain only information an actual witness, camera, officer or discovered vehicle/evidence source could plausibly know.
- Person recognition and vehicle recognition are separate tracks.
- An unknown suspect may remain unknown after police arrive.
- If the suspect is identified, escaping the chase does not automatically clear that identity.
- Severity escalates from nonviolent robbery to shots fired, homicide, officer injury and mass-casualty events.
- Surrender/arrest is a valid outcome and can resolve or alter cases.

## 4. Witnesses and memory

- Nearby NPCs can independently see/hear different parts of an event.
- Witness observations are confidence-based and may be incomplete.
- Important observations include face, mask, clothing, weapon class, vehicle model, vehicle color, plate, direction and location.
- Witnesses may freeze, hide, flee, call police, hit a panic alarm, shout, cooperate with police or fail to report.
- Killing a witness does not globally erase the crime; cameras, other witnesses, bodies, vehicles and business records may remain.
- Persistent named/role-bound clerks can remember previously observed robbers.
- If a clerk dies, that individual's personal memory dies; the business incident history does not.

## 5. Identity, masks and disguises

- A mask/bandana primarily blocks or reduces **new face identification**.
- Putting on a mask after a witness has already seen the player's face does not erase the observation.
- Clothing, vehicle and plate observations remain possible while masked.
- Clothing changes reduce a clothing match but do not clear a confirmed face ID.
- Vehicle swaps reduce vehicle continuity only if police/witnesses do not observe the swap.
- Repainting a car changes color evidence but not model/plate/history unless those are also changed.
- RDR2-inspired mechanics may be reproduced, but RDR2 game assets are not redistributed.

## 6. License plate system

- Owned vehicles persist a plate string and plate style/index.
- Garages/mod shops can offer a **Change Plate** service.
- A plate change has an economy cost and updates the persistent vehicle record.
- Changing the plate reduces future plate matching, but does not erase evidence already connecting a known owner/vehicle to a crime if that relationship was established by stronger evidence.
- Police cases store the **plate observed at the time**, not a magical pointer that updates when the player changes it later.
- Stolen/temporary getaway vehicles can be treated differently from registered owned vehicles.

## 7. Duffel/loot system

- Bags are gameplay inventory, not just cosmetics.
- `No Bag`, `Small`, `Standard` and `Large` capacity tiers are planned.
- Larger bags allow the player to remove **more money/valuables** from a target before leaving.
- More loot takes longer to collect and can increase movement/escape burden where animation/gameplay allows.
- Loot is not deposited instantly. It remains `Carried`, `Dropped`, `VehicleStored`, `Stashed`, `Seized`, `Lost`, or `Secured`.
- A player can abandon a loaded bag and potentially recover it while it remains simulated.
- Police can seize unsecured robbery proceeds.

## 8. Robbery philosophy

- Robberies are primarily **systems-driven**, not mission-marker scripts.
- Entering a target normally must not automatically start a mission.
- A robbery can emerge from actions such as presenting a weapon, issuing a demand, forcing access, opening a safe/vault or otherwise crossing a crime threshold.
- One excellent 24/7 store is built before scaling to all stores.
- Banks progress through Fleeca-scale jobs, Pacific Standard-scale jobs and only then major-score targets.
- Casing is diegetic where possible: the player learns entrances, guards, cameras, vault access and exits by visiting/observing.

## 9. Stores and businesses

- Clerks have behavior/personality variation.
- Repeat robberies alter a business's persistent state.
- Possible escalation includes faster alarm response, cameras, guards, upgraded safe behavior and reduced exposed cash.
- Returning later can show police activity or a recovered business state rather than an instant world reset.

## 10. Police investigation

- Police can arrive after the player has left.
- Investigation scenes can include officers approaching the business, speaking to surviving witnesses, checking bodies/vehicles and maintaining a temporary scene.
- Police dialogue should reflect real case data where possible.
- Search logic uses last-known-position/heading/vehicle information rather than teleporting knowledge to the player's current position.
- Arrest can cause confiscation, fines, impound and loss of unsecured loot.

## 11. Vehicles

- Owned vehicles are persistent entities with a project-level ID.
- Persistent vehicle state includes model, paint, plate, storage, damage/insurance state, police flags and robbery history where appropriate.
- A personal car is meaningful and can become a liability if used repeatedly in crime.
- Garages support vehicle storage and lawful/illicit services such as repairs, paint and plate changes depending on design stage.
- Impounded vehicles can retain trunk inventory unless later balance rules say otherwise.
- Destroying a vehicle does not automatically restore illegal trunk contents through insurance.

## 12. Weapons and trunk inventory

- The project aims to reduce GTA V's unlimited-pocket arsenal.
- On-person carry is limited to a practical subset (for example sidearm + primary + optional secondary/large slot + throwables).
- Additional weapons, armor, masks, bags, ammo and robbery equipment may be stored in vehicle trunks.
- Safehouses/garages provide larger storage.
- We will use GTA V-compatible animations/tasks for trunk interaction; where a perfect paired animation does not exist, choose a believable fallback rather than promising impossible bespoke motion.

## 13. Economy

Robbery money must have reasons to exist. Planned sinks include:

- owned cars and compatible DLC/Online vehicles available in the installed build
- garages and safehouses
- weapons/ammunition/armor
- masks/disguises/clothing
- bags and storage upgrades
- vehicle repair/paint/plate changes
- legal/arrest/impound costs
- robbery equipment and preparation
- intelligence/crew assistance in later phases

Large scores may distinguish immediately spendable cash from `hot/marked` proceeds that must be secured/processed before full use.

## 14. UI and presentation

- HUD is minimal, readable and Rockstar-like, not a large neon trainer menu.
- Debug UI is separate from player-facing UI and can be disabled.
- Planned player-facing concepts include duffel capacity, witness/report alerts, investigation status and active warrant/vehicle BOLO information.
- Do not copy GTA VI proprietary UI assets. Reproduce design principles with original implementation.

## 15. Dialogue and audio

- Dialogue is data-driven by semantic tags and state.
- Existing GTA V ambient speech lines may be used only where the game exposes suitable content.
- New lines may use original recorded audio or properly licensed/original TTS voices.
- The project does **not** depend on cloning GTA actors' voices.
- When no suitable voiced line exists, subtitles + generic GTA vocal reaction/gesture are an acceptable fallback.
- Police interviews are gameplay data exchanges first; voice presentation is layered on top.

## 16. Animation constraints

- Prefer GTA V's existing task system, scenario system and animation dictionaries.
- Reuse generic hands-up, intimidation, phone, register/safe, carry, pickup, trunk/vehicle and interaction animations when suitable.
- Attaching a duffel/prop to a ped is feasible; perfect cloth deformation or exact GTA VI bag behavior is not a baseline promise.
- New custom animation assets are a later optional content path, not required for core simulation.
- Every animation sequence must have timeouts and state recovery so a missing dictionary/blocked ped cannot deadlock a robbery.

## 17. Performance

- Crime creation is event-driven.
- Candidate witness scans are low-frequency and spatially bounded.
- Expensive LOS/raycast checks are staggered.
- Off-screen businesses/cases are abstract records, not always-live NPC simulations.
- Police/investigation planners run at low fixed frequency; per-frame work is limited mainly to immediate input/UI/critical entity checks.
- Never scan all peds/vehicles every frame.

## 18. Persistence safety

- Save files use explicit schema versions.
- Writes use temp-file + atomic replace semantics where possible.
- Unknown/new fields should not destroy older saves.
- Corrupt persistence falls back safely with diagnostics rather than crashing the game.
- Runtime handles missing/deleted GTA entities by project IDs and reacquisition rules, not raw handles persisted across sessions.

## 19. Mission compatibility

- Detect story missions/cutscenes/interiors where our systems would interfere.
- Suspend or degrade nonessential systems during incompatible Rockstar mission flow.
- Never hijack Rockstar mission peds/vehicles blindly.
- Active Crime Overhaul state must be resumable or safely cancelled when mission compatibility requires it.

## 20. Delivery strategy

- Build vertically in stages.
- Every phase has exit criteria and debug instrumentation.
- No phase is marked complete merely because UI exists; the underlying state must persist and recover.
- GitHub Actions should produce the `.asi` build artifact and release package once the official SDK is available to CI.
