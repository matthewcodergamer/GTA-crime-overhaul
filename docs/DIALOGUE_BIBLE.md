# GTA Crime Overhaul — Dialogue Bible

The goal is reactive dialogue that supports the simulation without becoming repetitive, annoying, or obviously generated. Dialogue is data, not hard-coded strings scattered through C++.

## 1. Core rule

A semantic event happens first. Dialogue is selected afterward.

Example:

```text
Witness observation:
  faceSeen=false
  maskSeen=true
  vehicleSeen=true
  plateSeen=false
  vehicleColor=black

Interview semantics:
  FACE_NOT_SEEN
  MASK_SEEN
  VEHICLE_SEEN
  PLATE_NOT_SEEN
```

The DialogueDirector then chooses presentation appropriate to speaker, personality, urgency, prior history and recent lines.

This prevents one sentence from becoming gameplay logic.

## 2. Speaker groups

Maintain separate pools for:
- `PLAYER_ROBBER`
- `CLERK`
- `CIVILIAN_WITNESS`
- `HOSTAGE`
- `SECURITY_GUARD`
- `PATROL_OFFICER`
- `INVESTIGATING_OFFICER`
- `TACTICAL_OFFICER`
- `POLICE_RADIO`

Within groups, support archetypes/personality profiles rather than one universal voice.

## 3. Dialogue event taxonomy

### Robber demands
- `ROBBERY_BEGIN_LOW`
- `ROBBERY_BEGIN_HIGH`
- `OPEN_REGISTER`
- `OPEN_SECOND_REGISTER`
- `OPEN_SAFE`
- `HANDS_UP`
- `GET_DOWN`
- `MOVE_AWAY_FROM_ALARM`
- `DO_NOT_MOVE`
- `HURRY_UP`
- `BACK_OFF`
- `LEAVE_NOW`

### Clerk reactions
- `CLERK_SHOCKED`
- `CLERK_COMPLY_FAST`
- `CLERK_COMPLY_SLOW`
- `CLERK_PANIC`
- `CLERK_DEFY`
- `CLERK_PLEAD`
- `CLERK_LIE_SAFE_EMPTY`
- `CLERK_PARTIAL_CASH`
- `CLERK_SECRET_ALARM`
- `CLERK_REACH_WEAPON`
- `CLERK_REPEAT_ROBBER_RECOGNIZED`
- `CLERK_REPEAT_ROBBER_MASKED_SUSPECTED`
- `CLERK_POST_ROBBERY_RELIEF`

### Civilian/witness reactions
- `WITNESS_SHOCKED`
- `WITNESS_PANIC`
- `WITNESS_FLEE`
- `WITNESS_CALLING_POLICE`
- `WITNESS_REPORT_INTERRUPTED`
- `WITNESS_HIDE`
- `WITNESS_DEFY`
- `WITNESS_SEES_FACE`
- `WITNESS_SEES_MASK`
- `WITNESS_SEES_VEHICLE`

### Police scene/interview
- `OFFICER_SCENE_ARRIVAL`
- `OFFICER_SECURE_SCENE`
- `OFFICER_CHECK_BODY`
- `OFFICER_INTERVIEW_START`
- `OFFICER_ASK_FACE`
- `OFFICER_ASK_MASK`
- `OFFICER_ASK_CLOTHING`
- `OFFICER_ASK_VEHICLE`
- `OFFICER_ASK_PLATE`
- `OFFICER_ASK_DIRECTION`
- `WITNESS_FACE_YES`
- `WITNESS_FACE_NO`
- `WITNESS_FACE_UNCERTAIN`
- `WITNESS_MASK_YES`
- `WITNESS_VEHICLE_DESCRIPTION`
- `WITNESS_PLATE_FULL`
- `WITNESS_PLATE_PARTIAL`
- `WITNESS_PLATE_NONE`
- `WITNESS_DIRECTION`
- `OFFICER_INTERVIEW_END`

### Search/pursuit
- `RADIO_ROBBERY_REPORTED`
- `RADIO_SUSPECT_UNKNOWN`
- `RADIO_PERSON_DESCRIPTION`
- `RADIO_VEHICLE_DESCRIPTION`
- `RADIO_PLATE_DESCRIPTION`
- `RADIO_LAST_KNOWN_DIRECTION`
- `RADIO_SUSPECT_LOST`
- `OFFICER_SURRENDER_ORDER`
- `OFFICER_ARREST_ORDER`
- `OFFICER_COMBAT_ESCALATION`

