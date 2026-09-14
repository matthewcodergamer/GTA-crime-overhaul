# Robbery, Duffel, Vehicle, Loadout and Economy Systems

## Design goal

Crime should create a reason to prepare, take risks, protect assets and spend money. The project therefore links five systems that vanilla Story Mode mostly treats separately:

1. robbery target,
2. loot extraction,
3. player/vehicle equipment,
4. owned vehicles,
5. economy/progression.

---

# Robbery framework

## No universal “start mission” marker

Targets remain normal world locations until the player's actions cross a crime threshold.

Examples:

- sustained weapon aim/threat at clerk,
- firing/assault inside a protected business,
- forcing a safe/vault/security interaction,
- presenting robbery demand through contextual input,
- entering a restricted target area using robbery equipment.

Each target adapter reports the crime to the same shared `CrimeDirector`.

## Common robbery session

```text
Idle
↓
ThreatDetected
↓
RobberyActive
├─ demands
├─ witness reporting
├─ alarm/security
├─ loot interaction
└─ violence escalation
↓
PlayerLeftTarget / Arrested / Dead / Aborted
↓
Aftermath + case + business memory
```

## Store demands

Contextual demand options can include:

- open register,
- open second register,
- empty safe,
- hands up,
- get on floor,
- move away from alarm,
- don't move,
- stay down.

The interaction system should prefer short contextual controls over a giant pause-menu mission UI.

## Clerk personalities

Personality affects reaction timing/probability, not magical scripted knowledge.

- **Cowardly:** fast compliance, low resistance.
- **Compliant:** follows clear threats unless opportunity to report appears.
- **Defiant:** stalls/refuses more often.
- **Panicky:** may flee, scream or make mistakes.
- **Armed:** may fight if conditions favor them.
- **Experienced:** more likely to use panic button or follow security behavior.
- **Reckless:** unpredictable resistance.

## Repeat-business response

A business stores:

```text
robbery count
last robbery time
total losses
violence history
surviving clerk profiles
recognized faces/descriptions
recognized vehicle observations
security level
police attention
recovery/cooldown
```

Repeated robbery can produce faster alarms, cameras, guards, lower exposed cash and stronger safe behavior.

---

# Duffel bag system

## Why it matters

The bag determines how much value can physically leave a robbery. A successful vault opening does not automatically award the entire vault balance.

## Bag tiers

Initial balance targets are placeholders and must be tuned in testing.

| Tier | Capacity concept | Intended use |
|---|---:|---|
| None | pocket-scale only | small loose cash / no serious robbery |
| Small | low | convenience store / compact valuables |
| Standard | medium | commercial target / small bank |
| Large | high | bank/major-score preparation |

Capacity uses abstract units so different loot types can coexist.

Example:

```text
cash bundle: 1 unit
watch/jewelry pack: 1–2 units
bond bundle: 1 unit
heavy valuable/equipment: more units
```

## More bag = more money

A target exposes a finite set of loot sources. The player can keep collecting until:

- bag fills,
- source empties,
- player chooses to leave,
- robbery collapses because of police/violence/death/arrest.

A larger bag therefore creates a direct payout advantage.

## Time tradeoff

More value should generally require more interaction time.

```text
small bag: fills quickly, lower max take
standard: more time, higher max take
large: longest exposure, highest max take
```

This makes a large bag strategically useful rather than a free upgrade.

## Visual implementation levels

### V1

- logical bag tier,
- HUD capacity/value,
- GTA-compatible duffel visual attached when possible.

### V2

- multiple bag variants or visual fullness states if suitable assets are available.

### V3 optional

- custom original GTA-compatible bag model/animation polish.

Exact GTA VI cloth/fill behavior is not required for gameplay correctness.

## Loot lifecycle

```text
AtSource
  ↓ collected
Carried
  ├─ dropped → Dropped
  ├─ placed in owned vehicle → VehicleStored
  ├─ placed in stash → Stashed
  ├─ safe extraction/processing → Secured
  ├─ police recovery → Seized
  └─ cleanup/destruction rules → Lost
```

Major robbery proceeds are not spendable until `Secured`.

## Dropping and recovery

A loaded bag can be dropped intentionally or because of design events such as arrest/death transitions.

When nearby:

- bag exists as GTA prop/attached entity,
- player can recover it,
- police can secure it.

When far away:

- physical prop may despawn,
- project retains an abstract dropped-loot record with expiry/scene state,
- returning before seizure/cleanup reconstructs the opportunity.

---

