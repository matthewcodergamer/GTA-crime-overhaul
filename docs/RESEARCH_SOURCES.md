# GTA Crime Overhaul — Research Sources & Findings Ledger

Research snapshot: 2026-09-14.

This file records which public references informed the implementation plan. A source proving that something exists in GTA V does **not** automatically make it production-ready. Exact fit still moves through the repository validation states in `RESEARCH_IMPLEMENTATION_PLAN.md`.

## Primary technical/data references

### Script Hook V / SDK

Purpose: native Story Mode `.asi` lifecycle, SDK headers/library and runtime support.

Project rule: Script Hook V remains an external prerequisite. We build against its SDK but do not redistribute `ScriptHookV.dll` or an ASI loader.

### alloc8or NativeDB / native references

Purpose: exact GTA V native names/hashes/signatures and categories.

Use for entity, ped/task, streaming, object, vehicle, audio, interior/door, HUD and related wrappers. A guessed signature is never accepted.

### DurtyFree — `gta-v-data-dumps`

URL: `https://github.com/DurtyFree/gta-v-data-dumps`

At the time of this research the repository states its data is current through GTA V v3717.0 / Online 1.72 and exposes large generated catalogs including:

- 20,179 animation dictionaries / 269,414 animations;
- 361,368 speech voice/speech entries;
- 43,428 sound-name entries across 43 refs;
- 921 vehicle infos;
- 33,558 ped component variations and 5,883 props;
- IPLs, scenarios, movement clipsets, particles, pickups, vehicle mods and other scripting data.

Use: discover exact candidate identifiers, then validate visual/gameplay fit in our installed target builds.

### Pleb Masters Forge / GTA asset browsers

Use: interactive lookup/preview for GTA V props and animations. Helpful for narrowing candidate lists before in-game tests.

### CodeWalker — dexyfex

URL: `https://github.com/dexyfex/CodeWalker`

Capabilities useful here:

- inspect GTA V RPF contents;
- world-view selection with archetype/drawable/entity information;
- edit/view entity positions and rotations;
- inspect map/interior assets;
- project support for YMAP and related world data;
- Explorer mode for archive inspection.

Use: exact store/bank coordinates, door entities, registers, safes, cameras, interior structure and world-model research. We do not copy coordinates blindly from multiplayer scripts when the actual Story Mode world can be measured.

## Animation/model findings

Public GTA V data confirms a substantial existing heist vocabulary. Current research candidates already recorded in `ASSET_ANIMATION_CATALOG.md` include:

- `anim@heists@money_grab@duffel`;
- `anim@heists@ornate_bank@grab_cash`;
- `anim@heists@fleeca_bank@bank_vault_door`;
- `anim@heists@fleeca_bank@drilling`;
- `anim@heists@keypad@`;
- `anim@heists@keycard@`;
- arrest/hands-up/surrender families;
- mobile-phone and cop/clipboard/medic scenarios.

Candidate GTA V props include heist/duffel bags, cash notes/piles/trolleys, Fleeca drill, heist drill, keycard, alarm and related equipment. These are references to assets in the user's installed game, not redistributed files.

Important conclusion: many required mechanics can be assembled from native GTA V content, but synchronized clips must be tested for actor/prop alignment. Logical gameplay always retains a non-cinematic fallback.

## Interior reference

### InteriorsV-ScriptHookV

URL: `https://github.com/SuleMareVientu/InteriorsV-ScriptHookV`

Why it matters:

- open-source native Script Hook V Story Mode interior work demonstrates that normally restricted Story Mode interiors/doors can be managed by an ASI;
- its published interior list includes locations relevant to this project such as Blaine County Savings Bank and Union Depository vault access, plus many mission interiors;
- its history explicitly mentions mission compatibility, interior streaming improvements and door-system work.

Use: reference for feasibility and interior-management patterns only. We still validate our Fleeca/Pacific Standard/Union Depository target behavior ourselves and obey the source license.

## Dialogue/audio findings

### GTA V speech dumps

DurtyFree's data contains hundreds of thousands of speech/voice entries usable through ambient-speech natives. This supports using installed GTA speech for short generic reactions where semantic meaning and voice compatibility truly fit.

### DialogueController — Lucas7yoshi

URL: `https://github.com/Lucas7yoshi/DialogueController`

This CitizenFX reference demonstrates a useful concept: GTA scripted conversations and randomized-line families can be addressed through GXT/line/voice metadata, and randomized line bases can select among numbered variants. It also notes the importance of correct speaker/voice mapping and conversation queuing.

Use: concept/reference only; Crime Overhaul remains a native Story Mode C++ ASI and does not depend on CitizenFX.

### Ambient-dialogue design research

Analysis of GTA V's pedestrian dialogue reinforces an archetype/pool approach: NPC reactions are organized by broad archetype/context and small reusable bark categories rather than one handcrafted conversation for every pedestrian. This is consistent with our data-driven semantic-event selector.

Project conclusion: repetition is solved with **context + pool depth + cooldown/no-repeat history + session limits + silence**, not merely by writing one longer script.

## Reference mods demonstrating feasibility

These are not dependencies and do not define our architecture.

### Store Robbery Enhanced

Public descriptions/releases demonstrate that Story Mode store robbery mods can maintain per-store state, cooldowns, clerk reactions, safe interaction, alarms, debug tooling and large configurable dialogue/message pools. One current description advertises hundreds of categorized reactive messages.

Lesson: large pools are useful, but Crime Overhaul will tie lines to semantic state/evidence and actively prevent repeat spam rather than simply randomizing a huge flat list.

### OnTheBlock / street-immersion systems

Public descriptions demonstrate Story Mode concepts such as persistent police cases, suspect clothing/vehicle/plate/mask context, vehicle BOLOs, impound, NPC memory and disguise/vehicle counterplay.

Lesson: the general direction is feasible in GTA V scripting. Crime Overhaul remains independent and implements the law simulation in the native ASI architecture defined here.

### LSPDFR ecosystem references

Police-computer/BOLO/ALPR/evidence mods show useful UI/data concepts for reports, person/vehicle records, evidence quality and impound flows. They are reference material only: core Crime Overhaul must run without LSPDFR/RAGEPluginHook.

## Research conclusions locked for implementation

1. **Native GTA V first.** Before making a custom model/animation, search the installed game's current asset catalogs.
2. **No RDR2 asset port.** Recreate the bandana identity mechanic using GTA V clothing or an original custom model.
3. **Duffel capacity is logical.** Existing heist bags/cash-grab animations can provide presentation; capacity/value do not depend on mesh fullness.
4. **Police interviews are semantic.** Officer/witness animations and speech decorate real case data rather than creating the data.
5. **Plate changes are feasible gameplay.** The live owned vehicle can receive a new plate while historical evidence remains the previously observed text.
6. **Interiors require edition tests.** Existing projects prove interior manipulation is possible, not that one exact configuration is safe on every Legacy/Enhanced build.
7. **Dialogue can reuse installed ambient speech selectively.** Original subtitles/authorized voice packs remain the fallback for exact semantic meaning.
8. **No psychic law simulation.** GTA natives provide LOS/entity/task primitives; our project must maintain its own observation/case/search logic.
9. **Streaming is a first-class failure mode.** Logical profiles persist; physical GTA entities are replaceable.
10. **Every unverified content identifier remains a candidate.** `VERIFIED_DATA` is not equivalent to `VERIFIED_IN_GAME`.

## Source-quality rule

Prefer official/current game data, current NativeDB/SDK references and direct in-game inspection over third-party articles or old lists. Reference mods are useful for proof-of-concept and edge-case ideas, not as authoritative native documentation.