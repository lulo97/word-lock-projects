#!/usr/bin/env python3
"""Validate the Kotoba word database.

Checks every database/words/**/*.json:
  * required fields and types (new generator schema and the legacy schema)
  * romaji contains no Japanese characters
  * examples are well formed
  * no duplicate words, and the filename matches the word
  * every non-empty audio path exists, is large enough and has a WAV/OGG header

Usage:
    python tools/validate_db.py
    python tools/validate_db.py --verbose

Exit code: 0 = all files valid, 1 = at least one invalid file.
A per-word report is written to database/validate_db.errors.log.

This is a manual tool; the C++ app does its own light check at startup
(see utils/word_db.cpp) and never refuses to start because of a bad file.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
import unicodedata
from pathlib import Path

# Japanese filenames/words crash a cp1252 console without this.
for stream in (sys.stdout, sys.stderr):
    try:
        stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

ROOT = Path(__file__).resolve().parent.parent
DATABASE_DIR = ROOT / "database"
WORDS_DIR = DATABASE_DIR / "words"
LOG_PATH = DATABASE_DIR / "validate_db.errors.log"

MIN_AUDIO_BYTES = 512

NEW_REQUIRED = ("word", "reading", "romaji", "meaning", "type", "examples")
LEGACY_REQUIRED = ("kanji", "hiragana", "romaji")
NEW_EXAMPLE_REQUIRED = ("jp", "reading", "romaji", "en")
LEGACY_EXAMPLE_REQUIRED = ("ja", "romaji", "vi")

JAPANESE_RE = re.compile(
    "[\u3000-\u303f\u3040-\u309f\u30a0-\u30ff"
    "\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]"
)


def normalize(value: str) -> str:
    return unicodedata.normalize("NFC", str(value)).strip()


def has_japanese(value: str) -> bool:
    return bool(JAPANESE_RE.search(value))


def word_files() -> list[Path]:
    if not WORDS_DIR.is_dir():
        return []
    return sorted(WORDS_DIR.rglob("*.json"))


def check_string(data: dict, key: str, where: str, errors: list[str],
                 *, allow_empty: bool = False) -> None:
    value = data.get(key)
    if not isinstance(value, str):
        errors.append(f"{where}.{key}: expected string, got {type(value).__name__}")
        return
    if not allow_empty and not value.strip():
        errors.append(f"{where}.{key}: must not be empty")


def check_romaji(value, where: str, errors: list[str]) -> None:
    if not isinstance(value, str):
        errors.append(f"{where}: expected string, got {type(value).__name__}")
        return
    if not value.strip():
        errors.append(f"{where}: must not be empty")
    elif has_japanese(value):
        errors.append(f"{where}: contains Japanese characters ({value!r})")


def check_audio(value, where: str, errors: list[str]) -> None:
    if value is None:
        return
    if not isinstance(value, str):
        errors.append(f"{where}: expected string, got {type(value).__name__}")
        return
    if not value.strip():
        return
    path = Path(value.strip())
    if not path.is_absolute():
        path = DATABASE_DIR / path
    if not path.is_file():
        errors.append(f"{where}: audio file not found: {value}")
        return
    size = path.stat().st_size
    if size <= MIN_AUDIO_BYTES:
        errors.append(f"{where}: audio file too small ({size} bytes): {value}")
        return
    try:
        header = path.read_bytes()[:12]
    except OSError as error:
        errors.append(f"{where}: cannot read audio file: {error}")
        return
    is_wav = header[:4] == b"RIFF" and header[8:12] == b"WAVE"
    is_ogg = header[:4] == b"OggS"
    if not (is_wav or is_ogg):
        errors.append(f"{where}: not a WAV/OGG file: {value}")


def validate_entry(data, file_path: Path) -> tuple[list[str], list[str], str]:
    """Validate one parsed entry. Returns (errors, warnings, word key)."""
    errors: list[str] = []
    warnings: list[str] = []

    if not isinstance(data, dict):
        return [f"$: top-level must be an object, got {type(data).__name__}"], [], ""

    is_new = isinstance(data.get("word"), str) and bool(data["word"].strip())
    schema = NEW_REQUIRED if is_new else LEGACY_REQUIRED
    for key in schema:
        if key not in data:
            errors.append(f"$.{key}: missing required key")

    word = normalize(data.get("word") or data.get("kanji") or "")
    if not word:
        errors.append("$.word/$.kanji: word key must not be empty")
    elif not has_japanese(word):
        errors.append(f"$.word: no Japanese characters in {word!r}")

    if word:
        if normalize(file_path.stem) != word:
            errors.append(
                f"$: filename {file_path.stem!r} does not match word {word!r}"
            )

    check_string(data, "romaji", "$", errors)
    if isinstance(data.get("romaji"), str) and has_japanese(data["romaji"]):
        errors.append(f"$.romaji: contains Japanese characters ({data['romaji']!r})")

    if is_new:
        for key in ("reading", "meaning", "type"):
            check_string(data, key, "$", errors)
    else:
        for key in ("hiragana", "romaji"):
            check_string(data, key, "$", errors)
        if "meaning_ja" in data:
            check_string(data, "meaning_ja", "$", errors, allow_empty=True)

    examples = data.get("examples")
    if not isinstance(examples, list):
        errors.append(
            f"$.examples: expected list, got {type(examples).__name__}"
        )
    else:
        if not examples:
            warnings.append("$.examples: list is empty")
        required = NEW_EXAMPLE_REQUIRED if is_new else LEGACY_EXAMPLE_REQUIRED
        for index, example in enumerate(examples):
            where = f"$.examples[{index}]"
            if not isinstance(example, dict):
                errors.append(f"{where}: expected object, got {type(example).__name__}")
                continue
            for key in required:
                if key not in example:
                    errors.append(f"{where}.{key}: missing required key")
                    continue
                if key == "romaji":
                    check_romaji(example[key], f"{where}.{key}", errors)
                else:
                    check_string(example, key, where, errors)
            if "audio" in example:
                check_audio(example["audio"], f"{where}.audio", errors)

    check_audio(data.get("audio"), "$.audio", errors)

    return errors, warnings, word


def read_entry(path: Path) -> tuple[list[str], list[str], str]:
    try:
        text = path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeDecodeError) as error:
        return [f"$: cannot read file: {error}"], [], ""
    try:
        data = json.loads(text)
    except json.JSONDecodeError as error:
        return [f"$: invalid JSON: {error}"], [], ""
    return validate_entry(data, path)


def timestamp() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate the Kotoba word database.")
    parser.add_argument("--verbose", action="store_true",
                        help="print warnings as well as errors")
    args = parser.parse_args(argv)

    files = word_files()
    started = timestamp()
    lines = [
        f"[{started}] Kotoba database validation",
        f"Directory: {WORDS_DIR}",
        f"Files found: {len(files)}",
        "-" * 72,
    ]
    print(lines[0])
    print(lines[1])
    print(lines[2])

    seen: dict[str, Path] = {}
    invalid = 0
    warning_count = 0

    for path in files:
        errors, warnings, word = read_entry(path)
        if word:
            previous = seen.get(word)
            if previous is not None:
                errors.append(
                    f"$: duplicate word {word!r} (also in {previous.name})"
                )
            else:
                seen[word] = path

        warning_count += len(warnings)
        name = path.relative_to(DATABASE_DIR).as_posix()
        if errors:
            invalid += 1
            message = f"INVALID {name} ({len(errors)} error(s))"
            print(message)
            lines.append(message)
            for error in errors:
                detail = f"  ERROR {name}: {error}"
                print(detail)
                lines.append(detail)
        else:
            message = f"OK      {name}"
            print(message)
            lines.append(message)
        if args.verbose:
            for warning in warnings:
                detail = f"  WARN  {name}: {warning}"
                print(detail)
                lines.append(detail)

    summary = (
        f"{len(files)} file(s) checked: "
        f"{len(files) - invalid} valid, {invalid} invalid, "
        f"{warning_count} warning(s)"
    )
    print("-" * 72)
    print(summary)
    lines.extend(["-" * 72, summary])

    try:
        LOG_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"Report written to {LOG_PATH.relative_to(ROOT).as_posix()}")
    except OSError as error:
        print(f"Could not write log file: {error}")

    return 1 if invalid else 0


if __name__ == "__main__":
    sys.exit(main())
