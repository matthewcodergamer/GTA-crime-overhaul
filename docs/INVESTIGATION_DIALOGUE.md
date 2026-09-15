# Investigation Dialogue and Overhearing

## Goal

Police interviews should be something the player can physically encounter in the world, not a hidden case-calculation step.

After a robbery, an officer may stand with the surviving clerk, customer, guard or other witness and ask what happened. The exchange is generated from what that witness actually knows. If police do not know who the robber is, the player can return to the area, walk near the conversation and overhear what the witness is telling them.

This creates information gameplay:

- the player may learn that nobody saw the face;
- the player may hear that a mask was reported;
- the player may learn that clothing was remembered;
- the player may discover that the getaway vehicle was seen;
- the player may discover that a full or partial plate was captured;
- the player may hear which direction police believe the robber went;
- the player can make later countermeasure decisions from information learned naturally in the world.

The conversation itself never identifies the player. Recognition/arrest remains the responsibility of the case/recognition systems.

## Reference study: Los Santos Alive

Los Santos Alive is useful as a presentation/architecture reference, not as code or content to copy.

Public project information describes:

- live AI-generated NPC dialogue rather than fixed dialogue trees;
- separate conversations for multiple NPCs;
- contextual awareness of world state and nearby people;
- NPC-to-NPC interaction;
- spatialized dialogue whose apparent source is the speaking NPC;
- dynamic lip-sync;
- a stack that publicly credits Google Gemini API, Node.js, WebSocketSharp and NAudio;
- a SHVDN3 variant that uses a local `LosSantosAliveServer` plus NPC prompt/data files.

Reference pages:

- https://www.gta5-mods.com/scripts/los-santos-alive-next-generation-ai-npcs
- https://www.gta5-mods.com/scripts/lossantosalive-shvdotnet3-transcription
- https://ai.google.dev/gemini-api/docs/speech-generation
- https://ai.google.dev/gemini-api/docs/live-api/capabilities

We are deliberately implementing a narrower and more deterministic system. Crime Overhaul does not need every pedestrian to be an AI character. It needs believable police/witness conversations whose factual content is legally important to gameplay.

## Core architecture

```text
WitnessObservation / Case facts
              |
              v
InvestigationDialogueComposer
  - chooses useful topics
  - emits question/answer turns
  - attaches explicit EvidenceClaim bits
  - never mutates the case
              |
              v
Conversation presentation
  - officer/witness speaker entity
  - timing
  - subtitles
  - optional voice
  - spatial attenuation/occlusion
              |
              v
Player hearing the scene
```

The critical rule is:

> Evidence truth is decided before wording.

A language model, TTS system, subtitle selector or voice actor may change *how* a fact is expressed. It must never decide *which facts are true*.

## Truth-bound dialogue

`WitnessStatementFacts` is the input contract.

It currently represents:

- witness role: clerk, civilian or security guard;
- suspect knowledge state: unknown, description-only or identified;
- face seen/not seen;
- mask/face covering;
- face confidence;
- clothing observation + description;
- vehicle observation + color/class description;
- plate knowledge: none, partial or full;
- direction of travel;
- violence/panic context.

Every evidence-bearing `ConversationTurn` also carries `EvidenceClaim` flags. Unit tests verify that a generated turn's claims are a subset of the facts supplied to the composer.

This is specifically intended to prevent dialogue hallucination such as:

- witness says a face was visible when it was masked;
- witness invents a plate;
- officer calls the player identified when the case is still unknown;
- witness names a vehicle that was never observed;
- dialogue silently upgrades a weak description into confirmed identity.

## Interview planning

Interviews should normally be short.

The composer can ask up to a configured number of question/answer pairs. Useful topics are selected from:

1. face / face covering;
2. clothing;
3. vehicle;
4. plate, only when a vehicle was observed;
5. direction.

The officer then summarizes the actual suspect-knowledge state and ends the statement.

Example, masked unknown robber:

```text
Officer: Did you get a look at the robber?
Clerk: No. Their face was covered. I couldn't tell you who it was.
Officer: What were they wearing?
Clerk: I remember a dark jacket and gray pants.
Officer: Did you get a look at the vehicle?
Clerk: They left in a black two-door coupe.
Officer: Any part of the license plate?
Clerk: I only caught part of it: 46E.
Officer: All right. For now we're looking for an unknown suspect.
```

That final line is useful gameplay information. The player now knows police have evidence, but not identity.

