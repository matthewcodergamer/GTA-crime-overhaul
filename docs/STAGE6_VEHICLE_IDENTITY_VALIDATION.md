# Stage 6 vehicle identity / garage validation

Roadmap scope: 112–128 only. This stage does not implement Stage 8 police search AI, Stage 12 trunk inventory, or the Stage 14 full economy.

## Ownership and evidence rules

- `OwnedVehicleRecord.id` is the persistent project identity. A GTA `Vehicle` handle is only a transient runtime binding and is never serialized.
- Owned, temporary and stolen records remain distinct. Seeing/using a temporary or stolen getaway car does not make it owned.
- Vehicle reconstruction state stores model hash/display label, mod kit/slots/toggles, wheel type, tint, livery, colors, plate/style, storage/status and crime-history associations.
- Historical case evidence is immutable. Garage mutations update only the current vehicle record and live GTA entity.
- Vehicle BOLO state is persisted separately from `activePersonWarrant`. BOLO fields are rebuilt only from case evidence, not from omniscient live-player sampling.
- Plate replacement defeats direct old-plate matching. Paint replacement defeats direct old-color matching. A separately established physical/logical continuity link can remain stronger than either appearance channel.
- Vehicle swaps preserve continuity only when a real observation source explicitly reports the swap. Unobserved swaps break the physical-continuity shortcut without deleting historical evidence.

## Plate rules

The project uses a conservative GTA-safe subset until Legacy and Enhanced are both validated in game:

- 1–8 characters after normalization.
- `A-Z`, `0-9`, and internal spaces only.
- Lowercase normalizes to uppercase.
- Leading/trailing spaces are removed and repeated spaces collapse.
- Unsupported punctuation is rejected.
- Plate style is constrained to indices 0–5 by the Stage 6 service.

This intentionally avoids claiming every GTA font/build glyph is safe.

## Service transaction

Default Change License Plate price: `$500` (tuning value; no canonical design price was specified by the roadmap).

Transaction order:

1. Validate logical vehicle/live-handle binding and plate text/style.
2. Read Story Mode cash and reject insufficient funds.
3. Charge once.
4. Apply the plate to the live GTA vehicle.
5. Update the logical current vehicle record.
6. Atomically save vehicle state.
7. If native mutation or persistence fails, restore the old live/current state and refund.

The service never receives a mutable case object, so it has no path that can rewrite historical evidence.

## Automated acceptance test

`VehicleIdentityTests` covers the required regression:

1. Owned logical car starts as `ABC123`, colors `27/0`.
2. Witness case evidence snapshots `plate=ABC123` and the old model/colors.
3. Separate vehicle BOLO snapshots the same reported facts.
4. Garage transaction changes the current live/logical plate to `XYZ789` and charges the configured price.
5. Assert live/current record is `XYZ789`.
6. Assert case evidence is still `plate=ABC123`.
7. Assert BOLO historical plate is still `ABC123`.
8. Assert direct old-plate match is false after the change.
9. Repaint the vehicle and assert historical color evidence remains unchanged.
10. Save/reload and verify current plate/paint/mod/storage/crime-history state persists while the historical BOLO remains old.
11. Verify temporary/stolen records never auto-promote to owned.
12. Verify observed swap preserves continuity and unobserved swap breaks it without rewriting historical BOLO fields.

## Manual GTA validation still required

Run separately on supported GTA V Legacy and Enhanced builds. Do not mark `VERIFIED_IN_GAME` from CI alone.

1. Load Story Mode in normal free roam with compatible Script Hook V installed separately.
2. Use an owned/test vehicle and verify capture of model, colors, current plate/style and modification reconstruction data.
3. Save, reload the game/mod and confirm the logical vehicle returns with the same project ID and reconstruction record; no GTA handle appears in the save.
4. Produce a real witness plate/model/color report during the prototype robbery and confirm a vehicle BOLO is created without creating a person warrant.
5. In a garage/mod-shop integration path, request `XYZ789` from a current `ABC123` vehicle and confirm the player is charged exactly once.
6. Confirm the visible GTA plate becomes `XYZ789`, the logical current record saves `XYZ789`, and the earlier case still reports `ABC123`.
7. Repaint the vehicle and confirm current colors change while the earlier witness report keeps the original color snapshot.
8. Confirm an old-plate lookup no longer directly matches after the plate change unless another evidence channel/physical-continuity link is strong enough.
9. Swap vehicles while unobserved and confirm physical continuity is broken; repeat with an explicit observed-swap event and confirm continuity is preserved.
10. Use/destroy/stream out a temporary or stolen getaway vehicle and confirm no stale GTA handle is persisted and the record does not become owned.
11. Enter a mission/cutscene and confirm Stage 6 gameplay sampling pauses under the mission compatibility gate.
12. Exit/reload normally and verify `vehicle_identity.json` remains valid; repeat after corrupting primary with a valid backup to validate recovery.

### Status vocabulary

- Domain/unit behavior after green CI: `VERIFIED_DATA` / automated behavior verified.
- Script Hook V native names/signatures: SDK-compiled, but runtime effect remains `REFERENCE_ONLY` until Legacy/Enhanced smoke tests.
- Live plate, paint, reconstruction and Story Mode cash mutation: not `VERIFIED_IN_GAME` until the manual tests above pass.
