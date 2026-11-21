"""Central UI style registry for Word Lock.

Public API
----------
    import ui_styles

    ui_styles.STYLES            # tuple of 10 Style objects
    ui_styles.current()         # active Style
    ui_styles.set_current(...)  # switch by Style object or key
    ui_styles.random_style()    # random Style (optionally excluding one)
    ui_styles.get("ocean")      # lookup by key

The UI widgets read only from ``ui_styles.current()`` (via
``WordGateUI.apply_style``), so swapping the whole look is one call.
"""

import random

from .base import SIZE, Style, Typography
from .styles import STYLES

__all__ = [
    "STYLES",
    "SIZE",
    "Style",
    "Typography",
    "current",
    "set_current",
    "get",
    "random_style",
    "next_style",
]

_BY_KEY = {style.key: style for style in STYLES}
_current = STYLES[0]


def current():
    """Return the active style."""
    return _current


def get(key):
    """Look up a style by key; returns None when unknown."""
    return _BY_KEY.get(str(key or "").strip().lower())


def set_current(style):
    """Activate a Style object or a style key and return it."""
    global _current
    if isinstance(style, Style):
        _current = style
    else:
        found = get(style)
        if found is None:
            raise KeyError(f"unknown UI style: {style!r}")
        _current = found
    return _current


def random_style(exclude=None):
    """Pick a random style, optionally avoiding one Style object or key."""
    exclude_key = exclude.key if isinstance(exclude, Style) else exclude
    candidates = [style for style in STYLES if style.key != exclude_key]
    return random.choice(candidates or list(STYLES))


def next_style():
    """Activate and return the next style in registry order."""
    index = STYLES.index(_current)
    return set_current(STYLES[(index + 1) % len(STYLES)])
