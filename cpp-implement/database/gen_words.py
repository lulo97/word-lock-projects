#!/usr/bin/env python3
"""Generate missing Kotoba word entries with a local llama.cpp server.

Input:  database/words.txt          (one Japanese word per line)
Output: database/words/<word>.json  (strict JSON, see ROADMAP §2.1)

Schema (per entry):
    {
      "kanji": str,
      "hiragana": str,
      "romaji": str,
      "pos": str,
      "meaning_ja": str,
      "meaning_ja_romaji": str,
      "meaning_vi": str,
      "examples": [{"ja": str, "romaji": str, "vi": str, "audio": str}],
      "tags": [str, ...],
      "audio": str
    }

The script is a manual, offline tool. It is not imported or tested by the C++
app; the app only reads the JSON files it produces.

Usage:
    python database/gen_words.py
    python database/gen_words.py --input database/words.txt --force
    python database/gen_words.py --endpoint http://127.0.0.1:9931

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
ERROR_LOG = DATABASE_DIR / "gen_words.errors.log"

DEFAULT_ENDPOINT = "http://127.0.0.1:9931"
DEFAULT_MODEL = "local-model"

JAPANESE_RE = re.compile(
    "[\u3000-\u303f\u3040-\u309f\u30a0-\u30ff"
    "\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]"
)
INVALID_FILENAME_RE = re.compile(r'[\\/:*?"<>|\x00-\x1f]')
# Hepburn romaji must stay ASCII: "dōbutsu" is not usable by the app.
ROMAJI_ALLOWED_RE = re.compile(r"[A-Za-z0-9 .,'\-]+")
# Tags are short lowercase English descriptors: "food", "fruit", "on-yomi".
TAG_RE = re.compile(r"[a-z0-9][a-z0-9\-]*")


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
    kanji = data.get("kanji")
    romaji = data.get("romaji")
    return (
        isinstance(kanji, str) and bool(kanji.strip())
        and isinstance(romaji, str) and bool(romaji.strip())
        and not has_japanese(romaji)
    )


def load_existing() -> dict[str, Path]:
    """Map normalized kanji key -> file for every valid existing entry."""
    existing: dict[str, Path] = {}
    for path in word_files():
        try:
            data = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError):
            continue
        if not entry_is_valid(data):
            continue
        key = normalize_word(str(data.get("kanji") or ""))
        if key:
            existing.setdefault(key, path)
    return existing


def target_path_for(word: str) -> Path:
    return WORDS_DIR / f"{word}.json"


def audio_path_for_word(word: str) -> str:
    return f"audio/{word}/word.ogg"


def audio_path_for_example(word: str, index: int) -> str:
    return f"audio/{word}/ex_{index}.ogg"


def check_romaji(value: str, where: str) -> None:
    if has_japanese(value):
        raise GenerateError(f"{where} contains Japanese characters")
    if not ROMAJI_ALLOWED_RE.fullmatch(value):
        raise GenerateError(
            f"{where} must be plain ASCII Hepburn romaji "
            f"(e.g. dobutsu, not dōbutsu): {value!r}"
        )


def validate_entry(entry, word: str) -> dict:
    """Return a canonical new-schema entry or raise GenerateError."""
    if not isinstance(entry, dict):
        raise GenerateError("model output is not a JSON object")

    returned = normalize_word(str(entry.get("kanji", "")))
    if returned != word:
        raise GenerateError(
            f"returned kanji {returned!r}, expected {word!r}"
        )

    result: dict = {"kanji": word}
    for key in (
        "hiragana",
        "romaji",
        "pos",
        "meaning_ja",
        "meaning_ja_romaji",
        "meaning_vi",
    ):
        value = entry.get(key)
        if not isinstance(value, str) or not value.strip():
            raise GenerateError(f"{key} must be a non-empty string")
        result[key] = value.strip()

    check_romaji(result["romaji"], "romaji")
    check_romaji(result["meaning_ja_romaji"], "meaning_ja_romaji")

    if not result["pos"][:1].isupper():
        raise GenerateError(
            f"pos must be capitalized (e.g. Noun, Verb, I-Adjective): "
            f"{result['pos']!r}"
        )

    raw_examples = entry.get("examples")
    if not isinstance(raw_examples, list) or not raw_examples:
        raise GenerateError("examples must be a non-empty array")

    examples = []
    for index, raw in enumerate(raw_examples):
        if not isinstance(raw, dict):
            raise GenerateError(f"examples[{index}] must be an object")
        example: dict = {}
        for key in ("ja", "romaji", "vi"):
            value = raw.get(key)
            if not isinstance(value, str) or not value.strip():
                raise GenerateError(
                    f"examples[{index}].{key} must be a non-empty string"
                )
            example[key] = value.strip()
        check_romaji(example["romaji"], f"examples[{index}].romaji")
        example["audio"] = audio_path_for_example(word, index + 1)
        examples.append(example)

    raw_tags = entry.get("tags")
    if not isinstance(raw_tags, list) or not raw_tags:
        raise GenerateError("tags must be a non-empty array of strings")
    tags: list[str] = []
    for index, tag in enumerate(raw_tags):
        if not isinstance(tag, str) or not tag.strip():
            raise GenerateError(
                f"tags[{index}] must be a non-empty string"
            )
        cleaned = tag.strip()
        if not TAG_RE.fullmatch(cleaned):
            raise GenerateError(
                f"tags[{index}] must be a short lowercase English tag "
                f"(e.g. food, fruit, on-yomi): {cleaned!r}"
            )
        tags.append(cleaned)

    result["examples"] = examples
    result["tags"] = tags
    result["audio"] = audio_path_for_word(word)
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

    if args.retries < 0:
        args.retries = 0

    prompt_template = """
You generate one Japanese study-dictionary entry for a Vietnamese learner of
Japanese.

Return exactly one valid JSON object and nothing else: no markdown fences, no
comments, no explanation before or after it.

Schema:
{
  "kanji": string,
  "hiragana": string,
  "romaji": string,
  "pos": string,
  "meaning_ja": string,
  "meaning_ja_romaji": string,
  "meaning_vi": string,
  "examples": [
    {
      "ja": string,
      "romaji": string,
      "vi": string,
      "audio": string
    }
  ],
  "tags": [string, ...],
  "audio": string
}

Rules:
- "kanji" must be exactly the requested Japanese word, unchanged. If the word
  is written in kana, "kanji" is that kana form.
- "hiragana" is the full hiragana reading (katakana words are written in
  hiragana).
- "romaji" is lowercase Hepburn romaji of "hiragana". Use only ASCII letters,
  spaces, apostrophes, hyphens and periods. Never put Japanese characters,
  macrons or other accents in "romaji".
- "pos" is the part of speech, capitalized, e.g. "Noun", "Verb",
  "I-Adjective", "Na-Adjective", "Adverb", "Expression".
- "meaning_ja" is one short natural Japanese definition.
- "meaning_ja_romaji" is lowercase Hepburn romaji of "meaning_ja" (ASCII only).
- "meaning_vi" is one short natural Vietnamese translation.
- Give exactly two natural Japanese example sentences. For each example:
  "ja" is the Japanese sentence, "romaji" its lowercase Hepburn romaji
  (ASCII only), and "vi" its natural Vietnamese translation. The "audio"
  field may be an empty string; the tool fills it in.
- "tags" is a non-empty array of short lowercase English tags describing the
  word (e.g. "food", "fruit", "on-yomi").
- The top-level "audio" field may be an empty string; the tool fills it in.
- Do not add extra top-level keys or extra keys inside examples.

Japanese word: {{word}}
"""
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