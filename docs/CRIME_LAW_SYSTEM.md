# Crime, Witness, Police and Investigation System

## Why this exists

Vanilla GTA V mainly treats criminal consequences as an immediate wanted-level problem. GTA Crime Overhaul adds a second layer: a persistent **case** that can survive the chase and is built from what witnesses, cameras and officers actually know.

The player can therefore:

- commit a crime nobody can identify,
- be reported as an unknown masked suspect,
- be known only by vehicle/plate,
- be personally identified,
- escape the immediate pursuit but keep a warrant,
- change clothing/vehicle/plate to reduce some forms of recognition,
- surrender and face consequences rather than always fighting to the death.

## State model

```text
NO CASE
  ↓ crime occurs
UNREPORTED
  ↓ witness/alarm/camera report completes
REPORTED
  ↓ dispatch
INVESTIGATING
  ├─ identity insufficient → UNKNOWN SUSPECT
  └─ identity sufficient   → IDENTIFIED SUSPECT
                                ↓
                         PERSON WARRANT / VEHICLE BOLO
                                ↓
                         ENCOUNTER / PURSUIT
                                ↓ LOS lost
                              SEARCH
                                ↓
                       DORMANT ACTIVE CASE
                                ↓
                    ARREST / DECAY / RESOLUTION
```

The built-in GTA wanted level is treated as an **immediate tactical signal**. Our case state is the long-term simulation.

## Witness candidate selection

Never scan every ped every frame.

A crime scene owns a bounded perception area. At a low frequency the witness system:

1. Queries nearby peds.
2. Rejects dead/invalid/mission-owned/irrelevant peds.
3. Uses distance and rough direction filters.
4. Only then performs expensive visibility/LOS checks.
5. Staggers witnesses across updates.

Witnesses are not guaranteed reporters. Personality, fear, injuries, distance and interruption affect behavior.

## What a witness can know

`WitnessObservation` can contain:

- face visible/not visible,
- mask/bandana visible,
- clothing signature,
- player character description category,
- weapon class,
- vehicle model,
- vehicle colors,
- license plate,
- direction of travel,
- last known location,
- approximate number of offenders for future crew support,
- confidence per observation.

A witness who heard shots from the back room may know **a violent crime occurred** but know nothing about the face or vehicle.

A clerk staring at an unmasked player for ten seconds can provide much stronger face evidence.

A pedestrian who only sees the getaway vehicle can create a vehicle-only report.

## Reporting

Possible report paths:

- mobile phone call,
- business panic button,
- alarm triggered by safe/vault/door system,
- officer directly witnesses offense,
- project-controlled CCTV/security system creates an alarm,
- later discovery of a body/abandoned vehicle where appropriate.

Reporting is timed. If a witness is running to safety or calling, the report can take several seconds. The information submitted is frozen from the witness's knowledge at report time.

## Confidence

Suggested interpretation:

```text
0.00–0.20  weak/unreliable
0.20–0.50  possible match
0.50–0.75  strong description
0.75–0.95  probable identity
0.95–1.00  confirmed/near-certain project-level identification
```

Do not simply add percentages. Evidence fusion should account for independence and type. Three witnesses all repeating the same weak plate fragment should not become magical proof.

## Face identity

Face recognition is a **simulation**, not computer vision.

Factors can include:

- was face exposed,
- observation distance,
- duration,
- obstruction,
- mask state,
- whether witness was directly interacting with the player,
- lighting/visibility approximation,
- witness panic.

A mask primarily prevents **new face evidence**. It does not erase an earlier exposure.

## Clothing identity

An `OutfitSignature` stores stable component/prop values relevant to the current ped model.

Changing clothing:

- can defeat or weaken clothing-only descriptions,
- does not erase face ID,
- does not change a vehicle BOLO,
- does not alter historical evidence.

## Vehicle evidence

A vehicle observation is a snapshot:

```text
model
primary/secondary color
plate + style if seen
confidence
location/time/direction
```

### License plate changes

If a witness reports `ABC123`, the case always says `ABC123` was seen.

Later at a garage:

```text
current owned car plate: ABC123
             ↓ pay service
current owned car plate: XYZ789
```

Future automated/patrol plate comparisons see `XYZ789`. Old evidence does not mutate.

If police had only a plate, the change is a strong countermeasure.

If police had already established the **specific owner/car identity** through face, arrest, vehicle seizure or other stronger evidence, a new plate is not a magical case eraser.

## Vehicle swapping

The system records last-known vehicle continuity only while it is observed.

- Swap vehicles while unseen: can break vehicle search continuity.
- Swap in full view of a witness/officer/camera: the new vehicle can become linked.
- Leave an owned car at the scene: creates powerful evidence/impound risk.

