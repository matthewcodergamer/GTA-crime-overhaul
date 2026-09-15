# Stage 4 — Witness perception and reporting validation

Roadmap scope: **77–95 only**.

This stage is deliberately built on the single Stage 3 prototype store. It does not scale robbery locations.

## Performance contract

- Candidate discovery is bounded to `maxCandidates` (default 16).
- Candidate discovery refreshes at low frequency (default every 600 ms), not every frame.
- Only `samplesPerFiveHzTick` candidates (default 3) receive detailed perception sampling on a 5 Hz tick.
- LOS checks occur only after a candidate is inside the configured visual distance and FOV.
- Ambient ped handles and live reaction state are transient and are never serialized.
- The case receives only meaningful reported observation snapshots.

## Knowledge contract

Hearing and sight are separate channels.

A hearing-only witness may know that a threat, gunshot, or violence occurred. Hearing alone must **not** produce face, face-cover, outfit, weapon-class, vehicle, plate, or visual-direction evidence.

Visual details require distance + FOV + LOS + view-quality checks. Confidence additionally depends on accumulated view time and an approximate ambient-light factor. Plate evidence has its own front/rear geometry and minimum-view-time gate.

Stage 4's face-cover check is intentionally a conservative visual proxy. Stage 5 owns the authoritative project mask/identity rules. Stage 4 must never treat the proxy as confirmed player identity.

## Manual in-game checks

Use a Story Mode build with the Stage 3 prototype store target already `VERIFIED_IN_GAME`.

1. Press **F3** to toggle witness spatial/FOV/LOS debug rendering.
2. Start a robbery with two civilians placed differently relative to the player.
3. Confirm candidate count remains bounded and debug cones/lines do not update from an every-ped/every-frame raycast loop.
4. Put Witness A in a clear, front-facing, well-lit view for several seconds. Confirm `witness.inspect` can show visual confidence and eventual face/outfit/weapon facts.
5. Put Witness B behind an obstruction but within hearing range. Confirm B can become aware/report without gaining visual identity facts.
6. Leave in a vehicle. Confirm vehicle model/color is captured only by witnesses that can actually see it.
7. Test the plate from a side-on angle and a brief glimpse: plate evidence must remain absent.
8. Test the plate from a clear front/rear angle for long enough: plate evidence may appear with confidence.
9. Aim at or knock down a witness while they are waiting/reporting. Confirm the report can be interrupted.
10. Interrupt after the partial/basic report point. Confirm already-committed basic evidence remains in the case and another witness's evidence is unaffected.
11. Kill/injure a tracked witness during the incident. Confirm other candidates can receive a separate violence hearing/visual stimulus only when their own geometry/range supports it.
12. Leave the area and wait through the aftermath window. Confirm transient ambient witness candidates are discarded while reported case evidence remains.

## Automated coverage

`GCO_WitnessDomainTests` covers:

- hearing cannot leak visual identity information;
- two witnesses at the same crime can produce materially different reports;
- plate reading requires sustained view time;
- staggered scan budgets never sample the whole candidate list every tick;
- delayed/partial reporting and interruption semantics;
- confidence falls to zero without LOS/outside visual range and scales with view time/light.

## Exit criterion

Stage 4 is code-complete when the native ASI builds, automated tests pass, and the in-game prototype demonstrates two witnesses producing different reports based on actual perception. The prototype store's separate Stage 3 anchor-validation gate remains authoritative; Stage 4 does not bypass it.
