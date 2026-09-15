#!/usr/bin/env python3
"""Deterministic anti-repetition/starvation simulation for dialogue pack data."""

from __future__ import annotations

import argparse
import json
import random
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


def read_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def matches(line: dict[str, Any], context: set[str]) -> bool:
    rule = line["conditions"]
    return (
        set(rule["all"]).issubset(context)
        and (not rule["any"] or bool(set(rule["any"]) & context))
        and not bool(set(rule["none"]) & context)
    )


def context_for(lines: list[dict[str, Any]]) -> set[str]:
    context: set[str] = set()
    forbidden: set[str] = set()
    for line in lines:
        context.update(line["conditions"]["all"])
        forbidden.update(line["conditions"]["none"])
    context.difference_update(forbidden)
    for line in lines:
        options = [tag for tag in line["conditions"]["any"] if tag not in forbidden]
        if options:
            context.add(options[0])
    return context


def choose(
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
        if last_reuse == line["reuseGroup"]:
            weight *= 0.20
        weights.append(max(weight, 0.0001))
    return rng.choices(candidates, weights=weights, k=1)[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("data/dialogue"))
    parser.add_argument("--encounters", type=int, default=120)
    parser.add_argument("--seed", type=int, default=1337)
    args = parser.parse_args()

    root = args.root
    manifest = read_json(root / "base.json")
    registry = read_json(root / "events.json")
    events = {row["id"]: row for row in registry["events"]}

    lines_by_event: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for pack_name in manifest["packs"]:
        pack = read_json(root / pack_name)
        for line in pack["lines"]:
            lines_by_event[line["event"]].append(line)

    rng = random.Random(args.seed)
    failures: list[str] = []

    for event_id, event in events.items():
        if not event["contentRequired"] or event_id.startswith("fallback."):
            continue

        direct = lines_by_event[event_id]
        context = context_for(direct)
        usage: Counter[str] = Counter()
        last_used: dict[str, int] = {}
        sequence: list[str] = []
        fallback_count = 0
        total_calls = 0
        last_reuse: str | None = None
        game_time = 0

        for encounter in range(args.encounters):
            game_time += 600
            session_counts: Counter[str] = Counter()
            used_no_repeat_groups: set[str] = set()
            calls = 2 if encounter % 10 == 0 else 1

            for _ in range(calls):
                total_calls += 1

                def eligible(line: dict[str, Any]) -> bool:
                    if not matches(line, context):
                        return False
                    if game_time - last_used.get(line["id"], -10**9) < line["cooldownSeconds"]:
                        return False
                    session_max = line["sessionMax"]
                    if session_max is not None and session_counts[line["id"]] >= session_max:
                        return False
                    if line["noRepeatGroup"] in used_no_repeat_groups:
                        return False
                    return True

                candidates = [line for line in direct if eligible(line)]
                if not candidates:
                    fallback_count += 1
                    fallback_id = event["fallbackEvent"]
                    candidates = [line for line in lines_by_event[fallback_id] if eligible(line)]
                if not candidates:
                    candidates = [line for line in lines_by_event["fallback.silent"] if matches(line, context)]
                if not candidates:
                    failures.append(f"{event_id}: selector starved with no fallback")
                    continue

                selected = choose(rng, candidates, usage, last_reuse)
                line_id = selected["id"]
                sequence.append(line_id)
                usage[line_id] += 1
                session_counts[line_id] += 1
                used_no_repeat_groups.add(selected["noRepeatGroup"])
                last_used[line_id] = game_time
                last_reuse = selected["reuseGroup"]

        direct_ids = {line["id"] for line in direct}
        unique_direct = len(direct_ids & set(usage))
        fallback_rate = fallback_count / total_calls if total_calls else 0.0

        max_streak = 0
        current = 0
        previous: str | None = None
        for line_id in sequence:
            if line_id == previous:
                current += 1
            else:
                previous = line_id
                current = 1
            max_streak = max(max_streak, current)

        if unique_direct < min(4, len(direct)):
            failures.append(f"{event_id}: only {unique_direct}/{len(direct)} direct variants were ever selected")
        if fallback_rate > 0.20:
            failures.append(f"{event_id}: fallback rate {fallback_rate:.1%} exceeds 20%")
        if max_streak > 2:
            failures.append(f"{event_id}: same line repeated {max_streak} consecutive times")

        print(
            f"{event_id}: variants={len(direct)} used={unique_direct} "
            f"fallback={fallback_rate:.1%} maxStreak={max_streak} top={usage.most_common(3)}"
        )

    if failures:
        print("Dialogue policy simulation FAILED")
        for failure in failures:
            print(f" - {failure}")
        return 1

    print("Dialogue policy simulation PASSED: cooldown/session/no-repeat/fallback rules did not starve selections")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