# Bank progression

## Tier 1 — convenience/liquor/gas-station targets

Purpose:

- teach threat/compliance,
- witness reporting,
- masks,
- small cash extraction,
- police aftermath.

## Tier 2 — small commercial targets

Potential targets where GTA geometry/interiors support them:

- cash businesses,
- small office/safe encounters,
- pawn/jewelry-style targets,
- ATM/safe events,
- armored-cash opportunities.

These should be added only after the store framework is reusable.

## Tier 3 — Fleeca

### Quick teller job

- low preparation,
- immediate threat,
- lower payout,
- shorter exposure,
- strong witness count.

### Vault route

- casing/equipment/access requirement,
- more time,
- higher payout,
- more security exposure,
- bag capacity matters heavily.

### Possible after-hours route

Only if interior/security implementation is convincing:

- fewer civilians,
- different alarm/door/equipment problem,
- no free reduction in difficulty; security replaces hostage pressure.

## Tier 4 — Pacific Standard

- larger interior,
- more civilians/guards,
- multiple loot sources,
- stronger security response,
- larger bag/team planning value,
- high-value proceeds/hot-money consequences.

## Tier 5 — major score

- rare,
- expensive preparation,
- severe response,
- high long-term case risk,
- long target recovery/cooldown,
- not a repetitive cash farm.

---

# Casing system

Casing is optional for small crimes and increasingly important for banks.

A target can expose discoverable facts:

```text
front entrance
rear/service exit
clerk/teller positions
guard presence
camera coverage
alarm/security point
safe/vault location
vault access route
parking/getaway opportunities
```

Discovery is stored by target ID.

Casing should happen through world presence/observation/context interaction, not a giant checklist floating permanently on the HUD.

---

# Owned vehicle system

## Vehicle record

A persistent owned vehicle stores enough data to reconstruct the player's possession without persisting a volatile GTA handle.

```text
project vehicle ID
model hash
owner/player character
primary/secondary colors
plate string
plate style
important mod/customization state
home garage
status: available / active / impounded / destroyed / replacement pending
inventory
related crime/BOLO flags
optional damage/insurance state
```

## Why personal cars matter

An owned vehicle is:

- transportation,
- getaway choice,
- evidence risk,
- weapon/equipment storage,
- economic investment,
- impound/insurance consequence.

Repeatedly using the same distinctive car for robberies should be convenient but dangerous.

---

# Garage services

Planned services:

- repair,
- repaint,
- **change license plate**,
- storage/garage assignment,
- later performance/cosmetic upgrade integration where appropriate,
- retrieve/recover owned vehicles.

## License plate change

Interaction:

```text
Garage → Vehicle Services → Change Plate
```

Rules:

1. Vehicle must be an eligible owned vehicle unless a later illicit-service system explicitly supports stolen cars.
2. Validate string length/character set for GTA plate limitations.
3. Charge configured fee.
4. Apply plate text/style through GTA vehicle natives.
5. Update `OwnedVehicleRecord`.
6. Save immediately/mark save dirty.
7. Emit `vehicle.plate_changed`.
8. Do not mutate any historical witness/camera/case evidence.

Possible later progression:

- lawful personalized plate service,
- illicit quick plate swap with different cost/risk,
- plate style/index selection where GTA supports it.

## Repaint

Repainting updates the owned vehicle's current colors. Old reports retain the old color snapshot.

---

# Trunk inventory

## Interaction flow

1. Stand near rear/trunk interaction zone of eligible owned vehicle.
2. Hold contextual interaction.
3. Open boot/trunk door if supported.
4. Play short reach/interaction animation if safe.
5. Open minimal inventory UI.
6. Move items between player and vehicle.
7. Close UI/trunk safely.

If animation fails, inventory correctness remains primary and the system uses a simpler fallback.

## Storable items

- primary/secondary weapons,
- ammunition,
- armor,
- masks/bandanas,
- small/standard/large duffels,
- robbery tools/equipment,
- future intelligence/mission items where appropriate.

## Vehicle capacity

Can begin as slot-based and later vary by vehicle class.

A motorcycle should not logically hold the same arsenal as an SUV unless balance intentionally abstracts it.

---

# Limited weapon carry

## Goal

Replace the vanilla feeling of carrying the whole gun store with planned loadouts.

Suggested free-roam slots:

```text
Sidearm
Primary
Secondary OR large equipment slot
Throwables / small equipment
```

Exact slot balance is configurable.

## Storage hierarchy

