import json
import random
from pathlib import Path

from config import DATABASE_DIR

FALLBACK_ENTRY = {
    "word": "no data",
    "language": {"source": "en", "target": "vi"},
    "pronunciation": {"ipa": "", "audio": ""},
    "part_of_speech": "",
    "definitions": [],
    "examples": [],
    "synonyms": [],
    "antonyms": [],
    "tags": [],
}


def word_key(entry):
    """Stable counter key for an entry: its normalized word."""
    return str(entry.get("word", "")).strip().lower()


def iter_word_files():
    """Word files live in one folder per first letter, e.g. database/a/apple.json."""
    return sorted(DATABASE_DIR.rglob("*.json"))


def _load_file(path):
    try:
        with open(path, encoding="utf-8") as f:
            entry = json.load(f)
    except (OSError, ValueError):
        return None
    return entry if isinstance(entry, dict) else None


def load_entries(ignore_words=None):
    """Load every valid word entry, optionally skipping ignored words."""
    ignored = set(ignore_words or ())
    entries = []
    for path in iter_word_files():
        entry = _load_file(path)
        if entry is not None and word_key(entry) not in ignored:
            entries.append(entry)
    return entries


def load_next_entry(attempts=None, ignore_words=None):
    """Pick the least-practiced word, breaking ties randomly.

    Words the user has answered correctly fewer times are shown first, so every
    word gets a turn before any word is repeated. Ignored words are excluded.
    """
    attempts = attempts or {}
    entries = load_entries(ignore_words)
    if not entries:
        return dict(FALLBACK_ENTRY)

    lowest = min(attempts.get(word_key(entry), 0) for entry in entries)
    candidates = [
        entry for entry in entries
        if attempts.get(word_key(entry), 0) == lowest
    ]
    return random.choice(candidates)


def load_random_entry():
    """Load one random word file. Kept for compatibility."""
    entries = load_entries()
    if not entries:
        return dict(FALLBACK_ENTRY)
    return random.choice(entries)


def _resolve_audio_value(value):
    """Resolve a database-relative audio path to an existing file, else None."""
    value = str(value or "").strip()
    if not value:
        return None

    path = Path(value)
    if not path.is_absolute():
        path = DATABASE_DIR / path
    try:
        return path if path.is_file() else None
    except OSError:
        return None


def resolve_audio(entry):
    """Return the existing pronunciation audio file for an entry, or None."""
    pronunciation = entry.get("pronunciation")
    if not isinstance(pronunciation, dict):
        return None
    return _resolve_audio_value(pronunciation.get("audio", ""))


def resolve_example_audio(example):
    """Return the existing audio file for an example sentence, or None."""
    if not isinstance(example, dict):
        return None
    return _resolve_audio_value(example.get("audio", ""))


def format_entry(entry):
    """Render a dictionary entry as readable text for the info screen."""
    lines = [entry.get("word", "")]

    pos = entry.get("part_of_speech", "")
    ipa = entry.get("pronunciation", {}).get("ipa", "")
    header = "  ".join(x for x in (pos, ipa) if x)
    if header:
        lines.append(header)

    lines.append("")
    for definition in entry.get("definitions", []):
        lines.append("EN: " + definition.get("english", ""))
        lines.append("VI: " + definition.get("vietnamese", ""))

    examples = entry.get("examples", [])
    if examples:
        lines.append("")
        lines.append("Examples:")
        for example in examples:
            lines.append("• " + example.get("english", ""))
            lines.append("  " + example.get("vietnamese", ""))

    for label, key in (("Synonyms", "synonyms"), ("Antonyms", "antonyms"), ("Tags", "tags")):
        values = entry.get(key, [])
        if values:
            lines.append("")
            lines.append(f"{label}: " + ", ".join(values))

    return "\n".join(lines)
