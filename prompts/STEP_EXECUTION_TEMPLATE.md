# GTA Crime Overhaul — Numbered Step Execution Template

Use this when implementing any single numbered item or a very small contiguous group from `docs/MASTER_PLAN.md`.

```text
You are implementing GTA Crime Overhaul roadmap step(s): [NUMBER(S)].

Repository:
matthewcodergamer/GTA-crime-overhaul

First read:
- prompts/MASTER_AGENT_PROMPT.md
- docs/DESIGN_LOCKS.md
- the exact section of docs/MASTER_PLAN.md containing these steps
- the owning system document(s)
- docs/RESEARCH_IMPLEMENTATION_PLAN.md
- docs/ASSET_ANIMATION_CATALOG.md if the step touches GTA assets/animations/interiors/audio
- docs/DIALOGUE_BIBLE.md if the step emits dialogue

Task:
Implement ONLY roadmap step(s) [NUMBER(S)] to production quality, plus the smallest prerequisite/fix needed to make those steps correct.

Before coding:
1. Quote/summarize the exact acceptance requirement from MASTER_PLAN.
2. Identify the owning module/class/data file.
3. List GTA-specific dependencies: natives, model names, animation dictionaries/clips, speech, doors/interiors, coordinates, bones, vehicle behavior.
4. Classify each dependency VERIFIED_IN_GAME, VERIFIED_DATA, REFERENCE_ONLY, CUSTOM_REQUIRED, REJECTED, or not-yet-researched.
5. Never invent missing identifiers. If anything is unverified, create a debug/research path and safe fallback instead of pretending it works.

Implementation requirements:
- native C++20 Script Hook V ASI architecture;
- offline Story Mode only;
- no raw GTA entity handles in persistent data;
- survive streaming/entity deletion;
- support mission/cutscene suspension where relevant;
- include cleanup on abort/death/ragdoll/unload;
- preserve evidence history and economy correctness;
- avoid per-frame expensive scans when an event/staggered update works;
- keep UI/dialogue data-driven;
- do not duplicate logic already owned by another director/system.

Validation:
- compile x64 Release;
- run available unit/domain tests;
- add/extend tests where logic is engine-independent;
- create exact manual GTA test instructions for anything requiring in-game validation;
- state Legacy/Enhanced status separately if relevant;
- test at least one failure/interrupt path, not only the happy path.

Output:
### Roadmap steps
[exact numbers]

### Research status
[dependency table]

### Files changed
[list]

### Implementation
[what changed and why]

### Automated/build validation
[results]

### Manual GTA validation still required
[exact steps and pass/fail criteria]

### Risks/fallbacks
[list]

### Next roadmap step
[next safe number(s)]
```

## Rule for step ranges

Only group steps when they form one atomic system change. Examples:

- Good: `81–87` witness observation channels.
- Good: `117–123` garage plate-changing behavior.
- Good: `191–196` bag tiers/equip/visual attachment.
- Bad: `77–128` because witness, identity and vehicle systems become too large to review safely.

## Research-only step variant

When a step cannot honestly be implemented before GTA testing, use this variant:

```text
Do not implement production gameplay yet. Research roadmap step [NUMBER].
Create the smallest debug lab/data manifest necessary to verify the required GTA V native/asset/coordinate/animation behavior.
Record candidates using the repository status vocabulary.
Leave production feature disabled until VERIFIED_IN_GAME.
Return the exact in-game procedure that promotes the candidate from VERIFIED_DATA/CANDIDATE to VERIFIED_IN_GAME.
```

## Content-authoring step variant

For dialogue/data-heavy steps:

```text
Implement the data/schema/content portion of roadmap step [NUMBER] without embedding prose into C++.
Use semantic tags, conditions, weights, cooldowns, no-repeat/reuse groups and fallbacks.
Validate schema and simulate repeated selection to detect repetition/starvation.
Do not claim voice/audio support until compatible GTA speech or an authorized custom voice asset has been validated.
```
