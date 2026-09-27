"""Inspect structural and simple statistical patterns in a generated Maplex config."""

from __future__ import annotations

import argparse
import json
import math
import string
from collections import Counter
from pathlib import Path
from typing import Any

DEFAULT_CONFIG = Path(__file__).resolve().parents[1] / "examples" / "generated" / "generated-config.json"
DEFAULT_SYMBOL_POOL = "".join(chr(codepoint) for codepoint in range(33, 127))


def load_config(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as config_file:
        config = json.load(config_file)
    if not isinstance(config, dict):
        raise ValueError("Configuration root must be a JSON object.")
    return config


def analyze(config: dict[str, Any]) -> tuple[list[str], list[str]]:
    issues: list[str] = []
    notes: list[str] = []
    alphabet = config.get("alphabet")
    mappings = config.get("mappings")
    triggers = config.get("triggers")

    if not isinstance(alphabet, str) or not alphabet:
        return ["Missing or invalid alphabet."], notes
    if not isinstance(mappings, dict) or not mappings:
        return ["Missing or invalid mappings object."], notes
    if not isinstance(triggers, list) or not triggers:
        return ["Missing or invalid triggers array."], notes

    notes.append(f"Alphabet: {len(alphabet)} symbols")
    notes.append(f"Mappings: {len(mappings)}")
    notes.append(f"Triggers: {len(triggers)}")

    combined_symbols: Counter[str] = Counter()
    mapping_symbol_sets: dict[str, set[str]] = {}
    triplet_counts: Counter[tuple[str, ...]] = Counter()
    adjacent_pairs = 0
    consecutive_triplets = 0
    choices_per_letter: Counter[int] = Counter()

    for mapping_id, mapping in mappings.items():
        if not isinstance(mapping, dict):
            issues.append(f"{mapping_id}: mapping is not an object.")
            continue

        used_symbols: set[str] = set()
        for letter in alphabet:
            choices = mapping.get(letter)
            if not isinstance(choices, list):
                issues.append(f"{mapping_id}/{letter}: missing or invalid sub-mapping.")
                continue
            if len(choices) != 3:
                issues.append(f"{mapping_id}/{letter}: expected 3 choices, found {len(choices)}.")
            choices_per_letter[len(choices)] += 1
            if any(not isinstance(symbol, str) or len(symbol) != 1 for symbol in choices):
                issues.append(f"{mapping_id}/{letter}: choices must be one-character strings.")
                continue
            if len(set(choices)) != len(choices):
                issues.append(f"{mapping_id}/{letter}: repeated symbol inside sub-mapping.")
            repeated = used_symbols.intersection(choices)
            if repeated:
                issues.append(f"{mapping_id}: ciphertext symbols reused across letters: {sorted(repeated)!r}.")
            used_symbols.update(choices)
            combined_symbols.update(choices)
            triplet = tuple(choices)
            triplet_counts[triplet] += 1
            ordered_codes = sorted(ord(symbol) for symbol in choices)
            adjacent_pairs += sum(
                1
                for first, second in zip(ordered_codes, ordered_codes[1:])
                if second - first == 1
            )
            if len(ordered_codes) == 3 and ordered_codes[1] == ordered_codes[0] + 1 and ordered_codes[2] == ordered_codes[1] + 1:
                consecutive_triplets += 1

        mapping_symbol_sets[mapping_id] = used_symbols

    duplicated_triplets = {triplet: count for triplet, count in triplet_counts.items() if count > 1}
    notes.append(f"Choice counts per letter: {dict(sorted(choices_per_letter.items()))}")
    notes.append(f"Distinct ciphertext symbols per mapping: { {key: len(value) for key, value in mapping_symbol_sets.items()} }")
    notes.append(f"Repeated identical triplets in a mapping: {len(duplicated_triplets)}")
    notes.append(f"Adjacent ASCII pairs inside triplets: {adjacent_pairs}")
    notes.append(f"Three-consecutive-ASCII triplets: {consecutive_triplets}")

    pool = set(DEFAULT_SYMBOL_POOL)
    if combined_symbols and set(combined_symbols).issubset(pool):
        expected_per_symbol = len(alphabet) * 3 / len(pool)
        full_counts = Counter({symbol: combined_symbols.get(symbol, 0) for symbol in pool})
        observed = list(full_counts.values())
        chi_square = sum((count - expected_per_symbol) ** 2 / expected_per_symbol for count in observed)
        notes.append(
            "Printable-ASCII pool frequency: "
            f"used {sum(count > 0 for count in observed)}/{len(pool)}, "
            f"min/max {min(observed)}/{max(observed)}, "
            f"uniformity chi-square {chi_square:.2f} (descriptive only)"
        )
    else:
        notes.append("Printable-ASCII frequency check skipped: config uses symbols outside the generator's default pool.")

    if len(mapping_symbol_sets) > 1:
        overlaps = []
        mapping_ids = list(mapping_symbol_sets)
        for index, first_id in enumerate(mapping_ids):
            for second_id in mapping_ids[index + 1 :]:
                overlap = mapping_symbol_sets[first_id] & mapping_symbol_sets[second_id]
                overlaps.append((first_id, second_id, len(overlap)))
        notes.append(f"Cross-mapping symbol overlaps: {overlaps}")

    assignment_counts = Counter(
        trigger.get("mapping")
        for trigger in triggers
        if isinstance(trigger, dict) and trigger.get("mapping") is not None
    )
    notes.append(f"Trigger mapping assignments: {dict(sorted(assignment_counts.items()))}")

    seeds: list[int] = []
    global_seed = config.get("symbolShuffleSeed")
    if isinstance(global_seed, int) and not isinstance(global_seed, bool):
        seeds.append(global_seed)
    for trigger in triggers:
        if not isinstance(trigger, dict):
            issues.append("Trigger entry is not an object.")
            continue
        for key in ("mappingSeed", "subMappingSeed"):
            seed = trigger.get(key)
            if isinstance(seed, int) and not isinstance(seed, bool):
                seeds.append(seed)
    unique_seed_count = len(set(seeds))
    bit_count = sum(seed.bit_count() for seed in seeds)
    total_seed_bits = len(seeds) * 64
    bit_z_score = 0.0
    if total_seed_bits:
        bit_z_score = (bit_count - total_seed_bits / 2) / math.sqrt(total_seed_bits / 4)
    notes.append(
        f"Configured seeds: {len(seeds)}, unique: {unique_seed_count}, "
        f"one-bits: {bit_count}/{total_seed_bits}, balance z-score: {bit_z_score:.2f}"
    )
    if unique_seed_count != len(seeds):
        issues.append(f"Seed collision: {len(seeds) - unique_seed_count} duplicate seed value(s).")

    notes.append(
        "Interpretation: these are structural/sample diagnostics, not proof of cryptographic randomness. "
        "A single generated configuration is too small to establish statistical randomness."
    )
    return issues, notes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", nargs="?", type=Path, default=DEFAULT_CONFIG)
    args = parser.parse_args()

    try:
        config = load_config(args.config)
        issues, notes = analyze(config)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        parser.error(str(error))

    print(f"Config: {args.config}")
    for note in notes:
        print(f"- {note}")
    if issues:
        print("Issues:")
        for issue in issues:
            print(f"- {issue}")
        return 1
    print("Structural checks: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())