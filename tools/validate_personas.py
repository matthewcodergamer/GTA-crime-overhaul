#!/usr/bin/env python3
"""Validate speaker persona, voice-pool and subtle age-variation policy.

This tool intentionally treats generated voices and facial assets as REFERENCE_ONLY until
manual in-game validation promotes them. It also prevents age/gender metadata from becoming
a source of evidence or stereotyped dialogue.
"""

from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path

ROOT = Path("data/dialogue")

OFFICIAL_GEMINI_VOICE_GENDER = {
    "Achernar": "feminine",
    "Achird": "masculine",
    "Algenib": "masculine",
    "Algieba": "masculine",
    "Alnilam": "masculine",
    "Aoede": "feminine",
    "Autonoe": "feminine",
    "Callirrhoe": "feminine",
    "Charon": "masculine",
    "Despina": "feminine",
    "Enceladus": "masculine",
    "Erinome": "feminine",
    "Fenrir": "masculine",
    "Gacrux": "feminine",
    "Iapetus": "masculine",
    "Kore": "feminine",
    "Laomedeia": "feminine",
    "Leda": "feminine",
    "Orus": "masculine",
    "Puck": "masculine",
    "Pulcherrima": "feminine",
    "Rasalgethi": "masculine",
    "Sadachbia": "masculine",
    "Sadaltager": "masculine",
    "Schedar": "masculine",
    "Sulafat": "feminine",
    "Umbriel": "masculine",
    "Vindemiatrix": "feminine",
    "Zephyr": "feminine",
    "Zubenelgenubi": "masculine",
}

EXPECTED_ROLES = {
    "clerk",
    "witness",
    "guard",
    "patrol_officer",
    "investigating_officer",
}
EXPECTED_GENDERS = {"masculine", "feminine"}
EXPECTED_AGES = {"young_adult", "adult", "older_adult"}
EXPECTED_REGISTERS = {"grounded_contemporary", "neutral_professional", "measured"}

CHEESY_SLANG = {
    "no cap",
    "on god",
    "bruh",
    "fr fr",
    "rizz",
    "yeet",
    "slay queen",
    "lit af",
    "highkey",
}
OLDER_CARICATURES = {
    "back in my day",
    "young whippersnapper",
    "sonny boy",
    "kids these days",
}


