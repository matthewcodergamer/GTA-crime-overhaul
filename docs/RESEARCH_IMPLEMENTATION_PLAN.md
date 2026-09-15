# GTA Crime Overhaul — Research & Implementation Plan

This document turns the design roadmap into a repeatable research-and-build process. It is intentionally conservative: no gameplay feature is allowed to depend on a guessed native, guessed model, guessed animation clip, or asset copied from another Rockstar title.

## 1. Source hierarchy

Use sources in this order whenever researching a GTA V implementation detail:

1. The installed GTA V build itself, inspected in-game and with CodeWalker.
2. Alexander Blade Script Hook V SDK / official Script Hook V distribution.
3. alloc8or NativeDB / current native references for exact native signatures.
4. DurtyFree GTA V data dumps for current animation, speech, sound, vehicle, ped-component, IPL and related names.
5. Decompiled GTA V scripts when needed to understand how Rockstar orchestrates an interaction.
6. CodeWalker for world entities, interiors, doors, props, coordinates, archetypes and RPF inspection.
7. Open-source Story Mode mods as implementation references, never as unquestioned truth.
8. FiveM/open-source resources only as secondary examples of GTA V asset usage; network-specific code must not be copied into the Story Mode runtime.

Every research result must be marked `VERIFIED_IN_GAME`, `VERIFIED_DATA`, `REFERENCE_ONLY`, `CUSTOM_REQUIRED`, or `REJECTED`.

## 2. Hard technical constraints

- Target GTA V Story Mode/offline only.
- Native C++20 Script Hook V `.asi` remains the authoritative runtime.
- Do not depend on GTA Online servers, FiveM, RAGEPluginHook or LSPDFR for core gameplay.
- Legacy and Enhanced are separate validation targets.
- Never persist raw entity handles. Persist logical IDs and reconstruct physical entities after streaming/load.
- Never ship ScriptHookV.dll, an ASI loader, Rockstar archives, RDR2 assets, extracted GTA audio, or other copyrighted game files.
- Existing GTA V model/animation/audio names can be referenced by name and invoked from the user's legally installed game.
- Custom original assets may be distributed only if we own or have permission to distribute them.

## 3. Required local research toolchain

### CodeWalker
Use for:
- selecting world entities and reading exact model/archetype names;
- finding doors, tills, safes, cameras, vault pieces and bank/store interior geometry;
- reading world coordinates/headings;
- inspecting RPF content and IPL/interior structure;
- verifying whether an object is static map geometry or a scriptable entity;
- creating temporary map research projects if needed.

### DurtyFree data dumps / Pleb Masters Forge
Use for:
- exact animation dictionary + clip names;
- speech names and compatible voices;
- sound names/reference sets;
- ped component/prop variations for masks and bags;
- vehicle model metadata;
- IPL names;
- scenario names and movement clipsets.

### NativeDB
Build wrapper inventory around, at minimum:
- ENTITY: existence, transforms, LOS, bones, attachments, animation state;
- PED: tasks, combat/flee/cower, components/props, weapon state, ragdoll/death;
- TASK: go-to, aim, flee, combat, scenario, synchronized/regular animation tasks;
- STREAMING: models, animation dictionaries, IPLs;
- OBJECT: create/delete/place objects;
- VEHICLE: doors, colors, plate text/style, mods, damage, locks;
- AUDIO: ambient speech, sounds, scripted conversation checks, police reports where safe;
- INTERIOR/DOOR: pin/refresh interiors and door state where required;
- HUD: subtitles/help text/blips;
- CAM/GRAPHICS only for debug and presentation polish, never core correctness.

## 4. Research workstreams

### R1 — Native wrapper audit (Roadmap 31–46)
Deliverable: `NativeWorldAdapter` implementation checklist.