## Overhearing

The conversation is autonomous. The player does not have to press an interaction key to make the officer and witness talk.

Production behavior:

1. InvestigationDirector starts an interview when an officer and witness are available at the scene.
2. Conversation turns continue on their own timetable.
3. The presentation layer evaluates the listener's distance from the current speaker.
4. Audio volume falls with distance.
5. Geometry/interiors can later provide soft occlusion/muffling.
6. Subtitles should only appear when the player is actually within the configured hearing range, unless accessibility settings explicitly change this.
7. Walking away does not pause the interview.
8. Walking back into range lets the player hear whatever part of the conversation is happening at that moment.

An unknown suspect can stand nearby because the dialogue system itself has no psychic player identity. If another system has enough evidence to recognize the player, that system may separately trigger officer suspicion or arrest.

## Current in-game debug path

With `DebugHotkeys=true`, press **F7** in populated free roam.

The debug path:

- selects two nearby live ambient peds as temporary conversation actors;
- does not spawn or transform them into police/clerk models;
- constructs a synthetic masked-unknown-suspect witness statement;
- plays the generated officer/witness turns every few seconds;
- displays the current line only when the player is within the hearing radius;
- logs speaker, topic, semantic event, distance and LOS state;
- does not change case, warrant, economy or business data;
- cancels if a speaker streams out or the mission compatibility gate becomes active.

The actors are placeholders only. The real investigation stage will bind the exact same dialogue plan to actual spawned/reacquired officers and witnesses.

## Voice strategy

### Baseline

The baseline is authoritative subtitles plus validated GTA ambient reactions where they genuinely fit.

We should not play a GTA ambient line whose spoken meaning contradicts the subtitle.

### Optional generated voice layer

Los Santos Alive demonstrates that live external voice can feel natural in GTA. For Crime Overhaul, generated voice should remain an optional presentation provider behind the dialogue system.

Recommended architecture:

```text
ASI
  -> sends exact already-approved utterance + speaker style ID
localhost voice bridge
  -> Gemini TTS request
  -> returns/streams 24 kHz PCM
native audio presenter
  -> plays from NPC world position
  -> distance attenuation / stereo positioning / later occlusion
  -> lip-sync presentation while audio is active
```

Gemini TTS is particularly suitable because it can be told to recite exact text, while controlling speaking style, pace and tone. This is safer than asking an unconstrained conversational model to invent police evidence live.

Do not put API keys in the repository or save file. A future optional bridge should load a user-owned key from an ignored local `.env`/config file.

### Why not use unrestricted AI conversation for evidence

An unrestricted model could improvise a more natural conversation but may also invent legally significant details. That is unacceptable for this mod because the player makes gameplay decisions based on what police know.

If an AI text model is later used, its role is **surface realization only**:

```text
structured meaning:
  FACE_NOT_SEEN
  MASK_SEEN
  VEHICLE_SEEN: black coupe
  PLATE_PARTIAL: 46E

model task:
  paraphrase those facts naturally without adding information
```

The project must never parse the AI's prose back into case evidence.

## Future production integration

This slice does not pretend that police investigation scenes already exist.

When the roadmap reaches the real investigation stages:

- `InvestigationDirector` chooses which uncertainty is worth asking about;
- actual witness records populate `WitnessStatementFacts`;
- officer and witness entity handles live only in presentation/runtime state;
- the interview plan remains value-only domain data;
- case updates happen from the witness record, not from generated words;
- actual officers face/approach witnesses through validated task adapters;
- scenes abstract when far away;
- returning to the area can reconstruct the physical interview if its timing/state allows;
- the player can overhear the same evidence the case already owns.

## Acceptance tests

A production interview system is not complete until these scenarios pass:

1. Masked robbery, face never seen: no line claims a visible face or identity.
2. Unmasked high-confidence witness: usable face evidence can be discussed without automatically claiming confirmed identity.
3. No vehicle observed: no plate question or plate evidence is invented.
4. Partial plate: only the stored fragment is spoken.
5. Player leaves hearing range: conversation continues but presentation becomes inaudible.
6. Player returns mid-interview: later lines become audible without restarting the conversation.
7. Unknown suspect player walks near interview: dialogue system does not react as though officers magically know them.
8. Mission/cutscene begins: physical presentation safely stops/suspends.
9. Officer/witness streams out or dies: conversation presentation aborts cleanly.
10. Generated voice provider offline: subtitles still preserve all gameplay information.
