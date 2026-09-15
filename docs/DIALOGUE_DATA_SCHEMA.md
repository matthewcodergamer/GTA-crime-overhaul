# Dialogue Data Contract — Roadmap 308–309

This document covers only the **data/schema/content** portion of roadmap steps **308–309**.

It does **not** implement GTA ambient speech mapping (310), runtime subtitle presentation (311), original voice packs (312), or cloned/imitated Rockstar voices (313 is a prohibition, not a feature).

## Acceptance scope

- Dialogue lives in data files, not C++ prose.
- Clerks, civilians, witnesses, police, guards and hostages have separate packs.
- Semantic event IDs are centralized in `data/dialogue/events.json`.
- Every line carries selection metadata: speaker archetypes, intensity, weight, cooldown, per-session cap, reuse group, no-repeat group, conditions, tags and delivery metadata.
- Every content-required event has a declared minimum variant count and a fallback chain.
- Silence is a valid global fallback.
- Evidence-bound witness lines require the matching fact tag; they may not invent face, vehicle or plate knowledge.
- `ambientSpeech` and `voiceAsset` remain `null` until later roadmap steps validate compatible GTA speech or authorized project-owned audio.

## Files

- `data/dialogue/base.json` — manifest and selection policy.
- `data/dialogue/schema.json` — JSON Schema for pack shape.
- `data/dialogue/events.json` — semantic event and context-tag registry.
- `data/dialogue/clerks.json`
- `data/dialogue/civilians.json`
- `data/dialogue/witnesses.json`
- `data/dialogue/police.json`
- `data/dialogue/guards.json`
- `data/dialogue/hostages.json`
- `data/dialogue/fallbacks.json`
- `tools/validate_dialogue.py` — structural/content validator.
- `tools/simulate_dialogue.py` — deterministic repetition/starvation simulation.

## Selection metadata

Each line uses:

```json
{
  "id": "clerk.comply.fast.01",
  "event": "clerk.comply.fast",
  "speakerArchetypes": ["clerk_fearful"],
  "intensity": 2,
  "weight": 1.0,
  "cooldownSeconds": 900,
  "sessionMax": 1,
  "reuseGroup": "clerk_compliance",
  "noRepeatGroup": "clerk_compliance_fast",
  "conditions": {
    "all": [],
    "any": [],
    "none": []
  },
  "tags": [],
  "delivery": {
    "type": "subtitle",
    "text": "I'm opening it.",
    "ambientSpeech": null,
    "voiceAsset": null
  }
}
```

`conditions.all` must all be present. `conditions.any` requires at least one listed context tag when non-empty. `conditions.none` must all be absent.

## Anti-repetition policy

The manifest enables:

- weighted random selection;
- line cooldowns;
- per-session usage caps;
- no-repeat groups inside one encounter/session;
- persistent reuse-group penalty;
- persistent recently-heard line penalty;
- semantic fallbacks;
- silence as the final fallback.

The simulation deliberately makes some events fire twice in the same synthetic encounter. The second selection must respect `sessionMax` and `noRepeatGroup`; if the direct pool is exhausted, it must fall back instead of repeating a distinctive line.

A CI failure occurs when:

- an event starves with no fallback;
- a content-required pool has fewer variants than declared;
- too few direct variants are ever selected;
- the same line streak exceeds the allowed QA threshold;
- fallback use exceeds the QA threshold;
- an undefined semantic/context identifier appears;
- duplicate line IDs exist;
- an unvalidated `ambientSpeech` or `voiceAsset` value is non-null.

## Current audio status

`ambientSpeechMappingStatus = not_validated`

`voicePackStatus = not_validated`

No current line claims compatibility with a GTA voice/model. Roadmap 310 must validate any ambient speech name against actual target voices before the validator is relaxed. Roadmap 312 must define licensing/metadata checks before project-owned voice assets are accepted.

## Runtime status

These files are ready for a future `DialogueDirector` to consume, but this change does not add that runtime director. The current work is intentionally engine-independent and can be tested in CI.

Manual GTA validation later should focus on pacing, subtitle readability, whether silence feels natural, and whether role/personality conditions match actual gameplay state. It must not promote audio support without separate asset validation.
