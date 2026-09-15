#!/usr/bin/env python3
"""Validate GCO dialogue data and simulate repeated selections.

Standard-library only so CI can gate dialogue content without adding a runtime dependency.
This is a content QA tool, not the eventual C++ DialogueDirector.
"""

from __future__ import annotations

import argparse
import json
import random
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

REQUIRED_LINE_FIELDS = {
    "id", "event", "speakerArchetypes", "intensity", "weight",
    "cooldownSeconds", "sessionMax", "reuseGroup", "noRepeatGroup",
    "conditions", "tags", "delivery",
}
REQUIRED_CONDITION_FIELDS = {"all", "any", "none"}
REQUIRED_DELIVERY_FIELDS = {"type", "text", "ambientSpeech", "voiceAsset"}


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def line_matches(line: dict[str, Any], context: set[str]) -> bool:
    conditions = line["conditions"]
    if not set(conditions["all"]).issubset(context):
        return False
    if conditions["any"] and not set(conditions["any"]).intersection(context):
        return False
    if set(conditions["none"]).intersection(context):
        return False
    return True


def build_test_context(lines: list[dict[str, Any]]) -> set[str]:
    context: set[str] = set()
    forbidden: set[str] = set()
    for line in lines:
        conditions = line["conditions"]
        context.update(conditions["all"])
        forbidden.update(conditions["none"])
    context.difference_update(forbidden)
    for line in lines:
        choices = [tag for tag in line["conditions"]["any"] if tag not in forbidden]
        if choices:
            context.add(choices[0])
    return context


def remove_immediate_repeat(
    candidates: list[dict[str, Any]],
    last_line_id: str | None,
) -> list[dict[str, Any]]:
    if last_line_id is None or len(candidates) <= 1:
        return candidates
    alternatives = [line for line in candidates if line["id"] != last_line_id]
    return alternatives or candidates


def weighted_choice(
    rng: random.Random,
    candidates: list[dict[str, Any]],
    usage: Counter[str],
    last_reuse: str | None,
) -> dict[str, Any]:
    weights: list[float] = []
    for line in candidates:
        weight = float(line["weight"])
        if usage[line["id"]] > 0:
            weight *= 0.15
        if last_reuse is not None and line["reuseGroup"] == last_reuse:
            weight *= 0.20
        weights.append(max(weight, 0.0001))
    return rng.choices(candidates, weights=weights, k=1)[0]