Research exact current native signatures for:
- player state and mission/cutscene gating;
- nearby ped/vehicle enumeration strategy;
- line-of-sight and field-of-view helpers;
- outfit signature collection;
- model request/release;
- animation request/play/stop;
- synchronized scenes;
- prop attach/detach;
- vehicle plate modification;
- vehicle door/trunk manipulation;
- ambient speech and scripted-conversation state;
- door/interior activation.

Acceptance: every wrapper has a small debug command that proves it works in both target editions or is explicitly edition-gated.

### R2 — Prototype store survey (60–76)
Survey one 24/7 first. Record:
- business volume;
- clerk anchor and heading;
- register object(s) and interaction offsets;
- optional safe location;
- entrances/exits;
- customer waiting/roaming region;
- panic/alarm opportunity;
- nearby road/police approach points;
- camera coverage if cameras are present or will be logical additions.

Create a JSON target definition only after coordinates have been validated in-game.

### R3 — Clerk behavior/animation lab (67–75)
Test candidate clips/tasks for:
- anxious hands up;
- generic surrender;
- cower;
- getting down;
- moving to register;
- register interaction;
- phone call;
- panic button reach;
- fleeing;
- drawing/firing a weapon;
- recovering to ambient clerk state.

Each test records: ped model, dictionary, clip, flags, blend rates, root-motion behavior, duration, looping safety, whether weapon props break it, and cleanup behavior.

### R4 — Witness perception lab (77–95)
Create a debug-only witness cone overlay and record practical thresholds for:
- face observation;
- clothing observation;
- vehicle model/color;
- license plate visibility;
- hearing gunshots/shouts;
- reporting interruption.

Do not use a single omniscient radius. Every evidence channel has separate rules.

### R5 — Identity/mask/clothing lab (96–111)
Use ped component data to define a stable outfit signature. Test existing GTA V masks/bandanas first. If a good lower-face bandana is unavailable for the required protagonist/model, create an optional original GTA-compatible custom clothing asset later. Do not port RDR2 cloth/model/animation assets.

### R6 — Vehicle/garage/plate lab (112–128)
Validate:
- reading current plate and plate style;
- applying new plate text;
- legal character/length limits in GTA V;
- owned vehicle reconstruction;
- repaint behavior;
- trunk/boot bone and door indices by vehicle class;
- persistence across despawn/load.

Historical case evidence remains immutable after a plate or paint change.

### R7 — Police scene/investigation lab (129–177)
Test robust, non-cinematic building blocks:
- police vehicle arrival and parking;
- officers walking to scene anchors;
- cop idle/scenario;
- clipboard/notepad pose;
- witness and officer facing;
- phone/radio pose;
- body-check/kneel pose;
- scene perimeter wandering;
- cleanup when actors stream/despawn.

Police interviews are semantic simulation first. Animation/audio presentation is a layer on top.

### R8 — CCTV/evidence lab (178–189)
Do not attempt computer vision. Cameras are logical observation volumes tied to physical or configured camera entities. Store timestamped snapshots of what the camera could see according to our rules.

### R9 — Duffel/loot lab (190–209)
Test existing heist-bag and money-grab content. Candidate systems include:
- a persistent visible shoulder/arm bag where safe;
- synchronized cash trolley sequences for banks;
- simpler generic grab/search clips for stores;
- physical cash-note/pile props only as short-lived visual effects;
- logical capacity as the authoritative source of truth.

Bag fullness does not require mesh deformation. Capacity, time and loss risk are gameplay guarantees; visible fullness is optional polish.

### R10 — Vehicle inventory/loadout lab (210–237)
Validate trunk interaction across sedans, coupes, SUVs, vans and unusual vehicles. If a vehicle has no sensible boot, use a class-specific fallback interaction point or mark it incompatible with storage.

### R11 — Bank/interior lab (264–298)
Validate in this order:
1. Fleeca vault/interior;
2. Pacific Standard public deposit bank/vault;
3. Union Depository only after the first two are stable.

For every target record exact IPL/interior/door/entity behavior separately for Legacy and Enhanced.

