# GTA Crime Overhaul — Dialogue Authoring Prompts

Use with `docs/DIALOGUE_BIBLE.md`. These prompts are for original text/subtitles and optional authorized voice packs. They are not prompts to imitate or clone GTA actors.

## 1. Clerk pack

Write an original GTA-style-but-not-copying dialogue pack for a convenience-store clerk during armed robbery. Generate at least 80 short barks split across: initial shock, comply, panic, stall, secret alarm, register opening, second-register denial, safe denial, safe admission, robber too close, robber looking away, customer endangered, clerk armed/defiant, clerk surrender after resistance, player leaving, police arrival aftermath, later recognition of same robber, and post-robbery nervous ambient lines. Tag each line with personality (`cowardly`, `compliant`, `defiant`, `panicky`, `experienced`, `reckless`), fear level 0–3, violence context, repeat group and minimum cooldown. Keep most lines under 2.5 seconds spoken. Avoid catchphrases. Common events need many interchangeable lines and some events should permit silence.

## 2. Robber demand pack

Write short generic player-demand subtitle lines that can plausibly fit Franklin/Michael/Trevor without impersonating their copyrighted voices. Categories: open register, second register, safe, hands up, get down, move from alarm, don't move, hurry, crowd control, guard warning, bag-money demand, bank-teller demand and retreat warning. Provide 12–20 variants per common category, with `calm`, `urgent`, `aggressive` intensity tags. No character-specific lore. These may be subtitle-only unless original voice acting is later supplied.

## 3. Civilian witness pack

Write 100 compact witness/civilian reactions covering: gun spotted, robbery realized, hiding, fleeing, calling 911, call interrupted, saw face, did not see face, saw mask, saw clothing, saw vehicle only, saw plate, unsure plate, direction seen, clerk shot, civilian shot, robber leaves, police questioning and returning to scene later. Lines must not reveal facts the witness did not observe. Include uncertainty language at low confidence. Tag by fear, confidence, distance/context and whether safe for ambient speech reuse.

## 4. Police interview pack

Write modular officer/witness interview dialogue for a crime scene. Officer questions should be generic and reusable: what happened, face, mask, clothing, weapon, vehicle, plate, direction, number of suspects, injuries. Witness answers must come in evidence-aware pools: `known`, `unknown`, `uncertain`, `contradictory/low confidence`. Create enough variants that two interviews do not sound identical. Keep questions/answers short so they work as ambient in-world exchanges rather than cutscenes.

## 5. Police scene pack

Write short original subtitle/radio-compatible police barks for arriving officers, perimeter control, checking a store, finding a body, finding an abandoned getaway car, requesting EMS, requesting additional units, beginning a search, losing visual, matching a vehicle BOLO, matching a plate, suspect surrender, arrest and scene clear. Do not use real-world police codes unless meaning is certain and useful. Avoid constantly announcing internal game-system state to the player.

## 6. Guard/hostage bank pack

Write separate dialogue pools for bank guards, tellers, customers/hostages and robber crowd-control demands. Cover quiet tension, panic, compliance, guard challenge, guard surrender, guard attack, teller vault/safe interaction, customers pleading, customers whispering, attempted escape and police arrival. Give each archetype 50+ lines across states. Avoid melodrama every few seconds; silence, crying/effort audio and animation should carry some moments.

## 7. Repeat-robber memory pack

Write clerk/business-memory lines for a player returning after a prior robbery. Split by: strong face recognition, uncertain recognition, recognized vehicle only, recognized clothing pattern only, prior violence high/low, prior clerk injured, friend/coworker replaced prior clerk, and business security upgraded. Include subtle pre-alarm lines (staring, nervous greeting, excuse to step away) rather than instantly yelling every time. These lines should make persistent memory noticeable without exposing hidden confidence numbers.

## 8. Anti-repetition expansion prompt

Given an existing dialogue JSON file, audit it for categories with fewer than 8 viable variants, repeated sentence openings, repeated profanity, duplicated semantic meaning and lines likely to fire frequently. Expand weak pools while preserving tags/schema. Add cooldown groups and `maxPerSession` values. For high-frequency events, target 15–30 variants plus a configurable silence chance. Do not pad the pool with near-identical paraphrases.

## 9. Dialogue editor prompt for the user

I am contributing lines to GTA Crime Overhaul. Take my rough dialogue below and convert it into the repository dialogue schema without changing the intended attitude. For each line assign: semantic event, speaker archetype/personality, emotional intensity, prerequisites, forbidden contexts, cooldown group, reuse group, weight and optional notes. Flag any line that assumes information the NPC could not know. Then create 3–5 genuinely different alternatives for each common/high-frequency line so it will not become repetitive.

Paste my lines below:

`[USER DIALOGUE HERE]`

## 10. Dialogue QA prompt

Audit `data/dialogue/*.json` and `docs/DIALOGUE_BIBLE.md`. Simulate 20 store robberies, 10 repeat visits and 10 police investigations at the selector level. Report repeated lines, repeated openings, events with no valid line, lines whose prerequisites contradict case evidence, cooldown starvation, personality mismatches and events that speak too frequently. Recommend exact pool-size/cooldown/silence changes. The desired result is that dialogue supports gameplay without sounding like a mod spamming subtitles every few seconds.