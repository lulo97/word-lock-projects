"""Persist per-word learning progress in %USERPROFILE%/wordlock.

Each time the user types a word correctly, its attempt counter is incremented.
The counters are used to pick the least-practiced word next, so words are
learned evenly instead of being chosen at random.
"""

import json
import os
import threading
from pathlib import Path

PROGRESS_DIR = Path(os.environ.get("USERPROFILE") or Path.home()) / "wordlock"
PROGRESS_FILE = PROGRESS_DIR / "progress.json"

_lock = threading.Lock()


def word_key(word):
    """Normalize a word so counters match regardless of case/whitespace."""
    return str(word or "").strip().lower()


def _read():
    """Load the raw progress document, returning an empty one when missing."""
    try:
        with open(PROGRESS_FILE, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return {"version": 1, "attempts": {}}

    if not isinstance(data, dict):
        return {"version": 1, "attempts": {}}

    attempts = data.get("attempts")
    if not isinstance(attempts, dict):
        attempts = {}

    # Keep only well-formed counters so a corrupted file cannot break the app.
    cleaned = {}
    for word, count in attempts.items():
        if isinstance(word, str) and isinstance(count, int) and count >= 0:
            cleaned[word] = count

    return {"version": 1, "attempts": cleaned}


def _write(data):
    """Write atomically so an interrupted save cannot corrupt progress."""
    PROGRESS_DIR.mkdir(parents=True, exist_ok=True)
    temp = PROGRESS_FILE.with_suffix(".json.tmp")
    with open(temp, "w", encoding="utf-8") as handle:
        json.dump(data, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    os.replace(temp, PROGRESS_FILE)


def load_attempts():
    """Return a copy of the word -> correct-answer count mapping."""
    with _lock:
        return dict(_read()["attempts"])


def record_success(word):
    """Increment the correct-answer counter for a word and return the new value."""
    key = word_key(word)
    if not key:
        return 0

    with _lock:
        data = _read()
        count = data["attempts"].get(key, 0) + 1
        data["attempts"][key] = count
        try:
            _write(data)
        except OSError:
            # Progress is a nice-to-have; never block the word gate on disk errors.
            pass
        return count


def attempt_count(word, attempts=None):
    """Return how many times the user has answered this word correctly."""
    if attempts is None:
        attempts = load_attempts()
    return attempts.get(word_key(word), 0)