```text
Player: immediate carried gear
Vehicle: mobile equipment reserve
Safehouse/garage: full collection
```

## Mission safety

During Rockstar missions that expect a specific weapon/loadout, enforcement is suspended or adapted. The overhaul must not soft-lock the campaign.

## Arrest/impound consequences

- carried illegal/involved weapons may be confiscated depending crime severity,
- trunk items stay with an impounded car,
- retrieving the vehicle restores access unless evidence/confiscation logic says an item was seized,
- insurance replacement does not automatically duplicate contraband.

---

# Story Mode vehicle marketplace

## Goal

Make compatible GTA Online/DLC vehicles that already exist in the player's installed build meaningful purchases in Story Mode.

The catalog must verify a model is present before offering it.

Possible categories:

- Legendary Motorsport-style high-end catalog,
- Southern San Andreas-style standard/sports/utility catalog,
- specialty/off-road/classic dealers,
- restricted/expensive military-style catalog only if balance supports it,
- used-car rotation later.

The UI is original and Rockstar-inspired; it does not redistribute Rockstar web-page assets unnecessarily.

Purchasing creates an `OwnedVehicleRecord`, not a disposable spawned car.

---

# Economy

## The problem being solved

If the player can already spawn anything or owns everything, robbery becomes pointless. Crime earnings need persistent sinks.

## Core sinks

- cars,
- garages,
- safehouses,
- vehicle repair,
- repainting,
- license plate changes,
- insurance/impound,
- weapons,
- ammunition,
- armor,
- clothing/disguises,
- masks/bandanas,
- duffel upgrades,
- robbery tools,
- storage upgrades,
- legal/fine costs,
- intelligence/preparation,
- crew assistance later.

## Money states

Suggested ledger:

```text
Pocket/clean cash
Carried robbery loot
Stashed loot
Hot/marked proceeds
Secured/cleaned proceeds
```

Not every store robbery needs tedious laundering. The distinction becomes more important at large-bank scale.

Example high-value score:

```text
Gross extracted:      $460,000
Immediate clean:       $45,000
Hot/marked:           $415,000
```

The exact split is a balance decision, not a hard-coded promise.

## Spending feedback loop

```text
Want valuable car / garage / equipment
              ↓
Prepare crime
              ↓
Rob target
              ↓
Escape and secure loot
              ↓
Manage case / vehicle evidence
              ↓
Spend / upgrade / prepare larger crime
```

This is the main anti-boredom loop.

---

# Presentation

## HUD principles

Do not turn the game into a debug dashboard.

During robbery show only what matters, for example:

```text
DUFFEL
$84,260     72%

WITNESS
Civilian reporting crime
```

During law aftermath:

```text
POLICE INVESTIGATING
Suspect: UNKNOWN
Vehicle: DARK SPORTS COUPE
Plate: UNKNOWN
```

If identified:

```text
ACTIVE WARRANT
FRANKLIN CLINTON
Vehicle BOLO: SENTINEL
Observed plate: 46EEK572
```

Garage service:

```text
CHANGE PLATE
Current: 46EEK572
New:     L8RBOZO
Cost:    $1,250
```

Exact UI art is original and restrained.

---

# Animation policy

Robbery correctness never depends on a cinematic-perfect animation.

Use existing GTA V tasks/anim dictionaries for:

- hands up/cower,
- phone/reporting,
- register/safe/keypad interaction approximations,
- grab/pickup,
- carrying/duffel,
- trunk open/reach,
- surrender/arrest,
- police/witness conversation idles.

Every sequence has:

- dictionary load timeout,
- entity validity checks,
- interruption handling,
- cancellation cleanup,
- fallback behavior.

Custom original animation/model content is optional polish after systems work.

---

# Final acceptance loop

The player should eventually be able to:

1. buy or choose an owned vehicle,
2. put a large duffel and rifle in the trunk,
3. change the plate before a job if desired,
4. wear a mask/bandana,
5. case a Fleeca bank,
6. retrieve equipment from the trunk,
7. initiate the robbery naturally,
8. control teller/crowd/security behavior,
9. physically fill the duffel,
10. leave early or stay for more loot,
11. be observed differently by separate witnesses/cameras,
12. escape a search based on last-known information,
13. abandon/swap/repaint/change plate as countermeasures,
14. recover or lose a dropped bag,
15. secure the proceeds,
16. face a persistent warrant if identified,
17. return to a previously robbed business and encounter its memory/security changes,
18. spend the profit on vehicles, garages, equipment and preparation for larger jobs.
