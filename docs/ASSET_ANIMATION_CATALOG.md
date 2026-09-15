# GTA Crime Overhaul — Asset & Animation Catalog

This is the working content catalog for the overhaul. Names in this file are references to content that already exists inside GTA V or research candidates from public GTA V data. The mod does not redistribute Rockstar assets.

## Status legend

- `VERIFIED_DATA` — name appears in current GTA V data/reference material.
- `CANDIDATE_TEST` — plausible for the mechanic but must be validated in our target GTA V builds before production use.
- `VERIFIED_IN_GAME` — reserved for assets we personally validate in Legacy/Enhanced.
- `CUSTOM_REQUIRED` — existing GTA content is insufficient; create an original compatible asset.
- `FALLBACK_ONLY` — acceptable when primary synchronized content fails.

No item becomes production-authoritative until it reaches `VERIFIED_IN_GAME` or has a documented fallback.

---

# 1. Duffel/heist bag candidates

| Model | Intended use | Status | Notes |
|---|---|---:|---|
| `prop_cs_duffel_01` | generic small/standard bag | VERIFIED_DATA | Good first generic prop candidate. |
| `prop_cs_duffel_01b` | generic duffel visual variant | VERIFIED_DATA | Test protagonist attachment offsets. |
| `prop_cs_heist_bag_01` | heist bag | VERIFIED_DATA | Candidate for carried/dropped loot. |
| `prop_cs_heist_bag_02` | heist bag | VERIFIED_DATA | Candidate for carried/dropped loot. |
| `prop_cs_heist_bag_strap_01` | bag strap visual | VERIFIED_DATA | Optional polish only. |
| `p_ld_heist_bag_01` | heist bag variant | VERIFIED_DATA | Test animation compatibility. |
| `p_ld_heist_bag_s_1` | heist bag variant | VERIFIED_DATA | Test animation compatibility. |
| `hei_p_heist_flecca_bag` | Fleeca-specific heist bag | VERIFIED_DATA | Strong candidate for bank work if it loads in current build. |
| `hei_p_m_bag_var22_arm_s` | arm/scene bag used by heist grabs | CANDIDATE_TEST | Widely used with ornate-bank synchronized cash-grab scenes. |
| `ch_p_m_bag_var02_arm_s` | later heist arm bag | CANDIDATE_TEST | Alternative synchronized-scene bag. |

### Gameplay rule

The logical `LootContainer` owns capacity and value. The visual bag is presentation only. If a visual model fails to stream, the player must still be able to complete the robbery through a fallback interaction.

---

# 2. Cash, register and vault loot candidates

| Model / group | Intended use | Status | Notes |
|---|---|---:|---|
| `prop_anim_cash_note` | transient note in hand | VERIFIED_DATA | Use as a short-lived effect, not thousands of persistent entities. |
| `prop_anim_cash_note_b` | transient note variant | VERIFIED_DATA | Same rule. |
| `prop_anim_cash_pile_01` | cash pile | VERIFIED_DATA | Register/table visual. |
| `prop_anim_cash_pile_02` | cash pile | VERIFIED_DATA | Variant. |
| `prop_cash_note_01` | cash note | VERIFIED_DATA | Small visual only. |
| `prop_cs_cash_note_01` | cash note | VERIFIED_DATA | Small visual only. |
| `prop_cash_case_01` | cash case | VERIFIED_DATA | Commercial target/briefcase candidate. |
| `prop_cash_trolly` | generic cash trolley | VERIFIED_DATA | Test versus heist trolley assets. |
| `hei_prop_hei_cash_trolly_01` | full heist cash trolley | VERIFIED_DATA | Primary bank cash-grab candidate. |
| `hei_prop_hei_cash_trolly_03` | empty trolley | CANDIDATE_TEST | Commonly paired with full trolley after synchronized grab. |
| `hei_prop_cash_crate_empty` | emptied cash crate | VERIFIED_DATA | Target dressing / result state. |
| `hei_prop_heist_cash_bag_01` | heist cash bag | VERIFIED_DATA | Loot dressing candidate. |