def validate(root: Path, simulations: int, seed: int) -> int:
    errors: list[str] = []
    manifest = load_json(root / "base.json")
    registry = load_json(root / "events.json")

    if manifest.get("schemaVersion") != 2:
        fail(errors, "base.json must use schemaVersion 2")
    if registry.get("schemaVersion") != 2:
        fail(errors, "events.json must use schemaVersion 2")

    known_context = set(registry.get("knownContextTags", []))
    event_rows = registry.get("events", [])
    events: dict[str, dict[str, Any]] = {}
    for row in event_rows:
        event_id = row.get("id")
        if not isinstance(event_id, str) or not event_id:
            fail(errors, "events.json contains an event without a valid id")
            continue
        if event_id in events:
            fail(errors, f"duplicate event id: {event_id}")
        events[event_id] = row

    for event_id, row in events.items():
        fallback = row.get("fallbackEvent")
        if fallback not in events:
            fail(errors, f"event {event_id} references unknown fallback {fallback}")
        minimum = row.get("minimumVariants")
        if not isinstance(minimum, int) or minimum < 0:
            fail(errors, f"event {event_id} has invalid minimumVariants")

    all_lines: list[dict[str, Any]] = []
    line_ids: set[str] = set()
    lines_by_event: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for pack_name in manifest.get("packs", []):
        pack_path = root / pack_name
        if not pack_path.is_file():
            fail(errors, f"manifest pack missing: {pack_name}")
            continue
        pack = load_json(pack_path)
        if pack.get("schemaVersion") != 2:
            fail(errors, f"{pack_name}: schemaVersion must be 2")
        if pack.get("fallbackEvent") not in events:
            fail(errors, f"{pack_name}: unknown fallbackEvent {pack.get('fallbackEvent')}")
        if not pack.get("speakerGroups"):
            fail(errors, f"{pack_name}: speakerGroups must not be empty")

        for index, line in enumerate(pack.get("lines", [])):
            where = f"{pack_name} line {index}"
            missing = REQUIRED_LINE_FIELDS.difference(line)
            if missing:
                fail(errors, f"{where}: missing fields {sorted(missing)}")
                continue
            line_id = line["id"]
            if line_id in line_ids:
                fail(errors, f"duplicate line id: {line_id}")
            line_ids.add(line_id)

            event_id = line["event"]
            if event_id not in events:
                fail(errors, f"{line_id}: unknown event {event_id}")
            if not isinstance(line["weight"], (int, float)) or line["weight"] <= 0:
                fail(errors, f"{line_id}: weight must be > 0")
            if not isinstance(line["cooldownSeconds"], int) or line["cooldownSeconds"] < 0:
                fail(errors, f"{line_id}: cooldownSeconds must be >= 0")
            if line["sessionMax"] is not None and (not isinstance(line["sessionMax"], int) or line["sessionMax"] < 1):
                fail(errors, f"{line_id}: sessionMax must be null or >= 1")
            if not isinstance(line["intensity"], int) or not 0 <= line["intensity"] <= 5:
                fail(errors, f"{line_id}: intensity must be an integer from 0 to 5")
            if not line["speakerArchetypes"]:
                fail(errors, f"{line_id}: speakerArchetypes must not be empty")
            if not line["reuseGroup"] or not line["noRepeatGroup"]:
                fail(errors, f"{line_id}: reuse/no-repeat groups must not be empty")

            conditions = line["conditions"]
            if set(conditions) != REQUIRED_CONDITION_FIELDS:
                fail(errors, f"{line_id}: conditions must contain exactly all/any/none")
            else:
                for bucket in ("all", "any", "none"):
                    unknown = set(conditions[bucket]).difference(known_context)
                    if unknown:
                        fail(errors, f"{line_id}: unknown context tags in {bucket}: {sorted(unknown)}")

            delivery = line["delivery"]
            if set(delivery) != REQUIRED_DELIVERY_FIELDS:
                fail(errors, f"{line_id}: delivery must contain exactly type/text/ambientSpeech/voiceAsset")
            else:
                delivery_type = delivery["type"]
                if delivery_type not in {"subtitle", "silent"}:
                    fail(errors, f"{line_id}: invalid delivery type {delivery_type}")
                if delivery_type == "subtitle" and (not isinstance(delivery["text"], str) or not delivery["text"].strip()):
                    fail(errors, f"{line_id}: subtitle delivery requires non-empty text")
                if delivery_type == "silent" and delivery["text"] is not None:
                    fail(errors, f"{line_id}: silent delivery must have null text")
                # Roadmap 310/312 are not implemented here. Non-null audio references are forbidden.
                if delivery["ambientSpeech"] is not None:
                    fail(errors, f"{line_id}: ambientSpeech is unvalidated; keep null until roadmap 310")
                if delivery["voiceAsset"] is not None:
                    fail(errors, f"{line_id}: voiceAsset is unvalidated; keep null until roadmap 312")

            all_lines.append(line)
            lines_by_event[event_id].append(line)

    for event_id, row in events.items():
        if not row.get("contentRequired", False):
            continue
        count = len(lines_by_event[event_id])
        minimum = row["minimumVariants"]
        if count < minimum:
            fail(errors, f"event {event_id}: {count} variants, minimum is {minimum}")

    if errors:
        print("Dialogue validation FAILED")
        for message in errors:
            print(f" - {message}")
        return 1

    rng = random.Random(seed)
    print(f"Dialogue schema OK: {len(all_lines)} lines across {len(events)} semantic events")
    print(f"Simulation: {simulations} encounters/event, seed={seed}")

    simulation_errors: list[str] = []
    for event_id, event_row in events.items():
        if not event_row.get("contentRequired", False) or event_id.startswith("fallback."):
            continue
        direct = lines_by_event[event_id]
        context = build_test_context(direct)
        usage: Counter[str] = Counter()
        last_used: dict[str, int] = {}
        sequence: list[str] = []
        fallback_count = 0
        starved = 0
        last_reuse: str | None = None
        last_line_id: str | None = None
        game_time = 0

        for _encounter in range(simulations):
            game_time += 600
            candidates = [
                line for line in direct
                if line_matches(line, context)
                and game_time - last_used.get(line["id"], -10**9) >= line["cooldownSeconds"]
            ]
            candidates = remove_immediate_repeat(candidates, last_line_id)

            if not candidates:
                fallback_id = event_row["fallbackEvent"]
                candidates = [line for line in lines_by_event[fallback_id] if line_matches(line, context)]
                candidates = remove_immediate_repeat(candidates, last_line_id)
                fallback_count += 1

            if not candidates:
                candidates = list(lines_by_event.get("fallback.silent", []))
                candidates = remove_immediate_repeat(candidates, last_line_id)

            if not candidates:
                starved += 1
                continue

            selected = weighted_choice(rng, candidates, usage, last_reuse)
            line_id = selected["id"]
            sequence.append(line_id)
            usage[line_id] += 1
            last_used[line_id] = game_time
            last_reuse = selected["reuseGroup"]
            last_line_id = line_id

        max_streak = 0
        current = 0
        previous: str | None = None
        for line_id in sequence:
            if line_id == previous:
                current += 1
            else:
                current = 1
                previous = line_id
            max_streak = max(max_streak, current)

        direct_ids = {line["id"] for line in direct}
        direct_used = len(direct_ids.intersection(usage.keys()))
        fallback_rate = fallback_count / simulations if simulations else 0.0
        if starved:
            simulation_errors.append(f"{event_id}: {starved} selections starved")
        if len(direct) >= 4 and direct_used < min(4, len(direct)):
            simulation_errors.append(f"{event_id}: only {direct_used}/{len(direct)} direct variants selected")
        if max_streak > 2:
            simulation_errors.append(f"{event_id}: same line repeated {max_streak} times consecutively")
        if fallback_rate > 0.20:
            simulation_errors.append(f"{event_id}: fallback rate {fallback_rate:.1%} exceeds 20%")

        top = usage.most_common(3)
        print(f" - {event_id}: direct={len(direct)} used={direct_used} fallback={fallback_rate:.1%} maxStreak={max_streak} top={top}")

    if simulation_errors:
        print("Dialogue selection simulation FAILED")
        for message in simulation_errors:
            print(f" - {message}")
        return 2

    print("Dialogue selection simulation PASSED: no starvation or excessive immediate repetition detected")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("data/dialogue"))
    parser.add_argument("--simulate", type=int, default=120)
    parser.add_argument("--seed", type=int, default=1337)
    args = parser.parse_args()
    if args.simulate < 1:
        parser.error("--simulate must be >= 1")
    return validate(args.root, args.simulate, args.seed)


if __name__ == "__main__":
    raise SystemExit(main())