### R12 — Dialogue/audio lab (299–316)
Build a searchable compatibility table linking semantic events to:
- base-game ambient speech name;
- compatible ped voice(s);
- subtitle fallback;
- optional project-owned voice asset;
- cooldown group;
- emotional intensity;
- repetition weight.

No line is allowed to be the sole response for a common event.

## 5. Asset validation protocol

For every candidate model/animation/audio item:

1. Add it to `data/research/asset_candidates.json` or the appropriate test file.
2. Spawn/play only in a debug lab, not production gameplay.
3. Verify the game reports model/anim dictionary loaded.
4. Test with Franklin, Michael and Trevor where applicable.
5. Test at least one ambient male and female ped for NPC clips.
6. Test entering/exiting the animation, interruption by damage, death, ragdoll and streaming.
7. Test attachment cleanup on vehicle entry/exit.
8. Record exact offsets only after visual confirmation.
9. Mark the entry `VERIFIED_IN_GAME` with game edition/build and date.
10. Production systems may use only verified entries or a documented safe fallback.

## 6. Model/content acquisition policy

### Reuse from installed GTA V
Preferred. We reference model names and ask the engine to stream them. Examples include heist bags, cash props, bank trolleys, phone/notepad props and existing masks/clothing.

### Create original custom content
Use only when GTA V lacks an acceptable asset. Likely candidates:
- a protagonist-compatible lower-face bandana/handkerchief if no existing component meets the design;
- optional polished duffel variants with our own textures;
- project UI icons/textures;
- authorized/original voice packs.

Recommended custom asset pipeline: Blender + Sollumz → GTA V drawable/texture/clothing formats → OpenIV development copy for local installation testing. Keep source `.blend`/textures in a separate licensed asset area only if distribution rights allow.

### Never copy
- RDR2 models, textures, animations or audio;
- GTA VI leaked/ripped assets;
- copyrighted third-party mod assets without permission.

Recreate mechanics and visual intent, not proprietary assets.

## 7. Data files we should add as implementation advances

```text
data/
  research/
    asset_candidates.json
    animation_test_results.json
    interior_test_results.json
    speech_test_results.json
  businesses/
    stores.json
    banks.json
  dialogue/
    clerks.json
    civilians.json
    witnesses.json
    police.json
    guards.json
    hostages.json
  economy/
    prices.json
    loot_tables.json
    bag_tiers.json
    vehicle_catalog.json
  law/
    evidence_rules.json
    response_profiles.json
    recognition_rules.json
```

## 8. Definition of researched enough to implement

A roadmap item may move from research to implementation when:
- the native/API path is known;
- all referenced game assets are validated or have safe fallbacks;
- persistence impact is specified;
- failure/cleanup behavior is defined;
- performance budget is known;
- Legacy/Enhanced uncertainty is documented;
- the feature has an acceptance scenario.

Research is not a substitute for implementation. Once these conditions are met, build the smallest end-to-end slice and test it in game.

## 9. Current research conclusions

- Native `.asi` Story Mode is the correct base for this project.
- CodeWalker is the primary world/interior/prop survey tool.
- DurtyFree data dumps provide a strong current index for animations, speech, audio, ped clothing/props, IPLs and vehicles.
- Existing GTA V heist content provides much of the cash/bag/drill/vault interaction vocabulary required for bank gameplay.
- Existing GTA V generic ped/scenario content provides hands-up, cower, phone, clipboard, cop idle, arrest and surrender presentation.
- Base-game speech is useful for short emotional barks. Complex police interviews and robbery-specific sentences need subtitle-driven semantic dialogue and optional original voice packs.
- Fleeca and Pacific Standard interiors are known to be accessible in Story Mode through existing interior work, but our mod must independently validate exact streaming/door behavior on both editions.
- The game does not give us a complete forensic/witness engine; those systems are ours and must be simulated from observed world state.