### Runtime rule

Cash visuals must be pooled/capped. Value moves through logical state; props represent only visible source/handful/bag transitions.

---

# 3. Bank equipment candidates

| Model | Intended use | Status | Notes |
|---|---|---:|---|
| `hei_prop_heist_drill` | drill | VERIFIED_DATA | Generic heist drill candidate. |
| `hei_p_heist_flecca_drill` | Fleeca drill | VERIFIED_DATA | Strong Fleeca candidate. |
| `hei_prop_hei_drill_hole` | drill-hole result | VERIFIED_DATA | Optional visual result. |
| `hei_prop_heist_thermite` | thermite charge | VERIFIED_DATA | Higher-tier target only; validate scenes. |
| `hei_bio_heist_card` | keycard prop | VERIFIED_DATA | Security/keycard interaction candidate. |
| `hei_prop_hst_usb_drive` | USB | VERIFIED_DATA | Optional equipment/intel interaction. |
| `hei_prop_bank_alarm_01` | bank alarm prop | VERIFIED_DATA | Can anchor logical alarm if present. |

Do not promise a minigame because a prop exists. Equipment is added only after the interaction and failure/cleanup path works.

---

# 4. Candidate animation dictionaries and clips

## Store/clerk threat and compliance

| Action | Dictionary | Clip | Status |
|---|---|---|---:|
| anxious hands up | `missheist_agency2ahands_up` | `handsup_anxious` | CANDIDATE_TEST |
| hands up | `random@arrests` | `idle_2_hands_up` | CANDIDATE_TEST |
| surrender | `random@arrests@busted` | `idle_a` | CANDIDATE_TEST |
| generic surrender | `anim@mp_player_intuppersurrender` | `idle_a` | CANDIDATE_TEST |
| restrained/cuffed idle | `mp_arresting` | `idle` | CANDIDATE_TEST |
| shop-robbery/search motion | `random@shop_robbery` | `robbery_action_b` | CANDIDATE_TEST |
| mugging hands-up | `random@mugging3` | `handsup_standing_base` | CANDIDATE_TEST |

`TASK_COWER`, flee/combat tasks and scenarios should be preferred over forcing looping clips when they produce more robust AI behavior.

## Phone/reporting

| Action | Dictionary | Clip | Status |
|---|---|---|---:|
| phone text idle | `amb@world_human_stand_mobile@male@text@base` | `base` | CANDIDATE_TEST |
| phone call listen | `cellphone@` | `cellphone_call_listen_base` | CANDIDATE_TEST |
| text to call | `cellphone@` | `cellphone_text_to_call` | CANDIDATE_TEST |
| call out | `cellphone@` | `cellphone_call_out` | CANDIDATE_TEST |
| call to text | `cellphone@` | `cellphone_call_to_text` | CANDIDATE_TEST |
| handheld radio gesture | `random@arrests` | `generic_radio_enter` | CANDIDATE_TEST |

Phone prop candidates include `prop_amb_phone`, `prop_npc_phone`, `prop_npc_phone_02`, and `prop_cs_phone_01`. Test bone offsets and male/female behavior.

## Police investigation/scene presentation

| Action | Dictionary/scenario | Clip | Status |
|---|---|---|---:|
| cop idle | scenario `WORLD_HUMAN_COP_IDLES` | n/a | CANDIDATE_TEST |
| cop idle male | `amb@world_human_cop_idles@male@base` | `base` | CANDIDATE_TEST |
| cop idle female | `amb@world_human_cop_idles@female@base` | `base` | CANDIDATE_TEST |
| clipboard idle | `amb@world_human_clipboard@male@base` | `base` | CANDIDATE_TEST |
| notepad/clipboard | `missheistdockssetup1clipboard@base` | `base` | CANDIDATE_TEST |
| medic/body check kneel | `amb@medic@standing@kneel@base` | `base` | CANDIDATE_TEST |
| medic kneel | scenario `CODE_HUMAN_MEDIC_KNEEL` | n/a | CANDIDATE_TEST |
| guard stand | scenario `WORLD_HUMAN_GUARD_STAND` | n/a | CANDIDATE_TEST |

