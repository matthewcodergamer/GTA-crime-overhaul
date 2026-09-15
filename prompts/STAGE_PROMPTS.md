# GTA Crime Overhaul — Stage-by-Stage Implementation Prompts

These prompts are designed to be pasted into an AI coding agent one stage at a time. Always prepend or reference `prompts/MASTER_AGENT_PROMPT.md`. Do not skip a stage's exit criteria.

## Stage 0 — Foundation (steps 1–30)

**Prompt**

Read `prompts/MASTER_AGENT_PROMPT.md` and all required docs. Audit Stage 0, steps 1–30. Bring the native C++20 Script Hook V project to a reproducible x64 `.asi` build. Ensure lifecycle registration/unregistration, scheduler, config, timestamped logging, version constants, schema-versioned persistence, atomic/backup saves, logical IDs, event bus, mission compatibility gate, debug diagnostics, GitHub Actions build and release package are present and robust. Do not package Script Hook V or an ASI loader. Build Release and report any remaining item preventing the Stage 0 exit criteria.

## Stage 1 — Shared GTA world adapters (31–46)

**Prompt**

Implement roadmap 31–46. Create/finish a GTA adapter layer for player state, safe entity access, bounded nearby queries, snapshots, animation loading/timeouts, prop attachment cleanup, doors/interiors, HUD, subtitles, audio/speech, input and spatial debug rendering. Raw native calls should not leak into robbery/law domain code. Add a debug command/test path for each risky wrapper. Never guess a native signature. Mark asset-dependent items research-required where appropriate. Exit only when game-specific details can change without rewriting domain logic.

## Stage 2 — Crime domain and persistent cases (47–59)

**Prompt**

Implement roadmap 47–59 with no store-specific behavior. Build `CrimeDirector`, `CrimeRegistry`, crime types/severity, immediate-vs-long-term state, canonical case state transitions, immutable evidence records with source/confidence/timestamp, merge/decay rules, persistence and a debug case inspector. Add unit tests for state transitions, evidence merging, save/load and schema behavior. Generate synthetic debug crimes to prove the system without GTA shop logic.

## Stage 3 — One perfect store (60–76)

**Prompt**

Implement only the first convenience-store vertical slice, roadmap 60–76. Do not scale to every store. Use a data-defined prototype target whose coordinates/anchors are validated in-game. Normal entry must not start a robbery. Detect a believable threat threshold, create a robbery session, persistent logical clerk, personality variants, hands-up/cower/flee/phone/alarm/armed branches, register demand, partial/lying behavior, cash-source state, timeout/abort and complete cleanup if the clerk dies, streams out, animation fails or GTA AI interrupts. Use only verified animations/tasks or documented fallbacks.

## Stage 4 — Witness perception/reporting (77–95)

**Prompt**

Implement roadmap 77–95. Build bounded/staggered witness candidate scans and separate hearing from visual identification. Capture face, mask, outfit, weapon class, vehicle model/color, plate and last-known direction only when observation rules support them. Confidence must depend on view quality/time and not an omniscient radius. Add fear/panic/defiance and reporting states with delays/interruption. Preserve important observations into the case but do not persist every ambient ped forever. Add spatial/FOV/LOS debug visualization and test two witnesses producing different reports from the same crime.

## Stage 5 — Identity/masks/clothing (96–111)

**Prompt**

Implement roadmap 96–111. Build a stable outfit signature from GTA components/props, project-approved mask/bandana state, face evidence rules, clothing matching, outfit-change counterplay and repeat-clerk recognition. A mask prevents future face capture but never erases face evidence already gathered. A clothes change weakens clothing match but never clears confirmed identity. Reuse GTA V masks/clothing where validated; if no suitable lower-face bandana exists, mark `CUSTOM_REQUIRED` and keep gameplay functional without copying RDR2 assets.

## Stage 6 — Vehicle identity / garage plates (112–128)

**Prompt**

Implement roadmap 112–128. Create persistent owned-vehicle identity separate from GTA handles. Persist model/mod reconstruction, colors, plate/style, storage/status. Record immutable historical model/color/plate evidence and separate vehicle BOLO state. Add garage plate-change service with GTA-valid text rules and price; apply/save new plate while historical case evidence remains unchanged. Add repaint and vehicle-swap counterplay, temporary/stolen getaway records and crime-history flags. Test `ABC123 crime → change to XYZ789 → current car shows XYZ789 → case still says ABC123`.

