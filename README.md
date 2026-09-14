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

**Phase 0 — native foundation.** The repository starts with the production architecture, feasibility locks, master roadmap, persistent data contracts and a loadable ASI skeleton. Gameplay is then implemented vertically, beginning with one fully simulated 24/7 robbery before scaling to all stores or banks.

Read these before changing gameplay:

- `docs/DESIGN_LOCKS.md`
- `docs/MASTER_PLAN.md`
- `docs/ARCHITECTURE.md`
- `docs/FEASIBILITY_MATRIX.md`
- `docs/CRIME_LAW_SYSTEM.md`
- `docs/ROBBERY_ECONOMY_VEHICLES.md`
- `docs/BUILD_AND_RELEASE.md`

If a later idea conflicts with `docs/DESIGN_LOCKS.md`, update the lock deliberately rather than silently changing the design.

## Major systems

- Free-form store, commercial-target and bank robberies
- Witness memory with actual line-of-sight/knowledge rather than omniscience
- Unknown-suspect → investigation → identified suspect → BOLO/warrant flow
- Face, mask, clothing, vehicle model/color and plate evidence
- Persistent clerks/businesses and repeat-offender recognition
- Police return to and investigate crime scenes after the player leaves
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