Notepad presentation candidates: `prop_notepad_01` and `prop_pencil_01`. Validate exact hand bones/offsets before use.

## Bank cash grab

Primary candidate dictionary:

`anim@heists@ornate_bank@grab_cash`

Candidate ped clips:
- `intro`
- `grab`
- `grab_idle`
- `exit`

Candidate bag clips:
- `bag_intro`
- `bag_grab`
- `bag_grab_idle`
- `bag_exit`

Candidate trolley clip:
- `cart_cash_dissapear`

This interaction should use synchronized scenes because the ped, bag and trolley must remain aligned. Event markers such as cash-appearance/release markers may be useful for transient cash-hand props, but must be validated in-game.

Other candidates:

| Action | Dictionary | Clips | Status |
|---|---|---|---:|
| duffel money grab | `anim@heists@money_grab@duffel` | `enter`, `loop`, `exit_strap` / related bag clips | CANDIDATE_TEST |
| cash trolley sequence | `anim@heists@ornate_bank@cash_trolley` | `enter`, `idle`, `exit` | CANDIDATE_TEST |
| bank hack | `anim@heists@ornate_bank@hack` | `enter`, `idle`, `exit` | CANDIDATE_TEST |
| Fleeca vault door | `anim@heists@fleeca_bank@bank_vault_door` | `bank_vault_door_opens` | CANDIDATE_TEST |
| Fleeca drilling family | `anim@heists@fleeca_bank@drilling` | multiple drill/bag/door clips | CANDIDATE_TEST |
| keypad | `anim@heists@keypad@` | `enter`, `idle_a`, `exit` | CANDIDATE_TEST |
| keycard | `anim@heists@keycard@` | `idle_a`, `exit`, related | CANDIDATE_TEST |
| box carry fallback | `anim@heists@box_carry@` | `idle` | FALLBACK_ONLY |

Do not hard-code a Fleeca drilling choreography until every prop and clip alignment has been tested in the actual bank interior.

---

# 5. Vehicle/trunk presentation candidates

No single animation should be assumed to fit every vehicle. Implementation order:

1. use the vehicle's boot/trunk door native and physical door state;
2. compute a class/model-specific rear interaction anchor;
3. use a short generic reach/store animation where it looks acceptable;
4. fall back to a timed interaction with the trunk open if animation alignment is poor;
5. never break inventory correctness because a cosmetic animation failed.

Research exact boot door/bone behavior per common classes first: sedan, coupe, SUV, hatchback, van, pickup. Exclude unusual vehicles until manually supported.

---

# 6. Masks and disguise content

Use GTA V ped components/props first.

Research process:
1. export/inspect current component variations for Franklin/Michael/Trevor and supported freemode models;
2. identify full masks, ski masks, scarves/bandana-like components and hats/eyewear;
3. classify each as `face_hidden_full`, `face_hidden_lower`, `face_visible`, or `cosmetic_only`;
4. map the classification to recognition multipliers rather than hard-coding drawable indexes inside the law system.

If no satisfying lower-face handkerchief exists for a protagonist, mark the feature `CUSTOM_REQUIRED` and create an original GTA-compatible clothing asset. Do not port the Red Dead Redemption 2 bandana.

---

# 7. Store and bank interiors

## Existing stores

The first 24/7 prototype must be surveyed from the actual Story Mode world. We should not ship guessed coordinates copied blindly from a multiplayer script.

Per-store data requires:
- volume bounds;
- clerk anchor;
- register anchors;
- optional safe;
- door/exit anchors;
- customer zone;
- camera definitions;
- police staging positions;
- getaway-road hints.

## Fleeca

Known GTA V interior data and existing Story Mode interior projects indicate Fleeca vault interiors can be accessed. Our own tests must identify the correct IPL/interior/door path for Legacy and Enhanced before Stage 16 is enabled.

