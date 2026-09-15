# GTA Crime Overhaul — Research & Asset Validation Prompts

Use these before implementing any GTA-specific feature whose native, model, animation, speech, coordinate, door, interior or bone has not been verified.

## A. Native capability research prompt

Read `docs/RESEARCH_IMPLEMENTATION_PLAN.md`, `docs/FEASIBILITY_MATRIX.md` and the relevant roadmap range. Research the exact GTA V native capabilities required for this feature. Prefer current Script Hook V/NativeDB-compatible signatures and Rockstar/decompiled-script usage patterns. Produce a table with: requirement, native name/hash, parameter meaning, return behavior, known edition differences, failure/streaming concerns, wrapper API we should expose, source, and status (`VERIFIED_DATA`, `REFERENCE_ONLY`, etc.). Do not code guessed hashes. If a native cannot be verified, design a safe fallback or defer that subfeature.

## B. Animation lab prompt

Find GTA V animation candidates for `[INTERACTION]`. Search current GTA V animation dumps/Forge and relevant decompiled scripts. For each candidate provide exact dictionary, clip, intended actor, full-body/upper-body suitability, likely flags, root-motion/synchronized-scene requirements, props required, interrupt/cleanup risks and source. Put untested candidates into the research catalog as `VERIFIED_DATA`, not `VERIFIED_IN_GAME`. Then design an in-game debug lab that cycles candidates one at a time and records load success, visual alignment, entry/exit, interruption by damage/ragdoll/death, vehicle transition, and cleanup. Never invent an animation name.

## C. Model/prop search prompt

Research installed GTA V props/models for `[NEED]`. Prefer existing GTA V heist/store/bank assets. For each candidate capture exact model/spawn name, intended use, dimensions/alignment concerns, whether it is a world-static entity or safe runtime prop, attachment bone/offset research needed, source and validation status. Favor referencing assets already in the user's game over distributing replacements. If no acceptable candidate exists, mark `CUSTOM_REQUIRED` and specify the smallest original asset needed.

## D. Custom model creation prompt

The GTA V asset catalog has no acceptable verified asset for `[NEED]`; design an original distributable replacement. Specify visual target, polygon/LOD goals, material/PBR-ish texture set appropriate to GTA V, collision needs, skeleton/clothing requirements if any, Blender + Sollumz export path, naming convention, source-file layout, in-game scale tests, attachment/bone tests, Legacy/Enhanced validation and license metadata. Do not imitate or extract an RDR2/GTA VI proprietary asset. The asset must be original or properly licensed.

## E. Store survey prompt

Survey one GTA V convenience/liquor/gas-station target for Crime Overhaul. Use CodeWalker and in-game debug coordinates. Record business volume, entry/exit points, clerk anchor/heading, register models/positions, safe opportunity, customer area, nearby camera/alarm props, likely police approach/parking points, road escape directions, interior/door behavior and edition differences. Do not commit production coordinates until confirmed in game. Output the final target as the repository's business JSON schema plus a human-readable validation sheet.

## F. Bank/interior survey prompt

Survey `[FLEECA / PACIFIC STANDARD / OTHER]` for a systemic free-roam robbery. Identify exact IPL/interior activation, public entry, exits, teller anchors, customer/hostage region, guard positions, security cameras, vault/secure doors, loot sources, alarm/security points and safe police approach paths. Record every door/entity that requires scripted state. Separate Legacy and Enhanced findings. If the interior cannot be made reliable without conflicting with Story missions, document the limitation instead of forcing it.

## G. Duffel/loot animation prompt

Research GTA V's own bag and money-grab vocabulary for Crime Overhaul. Start with the verified-data candidates listed in `docs/ASSET_ANIMATION_CATALOG.md`, including heist duffel/money-grab and Fleeca/vault-related content. Determine which clips work for store register grabs, bundle grabs, trolley grabs, bag equip/remove and dropped-bag pickup. Separate logical gameplay from visuals: bag capacity/value must remain correct if an animation fails. Return a test matrix for Franklin, Michael, Trevor and relevant NPCs with fallback behavior.

## H. Mask/bandana prompt

Research GTA V component/prop variations for a lower-face identity-concealment option on supported protagonists. Capture component slot, drawable/texture IDs or collection identifiers, visual coverage, hairstyle/hat conflicts, cutscene/vehicle issues and availability by story character/edition. If no acceptable GTA V asset exists, mark an original custom bandana `CUSTOM_REQUIRED`; do not port the RDR2 bandana.

## I. Vehicle trunk prompt

Research trunk/boot interaction across representative sedans, coupes, SUVs, vans, pickups, supercars, motorcycles and special vehicles. Determine door index, useful bones, rear interaction offset, whether the boot opens correctly, and fallback classification. Produce a compatibility policy so inventory correctness does not depend on every GTA vehicle having a conventional trunk.

## J. Speech/audio mapping prompt

For semantic event `[EVENT]`, research GTA V ambient speech or scripted dialogue that can legally be referenced from the installed game. Use current speech dumps and verify voice compatibility. Return speech name, compatible voices/ped archetypes, emotional tone, whether the line's meaning actually matches the event, repeat variations, interruption rules, subtitle fallback and validation status. Do not force an unrelated line merely because it exists. Never extract/distribute Rockstar audio.

## K. Open-source reference-mod prompt

Study `[REFERENCE MOD/REPOSITORY]` only for architecture/behavior ideas relevant to `[FEATURE]`. Identify what is demonstrably possible in Story Mode, useful state-machine/data concepts, performance lessons and failure cases. Keep our implementation independent; do not copy copyrighted assets or code with incompatible licensing. Clearly distinguish observation from something verified in our native C++ runtime.

## L. Decompiled-script orchestration prompt

Study GTA V decompiled scripts for how Rockstar orchestrates `[INTERACTION]`: streaming requests, task/animation setup, scene synchronization, prop ownership, door/interior state, audio and cleanup. Extract the sequence/pattern, not large copyrighted script passages. Translate it into a clean native C++ adapter design with timeouts and failure recovery. Record every asset/native identifier that still needs current-build validation.

## M. Performance research prompt

Profile `[SYSTEM]` in realistic dense-world conditions. Measure baseline and enabled frame time/CPU behavior, entity counts, raycast/task frequency and allocations. Test worst cases such as a busy intersection, several witnesses, active police scene and an interior. Recommend event-driven/staggered frequencies and hard caps. Never fix performance by making witnesses magically know information that avoids real perception checks.

## N. Research completion gate prompt

Audit all research dependencies for roadmap steps `[RANGE]`. For every dependency classify it `VERIFIED_IN_GAME`, `VERIFIED_DATA`, `REFERENCE_ONLY`, `CUSTOM_REQUIRED` or `REJECTED`. List exactly which remaining items block production implementation and create the smallest debug lab required to promote each one to `VERIFIED_IN_GAME`. Do not write production logic around `REFERENCE_ONLY` assets.