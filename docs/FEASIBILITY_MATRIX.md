# Feasibility Matrix — GTA V / Script Hook V Constraints

This document prevents the design from depending on mechanics the GTA V Story Mode scripting environment cannot reliably deliver.

## Technical baseline

- Native C++ x64 ASI loaded by the Script Hook V ASI loader.
- Official Script Hook V runtime/SDK from Alexander Blade.
- Story Mode only.
- Legacy + Enhanced compatibility target.
- We prefer game natives, script tasks, scenarios, props, animation dictionaries, decorators/local persistence and project-owned data rather than memory patches.
- Memory offsets are **not** part of the baseline because they are patch-fragile and can diverge between Legacy and Enhanced.

## Feasibility legend

- **GREEN** — practical with Script Hook V/native scripting.
- **YELLOW** — practical but needs approximation, asset work, careful state recovery or per-build validation.
- **RED** — not a baseline promise; would require invasive engine work, copyrighted asset extraction or unreasonable fragility.

| Feature | Rating | Implementation reality |
|---|---:|---|
| Native `.asi` plugin | GREEN | Standard Script Hook V use case. |
| Store robbery state machine | GREEN | Scripted ped tasks, entity checks, timers and persistent data. |
| Witness LOS | GREEN | Spatial candidate filtering + LOS/raycast/native visibility checks. |
| Witness memory | GREEN | Project-owned records; do not persist GTA handles. |
| Unknown suspect state | GREEN | Entirely project-owned law state. |
| Face/mask/clothing evidence | GREEN/YELLOW | Can classify current ped components/props and visibility; exact facial recognition is simulated, not GTA computer vision. |
| Vehicle model/color/plate evidence | GREEN | Native vehicle model/color/plate access plus persistence. |
| Garage plate changes | GREEN | Update live plate using vehicle natives and save the new string/style in the owned vehicle record. |
| Repaint as evidence countermeasure | GREEN | Vehicle color natives + persistent evidence snapshot. |
| Search area / last-known position | GREEN | Project AI/state with patrol spawn/task logic. |
| Persistent warrants | GREEN | JSON/state database + recognition checks. |
| Police interview gameplay | GREEN | Officers/witnesses can be assigned positioning/facing/tasks; answers update case data. |
| Perfect bespoke interview animation | YELLOW | Use available scenarios/gestures/idle anims; custom mocap would be separate asset work. |
| Existing ambient GTA dialogue | GREEN/YELLOW | Available lines can be triggered where suitable; exact requested sentences are not guaranteed to exist. |
| Unlimited new fully voiced dialogue | YELLOW | Requires original/legally licensed recordings/TTS; subtitle fallback is baseline. |
| Clone Rockstar actor voices | RED | Not a project dependency. |
| Handkerchief/bandana mechanic | GREEN/YELLOW | Identity mechanic is easy; visual item uses an existing GTA component/prop or a custom GTA-compatible asset. |
| Port RDR2 bandana model/animation directly | RED | Do not redistribute extracted RDR2 assets. |
| Duffel prop attached to player | GREEN | Attach an existing/custom bag prop/component to ped bones. |
| GTA VI-level dynamic bag cloth/fill deformation | RED/YELLOW | Capacity and visual tiers are feasible; exact modern cloth/fill simulation is not a baseline promise. |
| Duffel capacity affects max take | GREEN | Pure game logic. |
| Longer collection time for more loot | GREEN | Pickup/interaction loop + animation timing. |
| Drop/recover duffel | GREEN | Spawn/attach/detach a prop and persist abstract loot state while streamed appropriately. |
| Police seize dropped loot | GREEN | Project state transition when police secure the scene. |
| Clerk persistent recognition | GREEN | Persist identity observations by business/clerk profile. |
| Same exact ambient clerk survives forever | YELLOW | GTA streams/despawns peds; persist a logical clerk identity/appearance profile and respawn/reacquire rather than rely on one handle. |
| Business upgrades after repeated robbery | GREEN | Persistent state controls spawned guards/cameras/cash/alarm timing. |
| CCTV evidence | GREEN/YELLOW | Project-controlled camera zones/props + visibility checks; not real video analysis. |
| Destroy camera before/after capture distinction | GREEN | Project camera state/timestamp logic. |
| Crime scenes after escape | GREEN/YELLOW | Spawn scene actors/vehicles when near; abstract the scene when far away. |
| Bodies persist indefinitely | RED/YELLOW | GTA streaming/cleanup limits apply; persist incident records, not guaranteed physical bodies. |
| Medical/forensic response | YELLOW | Approximate with peds, vehicles, scenarios and animations. |
| Vehicle trunk inventory | GREEN | Persistent inventory + trunk bone/door native + interaction animation. |
| Physical weapon props visible in trunk | YELLOW | Optional later prop placement; inventory logic does not depend on it. |
| Limited carried weapons | GREEN | Control allowed weapon slots and move extras into storage. |
| Persistent owned vehicles | GREEN/YELLOW | Save model/mod/plate/location/state and reconstruct; cannot persist raw GTA entity handles across sessions. |
| Impound storage persistence | GREEN | Project record. |
| DLC/Online cars in Story Mode | GREEN/YELLOW | Can use models present in the player's installed build; availability varies by build/DLC and must be checked before spawn/purchase. |
| Custom web catalog/showroom | GREEN | Scaleform/native UI or custom HUD/menu logic. |
| Hot/marked money | GREEN | Project economy state. |
| Fleeca robbery | GREEN/YELLOW | Existing interiors/doors plus scripted peds/loot; exact interior availability must be validated per build. |
| Pacific Standard robbery | GREEN/YELLOW | Same principle, larger orchestration. |
| Union Depository-scale free-form job | YELLOW | Technically possible as a large scripted/systemic encounter, but content complexity is high and comes late. |
| Completely seamless every bank interior at all times | YELLOW | Interior/IPL behavior varies; use validated activation/interior adapters. |
| Dynamic police radio matching every event | YELLOW | Mix existing audio/subtitles/original audio; not every phrase exists in base game. |
| Surrender/arrest | GREEN | Player control + tasks + transition + inventory/case consequences. |
| Police remember player across game restarts | GREEN | Persistence. |
| NPCs globally remember every crime forever | YELLOW | Only persist bounded important observations/cases to keep saves/performance sane. |
| Full ballistic forensics/fingerprints | YELLOW | Can be simulated at design level; GTA does not expose a full forensic engine. |
| Real-world ANPR computer vision | YELLOW | Simulate plate capture when camera geometry/visibility criteria are met. |
| Mission-safe operation | GREEN/YELLOW | Requires strong mission/cutscene detection and suspension rules. |
| Run in GTA Online | RED | Explicitly out of scope. |