## Stage 7 — Dispatch/investigation/interviews (129–145)

**Prompt**

Implement roadmap 129–145. Police response must target the crime scene, not hidden player GPS. Build severity-based dispatch, safe arrival/tasking, scene perimeter behavior, checking interiors/bodies/abandoned vehicles/exits, and `InvestigationDirector`. Select witnesses and derive interview semantic events from actual recorded observations. Use robust facing/idles/clipboard/phone/radio presentation, never mandatory cinematic choreography. Base-game speech is optional presentation; subtitle fallback must always work. Support high-detail nearby scene and abstract distant scene record/reconstruction.

## Stage 8 — Search/pursuit/warrants/surrender (146–163)

**Prompt**

Implement roadmap 146–163. Wrap immediate GTA wanted behavior while maintaining project last-known location/direction, search region and encounter matching. Stop moving the search center when police lose legitimate observation. Separate unknown-suspect cases, identified-person warrants and vehicle BOLOs. Add fair re-recognition, plate/model opportunities, surrender, arrest, fines, loot confiscation, impound and case-resolution rules that do not clear unrelated cases. Demonstrate that breaking observation plus smart disguise/vehicle changes can work without erasing a confirmed identity.

## Stage 9 — Persistent clerk/business memory (164–177)

**Prompt**

Implement roadmap 164–177. Store logical business/clerk profiles independent of streamed GTA peds. Persist robbery count/time, losses, violence, recognized identities/vehicles, surviving clerks, security and police attention. Restore compatible physical clerks after streaming. Add repeat-recognition behavior and security escalation levels: faster alarms, better cameras, possible guard, safer cash/safe behavior, plus balanced cooldown/recovery. Killing a clerk removes that person's memory but not the business/case record.

## Stage 10 — CCTV/evidence extensions (178–189)

**Prompt**

Implement roadmap 178–189 as logical observation volumes tied to configured/physical cameras. Do not implement computer vision. Capture face only if unmasked and visible, plus clothing, vehicle and plate when geometry/time rules support them. Track active/disabled/destroyed state and distinguish disabling before capture from destroying after stored capture. Add abandoned getaway vehicle evidence and optional abstract weapon-class evidence. Fuse confidence without automatic 100% identification.

## Stage 11 — Duffel and physical loot (190–209)

**Prompt**

Implement roadmap 190–209. Build `LootDirector`, loot sources and bag tiers none/small/standard/large with authoritative logical capacity. Larger bags must permit more money/valuables and take longer to fill. Use validated GTA heist-bag/money-grab content where safe; animation failure must fall back without losing loot state. Implement equip/attach cleanup, HUD, carried/dropped/vehicle/stash/secured/seized/lost transitions, recovery after streaming and police seizure. Major loot is not spendable until secured. Do not require visual bag deformation.

## Stage 12 — Vehicle ownership/garage/trunk (210–227)

**Prompt**

Implement roadmap 210–227. Add purchase/ownership, reconstruction/home garage, capacity, duplicate prevention, repair/insurance/impound, garage services and trunk interactions. Use boot bones/door data per vehicle and class-specific fallbacks for unusual vehicles. `VehicleInventory` stores weapons, ammo, armor, masks, duffels and robbery equipment. Inventory remains with impounded vehicles; insurance does not automatically duplicate illegal contents. Add robust open/reach/store/retrieve animations only where verified.

## Stage 13 — Limited carried weapons (228–237)

**Prompt**

Implement roadmap 228–237. Add configurable on-person slots and make trunk/safehouse storage replace GTA's unlimited free-roam pocket arsenal. Audit current inventory, migrate extras safely rather than deleting them, restrict reselection of non-carried weapons, support loadout exchange and persistence by story character, recover across death/arrest, and suspend during incompatible Rockstar missions. Add seizure rules for serious crimes.

## Stage 14 — Economy/progression (238–254)

**Prompt**