## Pacific Standard

Likewise validate the public-deposit-bank/vault interior and all relevant doors/entities before enabling the robbery target.

## Union Depository

Do not build this first. Existing Story Mode interior work demonstrates access to the vault/parking area, but the major-score gameplay is gated behind a stable Fleeca and Pacific Standard implementation.

---

# 8. Peds and roles

Use ambient GTA V populations where possible instead of permanently spawning custom NPCs.

Logical roles:
- `CLERK`
- `CUSTOMER`
- `CIVILIAN_WITNESS`
- `SECURITY_GUARD`
- `PATROL_OFFICER`
- `INVESTIGATING_OFFICER`
- `TACTICAL_OFFICER`
- `MEDIC_OPTIONAL`

The logical profile owns memory/personality. The physical ped is replaceable after streaming.

Clerk personality properties:
- fear baseline;
- compliance probability;
- panic probability;
- alarm tendency;
- resistance/fight tendency;
- memory confidence modifier;
- prior-robbery trauma/aggression;
- optional weapon profile.

Do not bind a personality to one exact ped model unless the store deliberately owns a persistent named clerk profile.

---

# 9. CCTV content

Use physical camera objects when present and/or data-defined logical camera anchors. We are not extracting video frames or performing image recognition.

Each camera definition needs:
- position;
- forward vector or yaw/pitch;
- FOV;
- near/far observation range;
- face zone quality;
- plate zone quality;
- active/disabled/destroyed state;
- recording retention rule.

Destroying a camera after a stored capture does not erase previously stored evidence unless a later gameplay system explicitly models destruction of the recording source.

---

# 10. Audio and dialogue asset strategy

Three layers:

### Layer A — Existing ambient GTA speech
Best for short barks:
- fear/shock;
- curses;
- generic greetings/responses;
- threats;
- stay-down/fall-back style barks;
- police/civilian ambient reactions where a compatible speech name exists.

Use `PLAY_PED_AMBIENT_SPEECH_NATIVE` through our audio wrapper, with a validated speech name and compatible ped voice. Do not assume every ped can play every line.

### Layer B — Subtitle-first semantic dialogue
Best for information-rich lines:
- police interviews;
- witness descriptions;
- clerk recognizing a repeat robber;
- exact vehicle/plate descriptions;
- robbery demands;
- case-specific radio information.

The semantic event is authoritative. Audio may be a generic matching bark while the subtitle conveys the precise information.

### Layer C — Original/authorized voice packs
Optional polish. Voice files must be original, licensed or explicitly authorized. Do not clone Rockstar actors.

---

# 11. Content that must remain data-driven

Never bake the following directly into the C++ state machines:
- store/bank coordinates;
- model names;
- animation dictionaries/clips;
- speech names;
- price values;
- bag capacity;
- evidence thresholds;
- clerk personality weights;
- dialogue lines;
- camera coverage;
- police response profiles.

C++ owns rules and validation; JSON owns tunable content.

---

# 12. Immediate validation queue

Before Stage 3 production implementation, validate these first:

1. `missheist_agency2ahands_up / handsup_anxious` on likely clerk peds.
2. `random@arrests / idle_2_hands_up` as fallback.
3. `cellphone@ / cellphone_call_listen_base` + one phone prop.
4. `random@shop_robbery / robbery_action_b` for simple loot interaction.
5. `amb@world_human_cop_idles@male@base / base`.
6. `missheistdockssetup1clipboard@base / base` + notepad/pencil candidates.
7. `prop_cs_heist_bag_02` and `hei_p_heist_flecca_bag` attachment behavior.
8. `anim@heists@ornate_bank@grab_cash` synchronized scene with `hei_prop_hei_cash_trolly_01` and a compatible arm bag.
9. vehicle plate read/write using an owned test car.
10. trunk open/close and rear interaction anchor on five common vehicle classes.
11. one Fleeca interior and vault door path on Legacy.
12. the same Fleeca path on Enhanced.

Only after these pass should we promote the corresponding content into production tables.
