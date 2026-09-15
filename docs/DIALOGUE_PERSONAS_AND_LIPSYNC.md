# Dialogue Personas, Voice Matching and Lip-Sync

## Purpose

This document locks how GTA Crime Overhaul presents clerks, witnesses and police as distinct people without allowing presentation traits to corrupt investigation truth.

The core rule is:

> **Evidence decides what may be said. Persona decides only how it is said and which compatible voice presents it.**

Gender presentation, age band, cadence, voice choice and lip-sync must never create evidence, strengthen case confidence, identify a suspect, create a warrant or erase historical observations.

## Speaker persona contract

Presentation profiles use these dimensions:

- role: clerk, civilian witness, security guard, patrol officer or investigating officer;
- voice gender: masculine or feminine when the physical GTA ped can be classified safely;
- age band: young adult, adult or older adult;
- speech register: grounded contemporary, neutral professional or measured;
- optional personality/temperament supplied by the owning gameplay system later.

### Gender

Gender affects **voice selection**, not stereotyped wording.

For a live GTA ped, the native presentation adapter first verifies that the entity is a human ped and then uses GTA's male/female ped classification to choose a compatible masculine/feminine voice pool. Animals and invalid/non-human peds are rejected from the investigation-dialogue debug demo.

The system does not infer personality, intelligence, aggression or evidence quality from gender.

### Age

Age is **not inferred from GTA appearance or ped model**. GTA does not provide a trustworthy general-purpose age value for ambient peds.

A production logical clerk/witness profile owns its age band. When an ambient debug actor has no logical profile, the safe fallback is `adult` rather than guessing.

Age differences are intentionally restrained:

- **young adult:** slightly quicker cadence and somewhat shorter, contemporary but neutral phrasing;
- **adult:** neutral cadence and plain grounded phrasing;
- **older adult:** slightly more measured cadence and sometimes a more complete sentence.

Age must not become a gimmick. The content policy explicitly rejects forced youth slang, internet slang, fake generational catchphrases, archaic caricature, frailty stereotypes and "back in my day" writing.

Examples of the intended difference:

```text
young adult: "Okay. Tell me what happened."
older adult: "All right. Start from the beginning for me."
```

Both are ordinary believable speech. Neither is written as a stereotype.

## Persistent clerk identity

The physical GTA ped is not the clerk's identity.

Later Stage 3/Stage 9 integration should extend the logical clerk profile with presentation metadata such as:

```text
clerkId
ageBand
voiceGenderPreference / compatible physical gender
voiceStyleId
personality
fear baseline
prior trauma/aggression
memory state
```

If GTA streams the physical clerk out, the business system may reacquire or recreate a compatible physical ped. The replacement ped should match required presentation constraints when practical, but the **logical clerk keeps the same memory, age band, personality and voice persona**.

That is how a recurring clerk can feel like the same person without depending on one fragile GTA entity handle.

## Dialogue data

`data/dialogue/personas.json` owns the persona policy and candidate voice sets.

`data/dialogue/persona_lines.json` contains subtle age-conditioned wording variants. These are normal dialogue entries with the same semantic event/evidence conditions, weights, cooldowns, reuse groups and no-repeat groups as the rest of the dialogue system.

Gender-specific prose is intentionally forbidden. A line can be age-conditioned, but `voiceGenders` must remain `any`; the voice layer handles gender matching.

The build validator rejects:

- missing role/gender/age voice-set coverage;
- a male voice in a feminine pool or vice versa;
- persona data that says demographics can affect evidence/case/recognition/warrants;
- forced slang patterns such as `bruh`, `no cap`, `rizz`, `fr fr`, etc.;
- age caricatures such as `back in my day`, `young whippersnapper`, etc.;
- production-ready claims for generated voices or facial assets that are not yet manually validated.

The banned-phrase check is a regression guard, not the entire writing-quality standard. Human review still rejects dialogue that feels forced even when it does not contain a literal banned phrase.

## Voice strategy

### Current state

Generated voice is **not yet a production dependency**.

Gemini TTS is tracked as a candidate provider because its current public voice catalog offers multiple male/female voices with distinct styles and supports controlled speech generation. Candidate mappings in `personas.json` remain `REFERENCE_ONLY` until they are heard in GTA and approved for:

- gender fit;
- age/style fit;
- clarity;
- lack of exaggerated delivery;
- spatial playback quality;
- latency/caching behavior;
- cleanup when a speaker disappears or a scene aborts.

No API key belongs in the repository, save file or release package.

No generated voice may impersonate a Rockstar actor or named GTA character performer.

### Voice-set selection

The runtime contract chooses a voice set by:

```text
role + physical/logical voice gender + logical age band
```

Example IDs:

```text
clerk.feminine.young_adult
clerk.masculine.older_adult
investigating_officer.feminine.adult
patrol_officer.masculine.adult
```

Each set contains more than one candidate voice so every female clerk or every older officer does not sound identical.

## Lip-sync architecture

Lip-sync must follow the **actual audio waveform**, not the subtitle timer.

The project now has a provider-neutral `LipSyncEnvelope` that accepts PCM16 audio and produces timestamped mouth-openness samples. It uses RMS energy with fast attack and slower release so short consonant gaps do not cause frantic open/close flicker.

The intended production path is:

```text
approved semantic line
        |
        v
validated voice provider / authorized recording
        |
        v
actual PCM audio + exact duration
        |
        +----> spatial audio playback from speaker NPC
        |
        v
LipSyncEnvelope
        |
        v
validated GTA facial presenter
        |
        v
speaker mouth activity while audio is active
```

When playback ends or aborts, mouth activity must return to neutral.

### GTA facial candidates

Public GTA/FiveM references commonly use:

- talk candidate: `mp_facial / mic_chatter`;
- masculine neutral candidate: `facials@gen_male@variations@normal / mood_normal_2`;
- feminine neutral candidate: `facials@gen_female@variations@normal / mood_normal_2`.

These identifiers are recorded as **`REFERENCE_ONLY`**. They are not `VERIFIED_IN_GAME` for Crime Overhaul yet.

The native `FacialAnimationAdapter` isolates `PLAY_FACIAL_ANIM` plus bounded animation-dictionary loading. Gameplay/domain code never calls the facial native directly and never hardcodes the candidate animation names.

A future presenter may choose a more accurate viseme-capable path if validated. The domain `LipSyncEnvelope` does not care which GTA facial mechanism eventually renders it.

## F7 investigation demo

The existing F7 overhearing prototype now has a stricter presentation path:

1. only live human non-player peds are eligible as temporary actors;
2. each actor's physical male/female classification selects a compatible voice-gender pool;
3. ambient debug actors use `adult` because there is no logical age profile to trust;
4. the log records the chosen role/gender/age voice-set ID for every conversation turn;
5. no TTS audio or reference-only facial animation is automatically played yet;
6. subtitles remain the authoritative working fallback.

This gives us a safe in-game way to confirm actor/persona matching before adding the optional voice bridge.

## Required manual validation before voice/lip-sync promotion

Run separately on GTA V Legacy and Enhanced.

### Voice

For several male and female clerks/witnesses/officers across each age band:

1. generate the exact same evidence-safe line with candidate voices;
2. verify the chosen voice sounds compatible with the physical speaker;
3. reject voices that sound exaggerated, comedic or obviously wrong for the profile;
4. verify younger/older variation is subtle rather than stereotyped;
5. verify spatial volume/position follows the NPC;
6. walk out of hearing range and confirm playback attenuates/stops presenting information correctly;
7. delete/stream out/kill the speaker during playback and verify cleanup.

### Lip-sync

1. validate the candidate talk facial dictionary/clip on representative male and female human peds;
2. verify the mouth visibly moves while voiced PCM is active;
3. insert real pauses into audio and verify the mouth relaxes during pauses;
4. verify the mouth returns to a neutral face when audio ends;
5. abort audio mid-line and verify the face recovers immediately;
6. test ragdoll/death/stream-out/mission transition cleanup;
7. verify the result on both Legacy and Enhanced before changing status to `VERIFIED_IN_GAME`.

## Failure behavior

Presentation may fail; investigation truth may not.

If generated voice is unavailable, incompatible, too slow or invalid:

- keep the approved subtitle;
- optionally use a separately validated GTA ambient bark only when its meaning fits;
- do not block the interview;
- do not mutate evidence;
- do not leave a facial animation stuck.

If a physical clerk must be replaced after streaming, the logical profile persists and the new physical actor inherits the same role/age/personality/voice-style constraints rather than becoming a brand-new person.
