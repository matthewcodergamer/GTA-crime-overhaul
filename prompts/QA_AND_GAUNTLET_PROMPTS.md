# GTA Crime Overhaul — QA / Gauntlet Prompts

Use these after implementation prompts. The critic must judge the repository and evidence, not trust the builder's summary.

## Builder prompt

Implement roadmap steps `[RANGE]` under `prompts/MASTER_AGENT_PROMPT.md`. Keep the patch limited to the owning systems. Build Release, run available tests and produce a validation packet containing: changed files, exact requirements, compile/test logs, debug instrumentation, GTA-specific items still requiring manual validation, save/schema changes, known failure paths and a reproduction checklist. Do not mark GTA-specific behavior verified merely because it compiles.

## Blind critic prompt

You are a hostile but fair senior GTA V native-mod reviewer. Do not assume the builder's claims are true. Read `DESIGN_LOCKS`, the relevant `MASTER_PLAN` steps, architecture/research docs, then inspect the patch. Grade every acceptance requirement `PASS`, `PARTIAL`, `FAIL`, or `UNVERIFIED_IN_GAME` with concrete evidence. Look specifically for guessed natives/assets, raw entity handles persisted across frames/saves, streaming-invalid handles, stuck tasks, cleanup leaks, mission conflicts, omniscient witnesses/police, evidence being rewritten after disguise/plate changes, duplicated economy payouts, dialogue spam, missing controller path, hard-coded store logic, and release packaging mistakes. Return blockers in priority order and the smallest corrective patch needed. Do not reward scope expansion.

## Repair prompt

Read the critic report. Fix only `FAIL` and `PARTIAL` items plus compile/runtime blockers. Do not add unrelated features. Preserve save compatibility or add a migration. Rebuild and return a point-by-point response showing the code/data/test that resolves each finding. Keep `UNVERIFIED_IN_GAME` items explicitly unverified and create exact manual test instructions instead of pretending they passed.

## Regression critic prompt

Compare the repaired branch against its base. Confirm the fix did not regress earlier completed stages. Recheck lifecycle/unload, save/load, mission gating, performance caps, entity cleanup, evidence immutability, vehicle/plate history, duffel state, economy and dialogue selector as relevant. Return `MERGE READY` only if automated/build requirements pass and remaining GTA-specific items are clearly labeled manual validation rather than hidden failures.

# Feature-specific gauntlets

## Witness gauntlet

Attempt to break witness logic with: witness behind wall; hearing but no visual; player masked before entering; mask added after face exposure; vehicle visible but plate occluded; brief vs sustained plate view; darkness/long distance; witness killed before report; second witness sees witness attack; player changes clothes; player swaps cars out of sight/in sight. Verify the case contains only defensible observations.

## Police-search gauntlet

Attempt to make police cheat: lose LOS and reverse direction; hide in alley; switch car unseen; repaint/change plate after escape; circle back near scene; get newly spotted by a different officer/camera; leave target area entirely. Confirm last-known/search data updates only from legitimate sources and persistent warrants/BOLOs behave independently.

## Persistence gauntlet

Save/reload at every meaningful state: normal store; active robbery; witness reporting; reported case; police en route; investigation; active search; dormant warrant; dropped duffel; impounded vehicle; changed plate; replaced clerk. Validate no raw-handle resurrection, duplicates, money duplication, missing evidence or corrupted JSON.

## Duffel gauntlet

Test no bag/small/standard/large; fill to exact capacity; try overfill; interrupt animation; enter vehicle; ragdoll; die; arrest; drop/recover; stream away/back; police seize; store in trunk; secure at stash. Logical value must remain authoritative and never duplicate/disappear from a cosmetic failure.

## Dialogue gauntlet

Simulate repeated common events and verify no event monopolizes one line. Test personality filters, evidence-aware witness answers, cooldown/no-repeat history, session limits, silence chance and save-level reuse. Reject lines that reveal unseen face/plate/vehicle information or fire while actor is dead/fleeing/incapacitated.

## Garage/plate gauntlet

Commit a crime with `ABC123`, save/reload, repaint, change to `XYZ789`, despawn/reconstruct the car, trigger patrol/camera matching, and inspect the case. The current car must retain `XYZ789`; historical evidence remains `ABC123`; direct old-plate matching decreases but stronger owner/vehicle evidence can still matter.

## Bank gauntlet

Run Fleeca/Pacific Standard under: normal civilian visit; quick teller robbery; vault route; early exit; full bag; guard attack; hostage panic; alarm before/after face capture; camera disabled before/after capture; player dies; mission starts; interior streams out; getaway vehicle destroyed. No path may leave doors, peds, tasks, props, input lock or interior state permanently broken.