### Business memory
- `CLERK_RETURN_NORMAL`
- `CLERK_RETURN_NERVOUS`
- `CLERK_RETURN_RECOGNITION_LOW`
- `CLERK_RETURN_RECOGNITION_HIGH`
- `CLERK_SILENT_ALARM`
- `CLERK_REFUSE_SERVICE`

## 4. Anti-repetition system

Each line entry includes:

```json
{
  "id": "clerk_comply_fast_01",
  "event": "CLERK_COMPLY_FAST",
  "text": "Okay, okay. I'm opening it.",
  "speakerArchetypes": ["clerk_any"],
  "intensity": 2,
  "weight": 1.0,
  "cooldownSeconds": 1800,
  "sessionMax": 1,
  "globalReuseGroup": "clerk_compliance_opening",
  "requires": [],
  "forbids": [],
  "ambientSpeech": null,
  "voiceAsset": null
}
```

Selection rules:

1. Filter by semantic event.
2. Filter by speaker/archetype.
3. Filter by context requirements.
4. Remove lines used in the current encounter when alternatives exist.
5. Remove lines on per-line cooldown.
6. Penalize anything from a recently used reuse group.
7. Penalize lines heard several times in the current save.
8. Prefer lines whose intensity matches fear/escalation.
9. Weighted-random select from the remaining set.
10. If no line survives, use a generic short bark or subtitle fallback rather than forcing the same distinctive sentence again.

## 5. Memory-aware variation

Context tags can include:

```text
first_robbery
repeat_robbery
face_seen
face_not_seen
masked
unmasked
plate_seen
plate_not_seen
clerk_was_previous_victim
clerk_high_fear
clerk_angry
clerk_armed
shots_fired
civilian_injured
clerk_injured
officer_killed
clean_escape
violent_escape
store_security_high
```

A returning clerk should not use first-robbery dialogue. A terrified clerk and an angry armed clerk should not sound identical.

## 6. Minimum line-pool targets

Before an event is considered content-complete:

- very common short bark: 12–20 variants per broad archetype;
- common robbery interaction: 8–12 variants;
- uncommon branch: 4–8 variants;
- highly specific rare event: 2–4 variants plus a generic fallback;
- distinctive repeat-robber recognition line: at least 8 variants across fear/anger profiles;
- officer interview questions: 5–8 variants per question class;
- witness answers: data-composed or at least 8 structural variants per evidence class;
- radio BOLO phrasing: 6–10 templates per report category.

Do not create 300 lines just to inflate a number. Variation must be structurally meaningful.

## 7. Data-composed witness descriptions

Precise evidence should not require a unique recorded line for every combination.

Use templates such as:

```text
"I couldn't see {pronoun} face. {maskSentence}"
"{vehicleColor} {vehicleClass}. {plateSentence}"
"They went {direction} toward {zoneOrStreetFallback}."
```

Possible outputs:
- "I couldn't see his face. He had a mask on."
- "Black SUV. I didn't get the plate."
- "Dark coupe. I caught the first three characters: 46E."

For these exact-information lines, subtitles are authoritative. A generic frightened/response bark may play underneath if a validated GTA speech line fits the speaker.

## 8. Police interview structure

Do not make every investigation play the same six-question checklist.

The InvestigationDirector picks only questions that can resolve uncertainty in the case.

Example A — face already confidently captured by CCTV:
- skip face question;
- ask vehicle/plate/direction if those are weak.

Example B — no vehicle observed:
- ask face/clothing/mask;
- skip plate question.

Example C — witness panicked and low confidence:
- officer can ask one or two short questions and end.

Dialogue sequence length target: normally 2–5 exchanges, not a miniature cutscene every time.

## 9. GTA ambient speech integration

Use base-game ambient speech for short natural barks when compatible with the ped's voice. Candidate categories to research/validate include:
- `GENERIC_FRIGHTENED_HIGH`
- `GENERIC_FRIGHTENED_MED`
- `GENERIC_SHOCKED_HIGH`
- `GENERIC_SHOCKED_MED`
- `GENERIC_CURSE_HIGH`
- `GENERIC_CURSE_MED`
- `GENERIC_INSULT_HIGH`
- `GENERIC_HI`
- `CHAT_STATE`
- `CHAT_RESP`
- `STAY_DOWN`
- `FALL_BACK`
- threat-related ambient names where a police/civilian voice actually supports them.

Never assume a speech name works for every voice. The validation table must store compatible voice/model families.

## 10. Original voice packs

Optional voice packs can make longer interactions feel less subtitle-driven.