def load(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def main() -> int:
    errors: list[str] = []
    manifest = load(ROOT / "base.json")
    persona_file = manifest.get("personaRegistry")
    if persona_file != "personas.json":
        errors.append("base.json must register personas.json as personaRegistry")
        persona_file = "personas.json"

    personas = load(ROOT / persona_file)
    if personas.get("schemaVersion") != 1:
        errors.append("personas.json must use schemaVersion 1")

    if set(personas.get("voiceGenders", [])) != EXPECTED_GENDERS:
        errors.append("personas.json voiceGenders must be exactly masculine/feminine")
    if set(personas.get("ageBands", [])) != EXPECTED_AGES:
        errors.append("personas.json ageBands must be young_adult/adult/older_adult")
    if set(personas.get("speechRegisters", [])) != EXPECTED_REGISTERS:
        errors.append("personas.json speechRegisters are incomplete")

    policy = personas.get("policy", {})
    if policy.get("forcedSlangAllowed") is not False:
        errors.append("forcedSlangAllowed must remain false")
    if policy.get("caricatureAllowed") is not False:
        errors.append("caricatureAllowed must remain false")
    never_affects = set(policy.get("neverAffects", []))
    required_never = {"evidence_truth", "case_confidence", "recognition", "warrant_state"}
    if not required_never.issubset(never_affects):
        errors.append("persona policy must explicitly forbid demographic traits from changing evidence/case/recognition/warrant truth")
    if policy.get("ambientPedAgeInference") != "do_not_guess":
        errors.append("ambient ped age must not be guessed from appearance/model")

    provider = personas.get("provider", {})
    if provider.get("status") != "reference_only":
        errors.append("generated voice provider must remain reference_only until manual in-game validation")

    seen_voice_sets: set[str] = set()
    coverage: set[tuple[str, str, str]] = set()
    for row in personas.get("voiceSets", []):
        voice_set_id = row.get("id")
        role = row.get("role")
        gender = row.get("voiceGender")
        age = row.get("ageBand")
        if not isinstance(voice_set_id, str) or not voice_set_id:
            errors.append("voice set missing id")
            continue
        if voice_set_id in seen_voice_sets:
            errors.append(f"duplicate voice set id: {voice_set_id}")
        seen_voice_sets.add(voice_set_id)
        if role not in EXPECTED_ROLES:
            errors.append(f"{voice_set_id}: invalid role {role}")
        if gender not in EXPECTED_GENDERS:
            errors.append(f"{voice_set_id}: invalid gender {gender}")
        if age not in EXPECTED_AGES:
            errors.append(f"{voice_set_id}: invalid age band {age}")
        if row.get("status") != "reference_only":
            errors.append(f"{voice_set_id}: voice set must remain reference_only before in-game validation")
        candidates = row.get("candidateVoices", [])
        if len(candidates) < 2:
            errors.append(f"{voice_set_id}: needs at least two candidate voices to avoid one-voice monoculture")
        for voice in candidates:
            official_gender = OFFICIAL_GEMINI_VOICE_GENDER.get(voice)
            if official_gender is None:
                errors.append(f"{voice_set_id}: unknown Gemini voice {voice}")
            elif official_gender != gender:
                errors.append(f"{voice_set_id}: {voice} is {official_gender}, not {gender}")
        coverage.add((role, gender, age))

    expected_coverage = {
        (role, gender, age)
        for role in EXPECTED_ROLES
        for gender in EXPECTED_GENDERS
        for age in EXPECTED_AGES
    }
    missing_voice_sets = sorted(expected_coverage - coverage)
    if missing_voice_sets:
        errors.append(f"missing role/gender/age voice sets: {missing_voice_sets}")

    lip_sync = personas.get("lipSync", {})
    if lip_sync.get("strategy") != "pcm_envelope_gated_facial_animation":
        errors.append("lip sync must be driven from PCM envelope, not subtitle duration")
    if lip_sync.get("status") != "reference_only":
        errors.append("lip-sync assets must remain reference_only until Legacy/Enhanced manual validation")
    talk = lip_sync.get("talkCandidate", {})
    if not talk.get("dictionary") or not talk.get("clip") or talk.get("status") != "reference_only":
        errors.append("lip-sync talk candidate must be non-empty and reference_only")
    for gender in EXPECTED_GENDERS:
        neutral = lip_sync.get("neutralCandidates", {}).get(gender, {})
        if not neutral.get("dictionary") or not neutral.get("clip") or neutral.get("status") != "reference_only":
            errors.append(f"neutral facial candidate for {gender} must be non-empty and reference_only")

    all_lines: list[dict] = []
    persona_coverage: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    for pack_name in manifest.get("packs", []):
        pack = load(ROOT / pack_name)
        for line in pack.get("lines", []):
            all_lines.append(line)
            delivery = line.get("delivery", {})
            text = delivery.get("text")
            if isinstance(text, str):
                lower = text.casefold()
                for phrase in CHEESY_SLANG:
                    if phrase in lower:
                        errors.append(f"{line.get('id')}: prohibited forced slang phrase '{phrase}'")
                for phrase in OLDER_CARICATURES:
                    if phrase in lower:
                        errors.append(f"{line.get('id')}: prohibited age caricature phrase '{phrase}'")

            persona = line.get("persona")
            if persona is None:
                continue
            ages = persona.get("ageBands", [])
            genders = persona.get("voiceGenders", [])
            registers = persona.get("speechRegisters", [])
            if not ages or any(age not in EXPECTED_AGES | {"any"} for age in ages):
                errors.append(f"{line.get('id')}: invalid persona ageBands")
            if not genders or any(gender not in EXPECTED_GENDERS | {"any"} for gender in genders):
                errors.append(f"{line.get('id')}: invalid persona voiceGenders")
            if not registers or any(register not in EXPECTED_REGISTERS | {"any"} for register in registers):
                errors.append(f"{line.get('id')}: invalid persona speechRegisters")
            # Gender changes voice selection, not stereotyped wording. Age can bias wording subtly.
            if "any" not in genders:
                errors.append(f"{line.get('id')}: gender-specific prose is forbidden; use voiceGenders=['any'] and let voice selection handle gender")
            for age in ages:
                if age != "any":
                    persona_coverage[line.get("event", "")][age] += 1

    required_age_events = {
        "officer.interview.start": 2,
        "witness.face.not_seen": 2,
        "clerk.comply.fast": 2,
    }
    for event, minimum in required_age_events.items():
        for age in ("young_adult", "older_adult"):
            count = persona_coverage[event][age]
            if count < minimum:
                errors.append(f"{event}: needs at least {minimum} subtle {age} variants, found {count}")

    if errors:
        print("Dialogue persona validation FAILED")
        for error in errors:
            print(f" - {error}")
        return 1

    print(
        "Dialogue persona validation PASSED: "
        f"{len(seen_voice_sets)} role/gender/age voice sets, "
        f"{len(all_lines)} total dialogue lines, age variation coverage present, no forced slang/caricature detected"
    )
    print("Generated voices and facial assets remain REFERENCE_ONLY pending manual Legacy/Enhanced validation")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