Implement roadmap 238–254. Build a project ledger that cleanly distinguishes spendable cash, unsecured loot and optional hot/marked proceeds. Add meaningful sinks: owned/DLC-compatible vehicles, garages/safehouses, weapons/ammo/armor, masks/clothes, bag upgrades, repairs/paint/plates, impound/legal costs, robbery equipment/intelligence/crew help. Only list vehicle models present in the installed build. Balance stores below banks and use visible systemic anti-farming via security/escalation rather than arbitrary punishment.

## Stage 15 — Scale stores/small targets (255–263)

**Prompt**

Implement roadmap 255–263 only after the prototype store/law loop is stable. Move all store-specific world data into definitions and add supported 24/7, liquor/gas-style and selected commercial targets by validating clerk/register/safe/door/customer/camera positions per location. Every new target must use the same case/witness/police/economy systems. Adding a store must require data plus a small adapter, not copied law logic.

## Stage 16 — Fleeca systemic bank (264–284)

**Prompt**

Implement roadmap 264–284. Validate a Fleeca interior separately on Legacy/Enhanced before gameplay. Define entrances/exits/tellers/customers/guard/cameras/vault/doors/loot. Allow normal bank visits and persistent casing discoveries. Add quick teller route, bounded hostage/crowd behavior, teller personality, guard reaction, alarm severity and validated vault access/equipment. Populate physical/logical loot, apply bag capacity/time and partial early exit, feed all witnesses/cameras/vehicles into normal cases, allow free-form escape and secure/process proceeds afterward. No giant 'start mission' marker and no mandatory escape cinematic.

## Stage 17 — Pacific Standard / major scores (285–298)

**Prompt**

Implement roadmap 285–298 after Fleeca passes. Validate Pacific Standard interior/doors/streaming, then add larger teller/civilian/guard/camera/vault configuration, multiple approaches only where geography really supports them, stronger dispatch and partial-loot decisions. Keep bag capacity relevant. Add crew assistance only if it improves systemic play. Treat Union Depository as research-only until its constraints are validated. Major scores require expensive preparation, severe evidence consequences and long recovery/security response.

## Stage 18 — Presentation/dialogue/audio (299–316)

**Prompt**

Implement roadmap 299–316. Replace debug UI with minimal GTA-consistent original HUD for bag, witnesses/reporting, investigation/search, warrants/BOLOs, garage plate service, trunk and catalogs. Build data-driven dialogue for clerks/civilians/witnesses/police/guards/hostages using `docs/DIALOGUE_BIBLE.md`; common events need many variants, cooldown/history and optional silence. Map to verified ambient speech where suitable, always provide subtitles, and support optional original/authorized voice packs. Do not clone Rockstar actor voices. Polish animation timing without making animation success core to state correctness.

## Stage 19 — Compatibility/performance (317–340)

**Prompt**

Execute roadmap 317–340 as a hardening campaign, not new features. Validate Legacy/Enhanced, character swaps, save/load across active/dormant cases, death/arrest, missions/cutscenes/interiors, trainer teleports as robustness tests, streaming cleanup, destroyed getaway cars, changed paint/plates, duffel loss/recovery, clerk replacement, multiple cases and long saves. Profile witness raycasts and police planning, cap detailed scenes, abstract distance, add migrations and a diagnostic report generator. Fix leaks/duplication/corruption before proceeding.

## Stage 20 — Release engineering (341–352)

**Prompt**

Implement roadmap 341–352. Freeze alpha schema, build versioned x64 ASI package, include only project ASI/config/data/docs/changelog, never Script Hook V/loader, state exact supported versions, backup/upgrade/migration instructions, and release progression criteria. Do not call the project 1.0 until stores, law persistence, vehicles, trunks, plates, economy, duffel, Fleeca and Pacific Standard are integrated and tested.

# Final acceptance campaign (353–390)

**Prompt**

Run the six acceptance scenarios in `MASTER_PLAN.md` exactly: clean masked store robbery, careless unmasked repeat robber, plate countermeasure, duffel capacity/loss, meaningful car/loadout/impound, and systemic Fleeca. For each scenario create a deterministic test sheet with setup, actions, expected case data, expected UI/world behavior, save/reload checkpoints and failure evidence. Do not pass a scenario based only on logs if the visible in-game behavior contradicts the data. Fix blockers in their owning subsystem instead of adding scenario-specific hacks.