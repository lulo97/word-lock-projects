"""User-editable settings stored next to progress.json.

File: %USERPROFILE%/wordlock/config.json

    {
      "AUTO_SHOW_DELAY_MS": 60000,
      "IGNORE_WORDS": ["dog", "cat"],
      "SOUND_VOLUME": 2.0,
      "START_WITH_WINDOWS": false
    }

The file is created with defaults on first run. Invalid or missing values fall
back to the built-in defaults, so a typo never stops the app from starting.
"""

import json
import math
import os

import config
from progress import PROGRESS_DIR

CONFIG_FILE = PROGRESS_DIR / "config.json"

IGNORE_KEYS = ("IGNORE_WORDS", "ignore_words", "ignored_words")

MIN_SOUND_VOLUME = 0.0
MAX_SOUND_VOLUME = 4.0


def _defaults():
    return {
        "AUTO_SHOW_DELAY_MS": config.AUTO_SHOW_DELAY_MS,
        "IGNORE_WORDS": [],
        "SOUND_VOLUME": config.SOUND_VOLUME,
        "START_WITH_WINDOWS": False,
    }


def _write(data):
    PROGRESS_DIR.mkdir(parents=True, exist_ok=True)
    temp = CONFIG_FILE.with_suffix(".json.tmp")
    with open(temp, "w", encoding="utf-8") as handle:
        json.dump(data, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    os.replace(temp, CONFIG_FILE)


def _ensure_file():
    if CONFIG_FILE.exists():
        return
    try:
        _write(_defaults())
    except OSError:
        pass


def load():
    """Read config.json and return a validated settings dict."""
    _ensure_file()
    data = {}
    try:
        with open(CONFIG_FILE, encoding="utf-8") as handle:
            parsed = json.load(handle)
        if isinstance(parsed, dict):
            data = parsed
    except (OSError, ValueError):
        pass

    return {
        "AUTO_SHOW_DELAY_MS": data.get("AUTO_SHOW_DELAY_MS"),
        "IGNORE_WORDS": _read_ignore_words(data),
        "SOUND_VOLUME": data.get("SOUND_VOLUME"),
        "START_WITH_WINDOWS": _read_bool(data.get("START_WITH_WINDOWS")),
    }


def _read_bool(value):
    """Accept real JSON booleans and common string/number spellings."""
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on"}
    return False


def _read_ignore_words(data):
    for key in IGNORE_KEYS:
        values = data.get(key)
        if isinstance(values, list):
            return [
                value.strip().lower()
                for value in values
                if isinstance(value, str) and value.strip()
            ]
    return []


def auto_show_delay_ms():
    """AUTO_SHOW_DELAY_MS from config.json, or the app default."""
    value = load()["AUTO_SHOW_DELAY_MS"]
    if isinstance(value, bool):
        return config.AUTO_SHOW_DELAY_MS
    if isinstance(value, str):
        try:
            value = float(value.strip())
        except ValueError:
            return config.AUTO_SHOW_DELAY_MS
    if isinstance(value, (int, float)) and value > 0:
        return int(value)
    return config.AUTO_SHOW_DELAY_MS


def ignored_words():
    """Set of normalized words that should never be shown."""
    return set(load()["IGNORE_WORDS"])


def _clamp_volume(value):
    try:
        volume = float(value)
    except (TypeError, ValueError):
        return config.SOUND_VOLUME
    if not math.isfinite(volume):
        return config.SOUND_VOLUME
    return min(MAX_SOUND_VOLUME, max(MIN_SOUND_VOLUME, volume))


def sound_volume():
    """Playback gain from config.json (1.0 = Windows master volume)."""
    return _clamp_volume(load()["SOUND_VOLUME"])


def start_with_windows():
    """True when the user asked Word Lock to start with Windows."""
    return load()["START_WITH_WINDOWS"]


def add_ignored_word(word):
    """Append one word to IGNORE_WORDS in config.json.

    Preserves every other setting. Raises OSError when the file cannot be
    written; returns True when the word is in the list afterwards.
    """
    key = str(word).strip().lower()
    if not key:
        return True

    data = load()
    words = list(data["IGNORE_WORDS"])
    if key in words:
        return True

    words.append(key)
    save_config(
        auto_show_delay_ms(),
        words,
        _clamp_volume(data["SOUND_VOLUME"]),
        data["START_WITH_WINDOWS"],
    )
    return True


def save_config(auto_show_delay_ms, ignore_words, sound_volume=None,
                start_with_windows=None):
    """Write settings from the Settings dialog, preserving unknown keys."""
    _ensure_file()

    data = {}
    try:
        with open(CONFIG_FILE, encoding="utf-8") as handle:
            parsed = json.load(handle)
        if isinstance(parsed, dict):
            data = parsed
    except (OSError, ValueError):
        pass

    for key in IGNORE_KEYS:
        data.pop(key, None)

    words = []
    seen = set()
    for word in ignore_words:
        key = str(word).strip().lower()
        if key and key not in seen:
            seen.add(key)
            words.append(key)

    data["AUTO_SHOW_DELAY_MS"] = int(auto_show_delay_ms)
    data["IGNORE_WORDS"] = words
    data["SOUND_VOLUME"] = round(
        _clamp_volume(config.SOUND_VOLUME if sound_volume is None else sound_volume), 1
    )
    if start_with_windows is None:
        data["START_WITH_WINDOWS"] = _read_bool(data.get("START_WITH_WINDOWS"))
    else:
        data["START_WITH_WINDOWS"] = bool(start_with_windows)
    _write(data)
    return data
