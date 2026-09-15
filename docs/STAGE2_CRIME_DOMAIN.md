# Stage 2 — Crime Domain and Persistent Case Model

Roadmap scope: **47–59**.

This stage is deliberately store-agnostic. It creates the shared crime/case simulation that later store, bank, witness, vehicle and police systems feed. It does not contain store coordinates, clerk logic, robbery thresholds, police spawning, GTA animations, speech, doors or interior data.

## Acceptance mapping

| Step | Implementation |
|---|---|
| 47 | `CrimeDirector` is the lifecycle facade and publishes semantic runtime events. |
| 48 | `CrimeRegistry` owns unique `Case` and `Crime` logical IDs. `Crime` was appended as logical-ID domain 6 without renumbering Stage 0 domains. |
| 49 | `CrimeType`: robbery, armed robbery, assault, homicide, officer assault, vehicle theft and property damage. |
| 50 | Each crime has base severity; escalation is monotonic and a case severity is the maximum related crime severity. |
| 51 | `CrimeEvent` stores logical IDs, location, optional target business ID and occurrence timestamp. |
| 52 | `ImmediateResponseState` is separate from `CaseState`. Tactical response can end while the case remains active/dormant. |
| 53 | Canonical case states are represented explicitly and transitions are validated by `canTransition`. |
| 54 | Evidence records contain source, kind, confidence, immutable value snapshot, observation timestamp, source-family key and exact dedup key. |
| 55 | Related crimes merge only through an explicit non-empty `incidentKey`; Stage 2 does not guess that nearby crimes are the same event. Evidence duplicates are suppressed, while independent sources strengthen confidence. |
| 56 | Weak Minor/Moderate cases can expire after policy thresholds. Major/Critical cases and cases with an active warrant/BOLO are not automatically erased. Resolution preserves the historical case/evidence records. |
| 57 | Cases/crimes/evidence save inside `world.json` through `CrimePersistenceStore` and the existing atomic/backup `WorldStateStore`. |
| 58 | `CrimeDebugInspector` formats the full registry or one case; runtime command `crime.inspect` / F5 writes it to the log. |
| 59 | Runtime command `crime.synthetic` / F6 creates a synthetic incident, merges a related crime, adds evidence, transitions the case, saves it, reloads into a fresh registry and verifies the reconstructed state before reporting PASS. |

## Ownership

```text
CrimeDirector
  semantic lifecycle facade
  event publication
        |
        v
CrimeRegistry
  owns CaseFile + CrimeEvent
  incident correlation
  transition validation
  evidence insertion/fusion
  severity escalation
  decay/resolution
        |
        v
CrimePersistenceStore
  project-owned JSON codec
  per-case model-version validation
  world.json cases section
  atomic save delegated to WorldStateStore
```

Store/bank code must not create a parallel case model. Later gameplay systems call `CrimeDirector` and submit facts/evidence to the shared registry.

## Stable logical IDs

Stage 0 persisted these domain values:

```text
Case          = 1
Business      = 2
Clerk         = 3
Vehicle       = 4
LootContainer = 5
```

Stage 2 appends:

```text
Crime         = 6
```

The existing numbers are compatibility locks. Do not renumber them.

A crime ID and case ID are different. Several crime records may belong to one case when their caller supplies the same unresolved `incidentKey`.

## Merge rule

Stage 2 intentionally does **not** merge on only distance/time proximity. That can incorrectly combine unrelated free-roam events.

The producer owns an incident/session correlation key. Example later:

```text
robbery session 502
  -> armed robbery
  -> property damage
  -> assault
```

All three can use the same incident key and therefore strengthen/escalate one case.

A resolved case no longer owns the active incident-key mapping. Reusing the same key afterward creates a new case rather than silently reopening resolved history.

## Immediate response vs persistent case

`ImmediateResponseState` is tactical/session state:

- active response;
- report pending;
- immediate pursuit active;
- tactical response level;
- last tactical update time.

`CaseState` is persistent investigative/legal state.

Ending an immediate GTA wanted response must not automatically resolve or delete the long-term case.

## Canonical persistent state flow

The normal path is:

```text
Unobserved
 -> Observed
 -> Reporting
 -> Reported
 -> Investigating
 -> UnknownSuspect OR IdentifiedSuspect
 -> BoloOrWarrant when appropriate
 -> PursuitOrSearch when an encounter/search is active
 -> Dormant when immediate activity ends
 -> Resolved only through explicit resolution/expiry policy
```

A few validated recovery transitions exist, such as an interrupted report returning to `Observed`, or a dormant case being reactivated later. `Resolved` is terminal in Stage 2.

## Evidence immutability and confidence

An `EvidenceRecord` is appended to a case and exposed to the rest of the game only through const case access.

Historical evidence contains:

- source;
- evidence kind;
- confidence `[0,1]`;
- `observedAtMs`;
- stable snapshot descriptor/value fields;
- snapshot location;
- optional logical source/vehicle IDs;
- `independenceKey`;
- `dedupKey`.

There are no persisted GTA entity handles.

