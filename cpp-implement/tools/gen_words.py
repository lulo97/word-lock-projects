#!/usr/bin/env python3
"""Generate missing Kotoba word entries with a local llama.cpp server.

Input:  database/words.txt          (one Japanese word per line)
Output: database/words/<word>.json  (strict JSON, see ROADMAP §2.1)

The script is a manual, offline tool. It is not imported or tested by the C++
app; the app only reads the JSON files it produces.

Usage:
    python tools/gen_words.py
    python tools/gen_words.py --input database/words.txt --force
    python tools/gen_words.py --endpoint http://127.0.0.1:9931

Existing files that pass validation are skipped. Invalid files are
regenerated. Failed words are appended to database/gen_words.errors.log.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
import unicodedata
from pathlib import Path

from llm_client import LlmError, extract_json, request_chat

# Japanese filenames/words crash a cp1252 console without this.
for stream in (sys.stdout, sys.stderr):
    try:
        stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

ROOT = Path(__file__).resolve().parent.parent
DATABASE_DIR = ROOT / "database"
WORDS_DIR = DATABASE_DIR / "words"
DEFAULT_INPUT = DATABASE_DIR / "words.txt"
PROMPT_PATH = Path(__file__).resolve().parent / "prompts" / "word.txt"
ERROR_LOG = DATABASE_DIR / "gen_words.errors.log"

DEFAULT_ENDPOINT = "http://127.0.0.1:9931"
DEFAULT_MODEL = "local-model"

JAPANESE_RE = re.compile(
    "[\u3000-\u303f\u3040-\u309f\u30a0-\u30ff"
    "\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]"
)
INVALID_FILENAME_RE = re.compile(r'[\\/:*?"<>|\x00-\x1f]')


class GenerateError(Exception):
    """The model returned something that is not a usable word entry."""


def normalize_word(value: str) -> str:
    return unicodedata.normalize("NFC", str(value)).strip()


def has_japanese(value: str) -> bool:
    return bool(JAPANESE_RE.search(value))


def is_valid_word(word: str) -> bool:
    return bool(word) and not INVALID_FILENAME_RE.search(word) and has_japanese(word)


def read_words(path: Path) -> list[str]:
    """Read, normalize and de-duplicate words, preserving file order."""
    text = path.read_text(encoding="utf-8-sig")
    words: list[str] = []
    seen: set[str] = set()
    for line_number, line in enumerate(text.splitlines(), start=1):
        word = normalize_word(line)
        if not word or word.startswith("#"):
            continue
        if not is_valid_word(word):
            print(f"  SKIP line {line_number}: invalid Japanese word {line.strip()!r}")
            continue
        if word not in seen:
            seen.add(word)
            words.append(word)
    return words


def word_files() -> list[Path]:
    """Every word JSON in database/words (flat now, nested layout also works)."""
    if not WORDS_DIR.is_dir():
        return []
    return sorted(WORDS_DIR.rglob("*.json"))


def entry_is_valid(data) -> bool:
    """Light validity gate for the skip check (full check: validate_db.py)."""
    if not isinstance(data, dict):
        return False
    key = data.get("word") or data.get("kanji")
    romaji = data.get("romaji")
    return (
        isinstance(key, str) and bool(key.strip())
        and isinstance(romaji, str) and bool(romaji.strip())
        and not has_japanese(romaji)
    )


def load_existing() -> dict[str, Path]:
    """Map normalized word key -> file for every valid existing entry."""
    existing: dict[str, Path] = {}
    for path in word_files():
        try:
            data = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError):
            continue
        if not entry_is_valid(data):
            continue
        key = normalize_word(str(data.get("word") or data.get("kanji") or ""))
        if key:
            existing.setdefault(key, path)
    return existing


def target_path_for(word: str) -> Path:
    return WORDS_DIR / f"{word}.json"


def validate_entry(entry, word: str) -> dict:
    """Return a canonical new-schema entry or raise GenerateError."""
    if not isinstance(entry, dict):
        raise GenerateError("model output is not a JSON object")

    returned = normalize_word(str(entry.get("word", "")))
    if returned != word:
        raise GenerateError(
            f"returned word {returned!r}, expected {word!r}"
        )

    result = {"word": word}
    for key in ("reading", "romaji", "meaning", "type"):
        value = entry.get(key)
        if not isinstance(value, str) or not value.strip():
            raise GenerateError(f"{key} must be a non-empty string")
        result[key] = value.strip()

    if has_japanese(result["romaji"]):
        raise GenerateError("romaji contains Japanese characters")

    raw_examples = entry.get("examples")
    if not isinstance(raw_examples, list) or not raw_examples:
        raise GenerateError("examples must be a non-empty array")

    examples = []
    for index, raw in enumerate(raw_examples):
        if not isinstance(raw, dict):
            raise GenerateError(f"examples[{index}] must be an object")
        example = {}
        for key in ("jp", "reading", "romaji", "en"):
            value = raw.get(key)
            if not isinstance(value, str) or not value.strip():
                raise GenerateError(
                    f"examples[{index}].{key} must be a non-empty string"
                )
            example[key] = value.strip()
        if has_japanese(example["romaji"]):
            raise GenerateError(f"examples[{index}].romaji contains Japanese")
        example["audio"] = ""
        examples.append(example)

    result["audio"] = ""
    result["examples"] = examples
    return result


def build_messages(prompt: str, word: str, previous_error: str | None):
    messages = [
        {"role": "system", "content": prompt},
        {
            "role": "user",
            "content": f"Generate the JSON for the Japanese word: {word}",
        },
    ]
    if previous_error:
        messages.append(
            {
                "role": "user",
                "content": (
                    f"The previous reply was invalid: {previous_error}. "
                    "Return only corrected JSON for the word."
                ),
            }
        )
    return messages


def generate_entry(word: str, prompt: str, args) -> dict:
    last_error = ""
    for attempt in range(1, args.retries + 2):
        if attempt > 1:
            print(f"  Retry {attempt - 1}/{args.retries} for {word!r}...")
        try:
            content = request_chat(
                args.endpoint,
                args.model,
                build_messages(prompt, word, last_error or None),
                timeout=args.timeout,
                max_tokens=args.max_tokens,
            )
            return validate_entry(extract_json(content), word)
        except (LlmError, GenerateError) as error:
            last_error = str(error)
            print(f"  invalid response: {last_error}")
    raise GenerateError(last_error or "generation failed")


def save_entry(entry: dict, path: Path) -> None:
    """Atomic write: <file>.tmp then replace, so a crash cannot truncate."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp")
    temp.write_text(
        json.dumps(entry, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    os.replace(temp, path)


def log_failure(word: str, message: str) -> None:
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with ERROR_LOG.open("a", encoding="utf-8") as log:
        log.write(f"[{stamp}] {word}: {message}\n")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate missing Japanese word JSON entries with llama.cpp."
    )
    parser.add_argument(
        "--input", type=Path, default=DEFAULT_INPUT,
        help="one Japanese word per line (default: database/words.txt)",
    )
    parser.add_argument(
        "--endpoint", default=DEFAULT_ENDPOINT,
        help=f"llama.cpp base URL (default: {DEFAULT_ENDPOINT})",
    )
    parser.add_argument("--model", default=DEFAULT_MODEL,
                        help=f"model name (default: {DEFAULT_MODEL})")
    parser.add_argument("--timeout", type=float, default=300.0,
                        help="request timeout in seconds (default: 300)")
    parser.add_argument("--retries", type=int, default=3,
                        help="extra attempts after an invalid reply (default: 3)")
    parser.add_argument("--max-tokens", type=int, default=2048,
                        help="maximum tokens per reply (default: 2048)")
    parser.add_argument("--delay", type=float, default=0.0,
                        help="seconds between generated words (default: 0)")
    parser.add_argument("--force", action="store_true",
                        help="regenerate words that already have a valid JSON")
    return parser.parse_args(argv)


def run(args: argparse.Namespace) -> int:
    input_path = args.input
    if not input_path.is_file():
        print(f"ERROR: input file not found: {input_path}")
        return 2
    if not PROMPT_PATH.is_file():
        print(f"ERROR: prompt template not found: {PROMPT_PATH}")
        return 2

    if args.retries < 0:
        args.retries = 0

    prompt_template = PROMPT_PATH.read_text(encoding="utf-8")
    words = read_words(input_path)
    existing = load_existing()

    print(f"Database:    {DATABASE_DIR}")
    print(f"Words file:  {input_path}")
    print(f"Words found: {len(words)} | already in database: {len(existing)}")
    print("-" * 72)

    generated = skipped = 0
    failed: list[tuple[str, str]] = []

    for index, word in enumerate(words, start=1):
        prefix = f"[{index}/{len(words)}] {word}"
        target = target_path_for(word)

        if not args.force and existing.get(word) is not None:
            print(f"{prefix}: already exists, skipping")
            skipped += 1
            continue

        print(f"{prefix}: generating...")
        prompt = prompt_template.replace("{{word}}", word)
        try:
            entry = generate_entry(word, prompt, args)
            save_entry(entry, target)
        except GenerateError as error:
            print(f"{prefix}: FAILED - {error}")
            log_failure(word, str(error))
            failed.append((word, str(error)))
            continue

        existing[word] = target
        generated += 1
        print(f"{prefix}: saved {target.relative_to(ROOT).as_posix()}")
        if args.delay > 0 and index < len(words):
            time.sleep(args.delay)

    print("-" * 72)
    print(
        f"Done: {generated} generated, {skipped} skipped, {len(failed)} failed"
    )
    if failed:
        print("Failed words:")
        for word, message in failed:
            print(f"  - {word}: {message}")
        print(f"Details: {ERROR_LOG.relative_to(ROOT).as_posix()}")
        return 1
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        return run(args)
    except KeyboardInterrupt:
        print("\nInterrupted.")
        return 130


if __name__ == "__main__":
    sys.exit(main())