## Repainting

A repaint changes the live/persisted vehicle colors. The old case retains the old reported color.

Police may still match model + other evidence even when color differs, but confidence is reduced unless the vehicle is otherwise known.

## CCTV

CCTV is project logic tied to zones/props.

It can record:

- face when visible/unmasked,
- clothing,
- vehicle model/color,
- plate when criteria are met,
- time and direction.

Rules:

- disable before recording → no capture after disable,
- destroy after recording → stored capture can remain,
- camera coverage is not omniscient and must be spatially defined,
- stronger camera/security appears at businesses that upgrade after repeated robberies.

## Dispatch severity

Suggested progression:

### Low severity

- nonviolent theft / minor threat,
- few patrol units,
- standard investigation.

### Armed robbery

- multiple patrol units,
- perimeter/search behavior,
- stronger vehicle/person descriptions.

### Shots fired / injured civilians

- faster escalation,
- more armed officers,
- broader search.

### Homicide / officer assault

- major escalation,
- stronger/longer warrants,
- tactical response where appropriate.

### Mass violence

- NOOSE/FIB-style escalation where GTA assets/AI support it.

## Investigation scene

If the player escapes before police arrive, officers still go to the **crime location**.

Near the player, the scene can physically include:

- patrol vehicles,
- officers entering/checking the business,
- officer guarding entrance,
- surviving clerk/customer interview,
- body check/medical approximation,
- abandoned getaway vehicle inspection,
- temporary scene control.

Far away, the same event is an abstract timed record to avoid wasting CPU/entities.

## Interviews

Interviews are not hard-coded cinematics. They are a semantic interaction.

Example case data:

```text
Witness A:
face = false
mask = true
clothing = black jacket
vehicle = unknown

Witness B:
face = false
vehicle = black Sentinel
plate = unknown
```

The presentation layer can choose:

```text
Officer subtitle: "Did you get a look at the suspect?"
Witness response: "No, there was a mask. Black jacket."

Officer subtitle: "Anything about the vehicle?"
Witness response: "Black coupe. I didn't catch the plate."
```

Those answers must come from the record, not random flavor that contradicts the case.

## Dialogue fallback hierarchy

1. Existing GTA V ambient speech that semantically fits.
2. Project-owned/licensed original voice line.
3. Subtitle + generic vocal reaction/gesture.

This keeps the system implementable even if GTA does not contain the exact sentence requested.

## Search behavior

Search is centered on **last reliable observation**.

Stored fields:

```text
lastKnownPosition
lastKnownHeading
lastKnownVehicleDescription
lastSeenTime
searchRadius
```

Police do not receive continuous player coordinates while out of observation.

The search planner can:

- patrol likely nearby streets,
- bias toward known heading,
- check likely exits,
- expand radius with time,
- terminate immediate search while leaving the case/warrant active.

## Police recognition after the chase

When an officer is near the player outside a current chase, `RecognitionService` compares current observable state to active cases.

Examples:

- confirmed face warrant + unmasked face visible → strong recognition,
- clothing-only description after outfit change → weak/no match,
- plate BOLO + exact plate visible → strong vehicle match,
- old black-Sentinel description but same model repainted white → weaker match,
- unknown suspect with no plate/face → no magical recognition.

## Surrender/arrest

When officers have tactical control and a surrender opportunity is reasonable:

- show surrender input,
- disable attack temporarily after acceptance,
- play hands-up/kneel/arrest-compatible GTA tasks,
- resolve immediate wanted state through controlled transition,
- confiscate/seize appropriate unsecured loot,
- impound involved vehicle if applicable,
- apply fines/legal costs,
- update relevant cases.

Arrest does not necessarily wipe unrelated warrants/cases.

## Clerk memory

Logical clerk memory stores observations from prior crimes.

On future visits the clerk may:

- not recognize player,
- become uneasy,
- refuse service,
- silently trigger alarm,
- flee to back room,
- openly call police,
- arm themselves if profile/security state supports it.

If the clerk dies, their personal memory is removed. Other witness/business/camera evidence remains.

## Business escalation

Suggested persistent security stages:

```text
0 normal
1 faster alarm / wary clerk
2 improved CCTV
3 guard chance / stronger response
4 lower exposed cash / improved safe behavior
```

The system should eventually cool down so one mistake does not permanently ruin a location.

## Performance rules

- Candidate scan ~4–5 Hz.
- Witness perception staggered ~2–5 Hz depending proximity/event intensity.
- Search planner ~2 Hz.
- Case housekeeping ~1 Hz or slower.
- Event-driven updates for evidence/reporting/identification.
- Limit number of high-detail crime scenes active simultaneously.
- Never use an all-world every-frame ped scan.