### Duplicate suppression

Exact duplicate evidence is not appended twice. The existing historical record is not rewritten.

### Source-family confidence

Repeated updates from the same source family do not magically multiply certainty. For a requested evidence kind, the registry first keeps the highest confidence from each `independenceKey`, then combines independent source families using remaining uncertainty:

```text
combined = 1 - product(1 - independentConfidence)
```

This is a basic Stage 2 fusion rule, not a final identity/forensics engine. Later evidence systems may add type-specific policies without mutating old records.

## Severity

Base severity is currently:

```text
PropertyDamage -> Minor
Robbery        -> Moderate
Assault        -> Moderate
VehicleTheft   -> Moderate
ArmedRobbery   -> Serious
Homicide       -> Major
OfficerAssault -> Major
```

Severity can only escalate through the escalation API. Adding another crime to the same incident automatically raises the case to the highest related severity.

## Decay

Default Stage 2 policy:

- weak Minor case: eligible after 24 hours;
- weak Moderate/Serious case: eligible after 72 hours;
- strongest evidence must be at or below `0.35`;
- Major/Critical cases never auto-expire through this rule;
- active person warrants and vehicle BOLOs block weak-case expiry.

Expiry means `Resolved / ExpiredWeakEvidence`; it does **not** delete historical evidence.

The policy is project-owned and can be tuned later without adding per-frame work. Housekeeping runs at 1 Hz and only saves when a case actually changes.

## Persistence compatibility

The top-level world save stays `schemaVersion: 1` because Stage 0 deliberately reserved the `cases` array for later stages. An old valid Stage-0 world with `cases: []` remains loadable.

Every Stage 2 case carries:

```json
"modelVersion": 1
```

This lets the case codec reject an unknown future case representation independently of unrelated world sections.

`CrimePersistenceStore` parses the complete JSON document and replaces only the `cases` section plus the relevant `nextIds` counters before using the existing atomic world-save writer. Unknown top-level fields are preserved.

64-bit logical IDs are parsed/stored as integer tokens rather than floating-point values.

On load, next case/crime counters are raised to at least one more than the maximum persisted ID, even if a counter is stale. Older saves with no `nextIds.crime` field start safely from persisted crime IDs or 1.

## Performance

Stage 2 is overwhelmingly event-driven.

- Creating/escalating crimes: event-driven.
- Adding evidence: event-driven.
- State transitions: event-driven.
- Case inspector: debug-only/on demand.
- Decay housekeeping: 1 Hz.
- No world/ped/vehicle scan is introduced by this stage.

## Debug validation

Requires `DebugHotkeys=true` and `PersistentCases=true`.

### F5 — inspector

Writes every case, related crime and evidence record to `GTA_Crime_Overhaul.log`.

### F6 — synthetic Stage 2 proof

The diagnostic deliberately uses no shop/business logic. It:

1. samples only the player's current position through the existing Stage 1 adapter;
2. creates a synthetic `ArmedRobbery` with a unique debug incident key;
3. creates synthetic `PropertyDamage` using the same key and verifies one case/two crimes;
4. sets independent immediate tactical state;
5. appends two synthetic evidence records;
6. transitions through reporting, reported, investigating, unknown suspect, vehicle BOLO, pursuit/search and dormant;
7. ends immediate response without resolving the case;
8. atomically saves the world;
9. creates fresh ID/registry objects and reloads from disk;
10. requires the same case ID to reload as Dormant with two crimes, two evidence records and an active vehicle BOLO;
11. logs `Synthetic Stage 2 diagnostic PASS` and runs the inspector.

The command is blocked by the mission-compatibility gate. It does not spawn GTA actors, dispatch police, alter a business, award money or start a robbery.

## Manual in-game pass/fail

Run separately on Legacy and Enhanced where the current Script Hook V runtime supports the edition.

Pass criteria:

1. Enter normal controllable Story Mode free roam.
2. Press F5: inspector runs without error.
3. Press F6: on-screen PASS appears and the log contains `Synthetic Stage 2 diagnostic PASS`.
4. Press F5: the new case appears as `dormant`, with 2 crimes, 2 evidence records and vehicle BOLO=yes.
5. Press F11: save validation reports PASS including Stage 2 cases.
6. Exit GTA normally, relaunch, press F5 and verify the same logical case ID and historical evidence reload.
7. During a Rockstar mission/cutscene, press F6 and verify the diagnostic is blocked rather than mutating case state.

Fail criteria include a crash/hang, duplicate case for the two related synthetic crimes, changed IDs after restart, evidence loss, raw GTA handles in the save, case disappearing when immediate response ends, save validation failure, or F6 mutating state during an incompatible mission.

## GTA-specific research status

No new GTA asset/model/animation/interior/audio identifier is required by roadmap 47–59.

The optional F6 position sample uses the already-established Stage 1 world adapter. Domain correctness, persistence, state transitions and evidence logic are engine-independent and covered by unit tests. Therefore there is no asset that can honestly be promoted to `VERIFIED_IN_GAME` by this stage.
