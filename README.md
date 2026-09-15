# GTA Crime Overhaul

A native **Grand Theft Auto V Story Mode** crime-life overhaul focused on free-roam robberies, persistent witnesses and investigations, identity/vehicle recognition, meaningful money, persistent owned vehicles, limited carried weapons, vehicle storage, masks, duffel-bag loot capacity, and escalating businesses/security.

> **Story Mode / offline only.** This project is not intended for GTA Online. Script Hook V itself blocks multiplayer use.

## Core fantasy

**Prepare → disguise → choose vehicle → choose weapons/equipment → case target → commit robbery → physically collect loot into a bag → witnesses react → police investigate → escape/search phase → hide or change identity/vehicle/plate → secure proceeds → spend money on meaningful upgrades → prepare a larger job.**

Losing GTA V's immediate wanted stars does **not** automatically erase the crime. Cases, witnesses, vehicle descriptions, plates, masks/clothing, CCTV and warrants can persist.

## Build target

The authoritative runtime is a native C++ Script Hook V plugin:

```text
GTA_Crime_Overhaul.asi
```

The repository does **not** redistribute `ScriptHookV.dll` or the ASI loader. Download those from Alexander Blade's official Script Hook V site. The CI build downloads the developer SDK from the official source and packages only this project's files.

## Current stage

**Phase 0 native foundation and Stage 1 shared GTA adapter layer are complete.** The repository now also contains the first investigation-dialogue vertical slice: a fact-bound police/witness interview composer plus an in-game overhearing debug path. The real persistent case, witness and police-scene systems still arrive in their roadmap stages; the dialogue slice is intentionally designed so those systems can feed it structured evidence later without rewriting presentation.

The next full gameplay milestone remains one polished 24/7 robbery before scaling to all stores or banks.

## Read before changing gameplay

Read in this order:

1. `docs/DESIGN_LOCKS.md`
2. `docs/MASTER_PLAN.md`
3. `docs/ARCHITECTURE.md`
4. `docs/FEASIBILITY_MATRIX.md`
5. `docs/RESEARCH_IMPLEMENTATION_PLAN.md`
6. `docs/ASSET_ANIMATION_CATALOG.md`
7. `docs/DIALOGUE_BIBLE.md`
8. `docs/INVESTIGATION_DIALOGUE.md`
9. `docs/RESEARCH_SOURCES.md`
10. `docs/CRIME_LAW_SYSTEM.md`
11. `docs/ROBBERY_ECONOMY_VEHICLES.md`
12. `docs/BUILD_AND_RELEASE.md`
13. `docs/STAGE1_NATIVE_VALIDATION.md`

If a later idea conflicts with `docs/DESIGN_LOCKS.md`, update the lock deliberately rather than silently changing the design.

## AI implementation prompt pack

Use these when handing work to ChatGPT/Codex or another coding agent:

- `prompts/MASTER_AGENT_PROMPT.md` — permanent project rules and architecture contract.
- `prompts/STAGE_PROMPTS.md` — Stage 0 through Stage 20 plus final acceptance campaign.
- `prompts/STEP_EXECUTION_TEMPLATE.md` — converts any numbered roadmap item into a safe focused coding task.
- `prompts/RESEARCH_AND_ASSET_PROMPTS.md` — natives, animations, models, interiors, masks, trunks, speech and feasibility research.
- `prompts/DIALOGUE_AUTHORING_PROMPTS.md` — clerk, robber, civilian, witness, police, guard/hostage and anti-repetition content generation.
- `prompts/QA_AND_GAUNTLET_PROMPTS.md` — builder/critic/repair/regression loops and feature-specific torture tests.

The prompts explicitly forbid guessed GTA native/animation/model identifiers. Asset findings must be classified as `VERIFIED_IN_GAME`, `VERIFIED_DATA`, `REFERENCE_ONLY`, `CUSTOM_REQUIRED`, or `REJECTED`.

## Rule for step ranges

Only group roadmap steps when they form **one atomic system change** that can be reviewed and tested as a coherent unit. A roadmap stage is not automatically a safe implementation range.

- Good: `81–87` witness observation channels.
- Good: `117–123` garage plate-changing behavior.
- Good: `191–196` bag tiers/equip/visual attachment.
- Bad: `77–128` because witness, identity and vehicle systems become too large to review safely.