## Animation strategy

The rule is **reuse, orchestrate, recover**.

1. Request an animation dictionary only when needed.
2. Wait with a timeout; never block the script forever.
3. Use appropriate GTA V tasks/scenarios first.
4. Attach props to validated ped bones where needed.
5. Detect task interruption, ragdoll, death, combat, mission takeover or entity deletion.
6. On failure, release prop/task state and continue the gameplay state machine with a fallback.
7. Never require a cinematic-perfect animation for simulation correctness.

Likely reusable categories include:

- hands-up / surrender
- cower / fear
- phone use
- register/counter interaction approximations
- keypad/safe/door interactions
- pickup/grab/carry gestures
- kneeling / getting on floor approximations
- trunk open/close + reach interaction
- bag carry/idle/run variants when compatible
- officer note-taking/talking/idle/scene-control gestures

Exact dictionary/clip names are validated during implementation using GTA V animation browsing tools and in-game debug commands; names are not hard-locked in design documents because some clips behave differently across ped types and contexts.

## Dialogue strategy

Gameplay must never depend on finding a perfect Rockstar voice line.

`DialogueDirector` receives a semantic event such as:

```text
police_interview.face_not_seen
clerk.repeat_robber_recognized
robbery.clerk_compliant
witness.reporting_vehicle_only
```

It then selects, in order:

1. a validated existing GTA V speech line if one fits,
2. project-owned original/authorized audio,
3. subtitle + generic vocal reaction/gesture fallback.

This guarantees every state can be represented even when base-game dialogue is limited.

## Plate-changing reality

The live GTA vehicle plate can be changed through vehicle natives, but project persistence is authoritative. A case stores an immutable observation snapshot:

```json
{
  "observedPlate": "46EEK572",
  "confidence": 0.94,
  "observedAt": 123456789,
  "vehicleFingerprint": {
    "modelHash": 0,
    "primaryColor": 0,
    "secondaryColor": 0
  }
}
```

If the player later buys a new plate such as `L8R BOZO`, old case evidence remains `46EEK572`. Police may stop matching the new plate directly, but a case that already established the vehicle owner's identity can still remain active.

## Duffel reality

The **capacity mechanic is guaranteed** because it is our own state. Visual fidelity scales by implementation stage:

- Stage A: logical bag tier + HUD capacity.
- Stage B: attach GTA-compatible duffel visual.
- Stage C: different visual bag models/variants for tiers/fullness where assets allow.
- Stage D: optional custom animation/prop polish.

We do not block robbery gameplay on Stage C/D.

## Why native C++

The mod can be prototyped in ScriptHookVDotNet, but the repository's release target is C++ ASI because it gives:

- direct Script Hook V integration,
- one native binary runtime,
- fewer managed-runtime dependencies,
- easier low-level performance control for witness/police scheduling,
- a clean release artifact (`GTA_Crime_Overhaul.asi`).

## External dependencies policy

Required at runtime:

- GTA V Story Mode
- current compatible Script Hook V runtime
- ASI loader supplied with Script Hook V (or compatible loader)
- `GTA_Crime_Overhaul.asi`
- project data/config files

Not redistributed by this repository:

- `ScriptHookV.dll`
- `dinput8.dll`
- Rockstar proprietary game assets
- extracted RDR2 assets
