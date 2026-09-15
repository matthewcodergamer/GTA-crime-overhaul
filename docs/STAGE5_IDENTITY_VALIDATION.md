# Stage 5 — Identity, masks, clothing and recognition validation

Roadmap: **96–111**

## Production invariants

- Identity logic consumes `PedSnapshot` values; GTA native calls remain in the platform adapter layer.
- Outfit signatures are stable across mask/hair changes and include clothing components/props relevant to visual matching.
- Only `VERIFIED_IN_GAME` entries from `data/identity/mask_catalog.json` may block new face capture.
- A mask blocks **future** face capture only. Historical face/identity evidence and persistent clerk face memory are immutable.
- Clothing remains observable while masked.
- Changing clothes weakens clothing matching but never clears a confirmed face identity.
- Clothing alone may create suspicion; it never upgrades a suspect to confirmed identity.
- Persistent clerk memory belongs to the logical clerk ID. If the clerk dies and is replaced, the new clerk starts with no inherited personal memory.
- Vehicle/plate evidence is independent of masks and remains owned by the witness/vehicle evidence systems.
- RDR2 mask/bandana assets are never redistributed.

## Current content status

`data/identity/mask_catalog.json` intentionally has an empty production `approved` list until exact clothing entries are visually validated on supported player models/builds.

Freemode face-bandana/combat-mask entries are `VERIFIED_DATA` research candidates only. They are not assumed to fit Franklin, Michael or Trevor and cannot suppress face capture in production.

The protagonist lower-face bandana path is currently **`CUSTOM_REQUIRED`**. If no native GTA V lower-face option passes validation, create an original GTA-compatible asset. Do not port the RDR2 bandana.

## Automated acceptance

`IdentitySystemTests` must prove:

1. mask/hair changes do not alter the stable outfit signature;
2. clothing changes do alter/weaken the clothing match;
3. only `VERIFIED_IN_GAME` catalog entries can suppress new face capture;
4. an unapproved/reference-only mask candidate cannot suppress face capture;
5. a strong face observation remains stored after the player puts on a mask;
6. changing clothes does not clear confirmed face identity;
7. clothing alone produces suspicion, not confirmed identity.

`StorePersistenceTests` must prove:

1. face and clothing memory round-trip for the same logical clerk;
2. Stage-3 `modelVersion: 1` store saves load with empty Stage-5 recognition memory;
3. a replacement clerk gets a new logical ID and does not inherit the dead clerk's personal face/outfit memory.

`WitnessDomainTests` must continue proving that hearing never grants visual identity and that previously captured face identity remains present if a mask is observed later.

## Manual in-game validation

Run separately on Legacy and Enhanced after the prototype store target itself is `VERIFIED_IN_GAME`.

### Unmasked first robbery

1. Enter the prototype store unmasked as Franklin/Michael/Trevor.
2. Let the same clerk obtain a sustained, front-facing LOS observation during the robbery.
3. Complete/allow the clerk report.
4. Inspect logs/case evidence and `identity.inspect` / `store.inspect`.
5. PASS: face identity + clothing are recorded only when observation quality permits; clerk memory persists after leaving/reloading.

### Mask before exposure

1. Equip a catalog entry only after it has been promoted to `VERIFIED_IN_GAME` for that exact player model/build.
2. Enter/rob while the clerk never saw the uncovered face first.
3. PASS: mask/clothing can be observed, but no new face identity is captured.
4. FAIL: the system identifies the character merely from the loaded player model while the approved mask blocks the face.

### Mask after face exposure

1. Let the clerk clearly observe the uncovered face.
2. Put on an approved mask while the same incident remains active.
3. Continue the robbery/report.
4. PASS: no new covered-face capture is added, but the earlier face/identity observation remains unchanged.
5. FAIL: applying the mask removes or downgrades old face evidence.

### Outfit counterplay

1. Let the clerk remember a strong outfit and, separately, a strong face identity.
2. Leave, change several major clothing components, and return to the same surviving clerk.
3. PASS: clothing match drops. If the face is visible and already confirmed, face recognition can still succeed.
4. With the face hidden, PASS: clothing alone may cause suspicion but must not recreate a live face match.

### Repeat-clerk behavior

1. Return to the same surviving clerk after prior strong observations.
2. PASS: recognition progresses through subtle `Notice`, `StarePause`, `BackAway`, or `SilentAlarmPossible` behavior rather than instantly starting a robbery or magically informing police.
3. Kill the clerk, allow replacement, return later.
4. PASS: the new logical clerk does not inherit the dead clerk's personal memory; the business incident history still exists separately.

## Exit criterion

Stage 5 is runtime-validated only when the same prototype store demonstrates:

- unmasked robbery → supported face identification;
- masked-before-exposure robbery → clothing/vehicle may be known, face not newly identified;
- mask added after face exposure → old identity remains;
- clothing change weakens clothing match without clearing confirmed face identity;
- replacement clerk does not inherit a dead clerk's personal recognition memory.