Allowed:
- voices recorded by the project/user/contributors with permission;
- licensed voice talent;
- TTS voices whose license explicitly permits distribution/use, provided they are not impersonating a real actor without permission.

Do not clone Franklin/Michael/Trevor/Lester or other Rockstar performers.

Voice file metadata should include:
- line ID;
- performer/license;
- speaker archetype;
- emotional intensity;
- duration;
- normalized loudness target;
- subtitle text;
- optional lip-sync/gesture tag if supported later.

## 11. How the user can help create dialogue

Create line packs by category instead of writing one giant script.

Recommended submission format:

```text
CATEGORY: CLERK_COMPLY_FAST
PERSONALITY: fearful
INTENSITY: 2/5
CONTEXT: first or repeat robbery

1. Okay, okay. I'm opening it.
2. Don't shoot. I'll give you the cash.
3. Just give me a second. Please.
...
```

Avoid:
- repeating the same opening word across every line;
- every character swearing constantly;
- references that only make sense at one store unless tagged for that location;
- movie-style monologues during an armed robbery;
- lines that reveal information the speaker could not know.

Prefer:
- fragments and interruptions under stress;
- different rhythms and sentence lengths;
- personality-specific behavior;
- occasional silence instead of mandatory chatter.

## 12. Prompt: generate a clerk line pack

Use this prompt with a writing model:

```text
You are writing grounded ambient dialogue for a GTA V Story Mode crime-overhaul mod.

Create 30 ORIGINAL short clerk lines for event: {EVENT}.
Personality: {PERSONALITY}.
Emotional intensity: {INTENSITY}/5.
Context tags: {CONTEXT_TAGS}.

Rules:
- Do not imitate or quote Rockstar dialogue.
- Do not imitate named GTA actors/characters.
- Most lines should be 2–12 words; a few may be longer.
- Vary syntax, opening words, rhythm and emotional reaction.
- No two lines should communicate the exact same idea with only a synonym swapped.
- The clerk may know only facts included in the context tags.
- Avoid constant profanity; use it selectively.
- Avoid action directions inside the spoken line.
- Return JSON objects with: id, text, intensity, tags, weight=1.0, cooldownSeconds, sessionMax=1.
- Add a final duplicate/near-duplicate audit and rewrite any weak pair.
```

## 13. Prompt: generate witness interview answers

```text
Write ORIGINAL short witness answers for a systemic GTA V police investigation.

Evidence facts:
{EVIDENCE_FACTS}
Witness confidence: {CONFIDENCE}/100
Witness fear: {FEAR}/100
Witness archetype: {ARCHETYPE}
Question semantic event: {QUESTION_EVENT}

Create 12 variants.
Rules:
- Never add facts not present in EVIDENCE_FACTS.
- Low-confidence witnesses should hedge naturally.
- High-confidence witnesses may be concise and definite.
- Keep most lines under 14 words.
- Do not use detective exposition or legal jargon ordinary civilians would not say.
- Avoid repeating sentence structures.
- Return JSON-ready entries with tags and confidence range.
```

## 14. Prompt: generate police question variants

```text
Create 10 ORIGINAL GTA-compatible police interview question variants for semantic event {EVENT}.
Tone: professional street-level patrol officer, brief and natural.

Rules:
- Do not quote GTA V/RDR2/Mafia dialogue.
- 3–10 words preferred.
- No comedy unless context explicitly requests it.
- No more than two variants may begin with the same first word.
- Questions must ask only about {INFORMATION_TARGET}.
- Return JSON-ready entries.
```

## 15. Prompt: anti-repetition audit

```text
Audit this dialogue pool for repetition.

Find:
1. exact duplicates;
2. near-duplicates that differ only by synonyms;
3. repeated openings;
4. repeated punchlines/ideas;
5. lines too long for ambient gameplay;
6. lines that assume facts the NPC cannot know;
7. lines whose personality/intensity tags do not match the wording.

For every problem, identify the line IDs and rewrite only the weaker line. Preserve the event's gameplay meaning. Return the corrected full pool plus a compact audit summary.
```

## 16. Runtime repetition telemetry

Debug builds should track:
- selected line ID;
- times heard this save;
- last-used game time;
- last-used real timestamp optionally for tests;
- speaker;
- event;
- rejected candidates and rejection reason.

A debug command should print the last 50 dialogue selections so we can find repetitive patterns during actual play.

## 17. Silence is content

Not every event needs speech. Add explicit `SILENT_REACTION` weight to appropriate pools. A clerk staring, backing away and hitting a silent alarm can be more effective than hearing another recognition line.
