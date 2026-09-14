# Runtime Architecture

## Layering

```text
GTA_Crime_Overhaul.asi
│
├─ Platform / GTAV adapter
│  ├─ ScriptHookLifecycle
│  ├─ NativeInvoker / NativeFacade
│  ├─ InputAdapter
│  ├─ AnimationAdapter
│  ├─ AudioAdapter
│  ├─ UIAdapter
│  ├─ InteriorDoorAdapter
│  └─ WorldQueryAdapter
│
├─ Core runtime
│  ├─ Runtime
│  ├─ Scheduler
│  ├─ EventBus
│  ├─ Logger
│  ├─ Config
│  ├─ Persistence
│  ├─ IdGenerator
│  └─ MissionCompatibilityGate
│
├─ Crime domain
│  ├─ CrimeDirector
│  ├─ CrimeRegistry
│  ├─ CaseRepository
│  ├─ EvidenceSystem
│  ├─ WitnessDirector
│  ├─ IdentitySystem
│  ├─ VehicleIdentitySystem
│  ├─ DispatchDirector
│  ├─ InvestigationDirector
│  ├─ PursuitDirector
│  └─ WarrantSystem
│
├─ Robbery domain
│  ├─ RobberyDirector
│  ├─ BusinessRegistry
│  ├─ ClerkController
│  ├─ StoreRobberyController
│  ├─ BankRobberyController
│  ├─ SafeVaultController
│  └─ LootDirector
│
├─ Vehicle / inventory domain
│  ├─ OwnedVehicleRepository
│  ├─ GarageService
│  ├─ PlateService
│  ├─ VehicleInventory
│  ├─ ImpoundService
│  └─ LoadoutService
│
├─ Economy domain
│  ├─ EconomyLedger
│  ├─ DealerCatalog
│  ├─ PurchaseService
│  ├─ HotMoneyService
│  └─ LegalCostService
│
└─ Presentation
   ├─ HudPresenter
   ├─ DebugPresenter
   ├─ DialogueDirector
   └─ NotificationPresenter
```

## Hard architecture rules

1. Domain code never persists GTA entity handles.
2. Domain code prefers interfaces/value snapshots over direct native calls.
3. A robbery never owns the police system; it emits crime/events into the shared simulation.
4. Witnesses create immutable observation snapshots.
5. Changing the live world later never rewrites historical evidence.
6. Distant scenes are data, not always-live NPCs.
7. Every long animation/task has timeout/cancellation recovery.
8. Save/load code knows schema versions; gameplay classes do not parse arbitrary JSON everywhere.
9. Player-facing UI reads domain state; UI does not become the source of truth.
10. Debug commands may force states but are excluded from normal gameplay logic.

## Runtime tick model

### Every frame

Use only for:

- Script Hook V `WAIT(0)` loop
- critical input edge detection
- immediately active interaction prompts
- current robbery animation/task supervision
- minimal HUD render

### 5 Hz queue

- nearby witness candidate refresh
- current crime-zone entity validity
- current player/vehicle snapshot refresh
- nearby target interaction availability

### 2 Hz queue

- witness perception updates (staggered groups)
- police search planner
- business scene controller
- nearby dropped-loot simulation

### 1 Hz queue

- case/warrant housekeeping
- business cooldown/recovery
- persistence dirty-state checks
- owned vehicle reconciliation

### Event-driven

- crime created/escalated
- witness begins/finishes report
- evidence added
- suspect identified
- plate changed
- vehicle repainted
- bag dropped/seized/secured
- arrest/surrender
- store robbery starts/ends
- mission compatibility suspension/resume

## Domain data contracts

### CrimeEvent

```cpp
enum class CrimeType {
    Robbery,
    ArmedRobbery,
    Assault,
    Homicide,
    OfficerAssault,
    VehicleTheft,
    PropertyDamage
};

struct CrimeEvent {
    uint64_t id;
    CrimeType type;
    uint64_t gameTimeMs;
    Vec3 location;
    std::optional<uint64_t> businessId;
    int severity;
};
```

### WitnessObservation

```cpp
struct WitnessObservation {
    uint64_t witnessLogicalId;
    uint64_t caseId;
    float confidence;

    bool sawFace;
    bool sawMask;
    std::optional<OutfitSignature> outfit;
    std::optional<WeaponClass> weapon;
    std::optional<VehicleEvidence> vehicle;

    Vec3 lastKnownLocation;
    float lastKnownHeading;
    uint64_t observedAtMs;
};
```