If a range crosses subsystem ownership, persistence boundaries, or distinct acceptance criteria, split it into smaller contiguous ranges. Future implementation prompts should take the smallest safe atomic slice rather than bundling unrelated systems simply because they appear under the same stage heading.

## Major systems

- Free-form store, commercial-target and bank robberies
- Witness memory with actual line-of-sight/knowledge rather than omniscience
- Unknown-suspect → investigation → identified suspect → BOLO/warrant flow
- Face, mask, clothing, vehicle model/color and plate evidence
- Persistent clerks/businesses and repeat-offender recognition
- Police return to and investigate crime scenes after the player leaves
- **Overhearable police interviews:** officers question clerks/witnesses from actual evidence, and an unknown suspect can physically walk near the scene and hear what police currently know
- Search areas and last-known-position logic instead of psychic tracking
- Separate **person wanted** and **vehicle wanted** concepts
- Garage plate changes, repainting and vehicle swapping as countermeasures
- Masks/bandanas that reduce face identification but do not erase prior recognition
- Physical duffel-bag capacity; larger bags carry more loot and take longer to fill
- Loot can be dropped, lost, recovered or seized before it is secured
- Persistent owned vehicles, garages, impounds and vehicle crime history
- Trunk weapon/equipment inventory and limited on-person weapon carry
- Story Mode purchase path for compatible DLC/Online vehicles present in the installed game build
- Meaningful economy: vehicles, garages, property, equipment, masks, bags, weapons, repairs, plate changes, legal costs and heist preparation
- Store security escalation after repeated robberies
- Fleeca → Pacific Standard → major-score progression
- Minimal Rockstar-style HUD, data-driven dialogue, ambient police interviews and reusable animation orchestration

## Asset strategy

Use existing GTA V content first. Current research catalogs candidate GTA V heist bags, cash/trolley/drill props, Fleeca/vault interactions, heist money-grab animations, surrender/hands-up/cower, phone reporting, cop/clipboard/medic presentation and mask/component data. Candidates are not production-authoritative until validated in the target game build.

If GTA V lacks a suitable asset, create an **original GTA-compatible custom asset** using an appropriate Blender/Sollumz/OpenIV development pipeline and only distribute content we have rights to. Do not port RDR2 or GTA VI assets.

## Dialogue rule

Dialogue is not one repeated script. Evidence/knowledge is resolved first; presentation happens afterward. A witness may only talk about a face, mask, clothing, vehicle, plate or direction that their observation actually contains. C++ emits semantic events and the dialogue/presentation layer varies wording using speaker role, fear, prior use and context. An optional future generated-voice/TTS provider may speak or paraphrase already-approved facts, but it must never invent or upgrade case evidence.

Police/witness interviews are autonomous scene events rather than player dialogue trees. If the player's identity is still unknown, simply approaching the interview does **not** reveal that the player is the suspect. Recognition belongs to the separate law/identity system. Walking out of hearing range does not pause an interview; walking back can let the player overhear whatever part is happening then.

### Current dialogue debug path

With `DebugHotkeys=true`, press **F7** in populated free roam. The prototype picks two nearby live non-player peds as temporary officer/witness actors, builds a synthetic masked/unknown-suspect statement, and runs the conversation every few seconds. Lines display only while the player is within the hearing radius. The demo mutates no case, warrant, economy or business data and stops safely if actors stream out or a mission/cutscene compatibility gate activates.

See `docs/INVESTIGATION_DIALOGUE.md` for the production architecture and voice strategy.

## Development rule

Do **not** build twenty shallow robbery locations first. The first gameplay milestone is one polished 24/7 where a clerk and customer can independently observe the crime, report only what they actually know, police can arrive after the player has left, a case can persist, and the same clerk can recognize an unmasked robber on a later visit.

## Compatibility goal

- Windows x64 GTA V Story Mode
- GTA V Legacy + Enhanced, through current Script Hook V compatibility
- No Online support
- No hard dependency on LSPDFR/RAGEPluginHook
- Graceful suspension during incompatible Rockstar missions/cutscenes

## License / game assets

Project source is authored for this repository. Rockstar assets are not redistributed. RDR2 mechanics may inspire behavior, but RDR2 models, animations or audio are **not** copied into GTA V. Use GTA V assets that already exist, original/custom assets, or user-installed optional packs with appropriate rights.