### VehicleEvidence

```cpp
struct VehicleEvidence {
    uint32_t modelHash;
    int primaryColor;
    int secondaryColor;
    std::optional<std::string> plate;
    std::optional<int> plateStyle;
    float modelConfidence;
    float colorConfidence;
    float plateConfidence;
};
```

**Important:** this is a historical snapshot. A later garage plate change never mutates it.

### CaseFile

```cpp
enum class SuspectKnowledge {
    Unknown,
    DescriptionOnly,
    ProbableIdentity,
    Identified
};

struct CaseFile {
    uint64_t id;
    std::vector<uint64_t> crimeIds;
    SuspectKnowledge suspectKnowledge;
    float identityConfidence;
    bool activePersonWarrant;
    bool activeVehicleBolo;
    SearchState search;
    std::vector<EvidenceRecord> evidence;
};
```

### OwnedVehicleRecord

```cpp
struct OwnedVehicleRecord {
    uint64_t id;
    uint32_t modelHash;
    std::string plate;
    int plateStyle;
    int primaryColor;
    int secondaryColor;
    VehicleModsSnapshot mods;
    VehicleStatus status;
    VehicleInventory inventory;
    std::vector<uint64_t> relatedCaseIds;
};
```

### LootContainer

```cpp
enum class LootState {
    AtSource,
    Carried,
    Dropped,
    VehicleStored,
    Stashed,
    Secured,
    Seized,
    Lost
};

enum class BagTier {
    None,
    Small,
    Standard,
    Large
};

struct LootContainer {
    uint64_t id;
    BagTier bag;
    int capacityUnits;
    int usedUnits;
    int cashValue;
    LootState state;
};
```

## Event examples

```text
crime.created
crime.escalated
witness.observation_created
witness.report_started
witness.report_completed
case.evidence_added
case.suspect_identified
case.person_warrant_issued
case.vehicle_bolo_issued
vehicle.plate_changed
vehicle.paint_changed
vehicle.impounded
loot.bag_equipped
loot.bag_dropped
loot.seized
loot.secured
business.robbery_started
business.robbery_finished
business.security_level_changed
player.surrendered
player.arrested
runtime.mission_suspend
runtime.mission_resume
```

## Plate service

`PlateService` is intentionally separate from garage UI.

```text
Garage UI
  ↓ request
PlateService::Validate
  ↓
EconomyLedger::CanAfford
  ↓
Native vehicle plate update
  ↓
OwnedVehicleRepository update
  ↓
Event: vehicle.plate_changed
  ↓
Persistence dirty
```

No call is made to mutate old `VehicleEvidence` records.

## Robbery composition

A store/bank adapter defines **world facts**:

```text
location volume
clerk/teller anchors
register/loot anchors
safe/vault anchors
door IDs/entities
camera zones
guard anchors
customer zones
entrances/exits
security profile
```

The shared systems supply:

```text
witness perception
identity recognition
crime creation
police dispatch
investigation
case persistence
duffel capacity
vehicle evidence
economy
```

That prevents each new bank from becoming a second incompatible police game.

## Persistence layout

Planned runtime layout:

```text
Grand Theft Auto V/
├─ GTA_Crime_Overhaul.asi
└─ GTA_Crime_Overhaul/
   ├─ config.ini
   ├─ data/
   │  ├─ businesses.json
   │  ├─ dialogue/
   │  └─ economy.json
   ├─ saves/
   │  ├─ world.json
   │  └─ world.backup.json
   └─ logs/
      └─ GTA_Crime_Overhaul.log
```

`world.json` eventually contains separate top-level schema-versioned sections for:

- cases
- business memory
- logical clerks
- owned vehicles
- inventories/loadouts
- loot/stashes
- economy
- casing discoveries
- settings requiring persistence

## Mission compatibility

`MissionCompatibilityGate` exposes three levels:

- `Normal`: all systems allowed.
- `Restricted`: persistence/case bookkeeping continues but spawning, inventory enforcement or robbery prompts are suspended.
- `Suspended`: Crime Overhaul yields control during known incompatible mission/cutscene state.

On resume, the runtime revalidates all live GTA entities before continuing.

## No raw-handle persistence

Wrong:

```json
{ "clerkPed": 3124, "vehicleHandle": 884 }
```

Correct:

```json
{
  "clerkLogicalId": 42,
  "profile": { "modelHash": 123, "businessId": 5 },
  "ownedVehicleId": 19
}
```

Handles are reacquired/spawned only for the current session.
