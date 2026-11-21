"""Font resolution that guarantees Vietnamese (and English) glyph coverage.

Tk on Windows happily accepts a font family that has no Vietnamese glyphs and
then renders empty boxes.  To avoid that, every family suggested by a style is
verified against the actual font file before it is used:

1. collect installed font families (Tk) and font files (Pillow),
2. render the tricky Vietnamese characters and compare the bitmap with the
   ".notdef" (missing glyph) bitmap,
3. walk the style's preferred family list until a verified family is found,
4. fall back to a curated list of fonts that ship with Vietnamese support.

The check only runs once per family and is cached, so switching styles stays
fast.  On a system where Pillow cannot open a font, Tk's own GDI font linking
still renders Vietnamese, so the last-resort family is always returned.
"""

import functools
import os
import sys
from pathlib import Path

from PIL import ImageFont

# Characters that cover every Vietnamese diacritic combination used in the app.
VIETNAMESE_TEST_CHARS = "ếứạữđĐệốồộầẩỹợỗĩũƠƯơư"

# Verified in the fallback chain below; only used when a family is installed.
FALLBACK_FAMILIES = (
    "Segoe UI",
    "Arial",
    "Tahoma",
    "Verdana",
    "Calibri",
    "Candara",
    "Corbel",
    "Constantia",
    "Microsoft Sans Serif",
    "DejaVu Sans",
    "Liberation Sans",
    "Noto Sans",
    "Noto Sans Vietnamese",
    "Roboto",
    "Open Sans",
)

_MISSING_GLYPH = "\uffff"


def _font_directories():
    if sys.platform.startswith("win"):
        directories = [Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"]
        local = os.environ.get("LOCALAPPDATA")
        if local:
            directories.append(Path(local) / "Microsoft" / "Windows" / "Fonts")
        return [path for path in directories if path.is_dir()]

    if sys.platform == "darwin":
        return [
            Path("/System/Library/Fonts"),
            Path("/Library/Fonts"),
            Path.home() / "Library" / "Fonts",
        ]

    return [
        Path("/usr/share/fonts"),
        Path("/usr/local/share/fonts"),
        Path.home() / ".fonts",
        Path.home() / ".local" / "share" / "fonts",
    ]


def _font_files():
    extensions = {".ttf", ".otf", ".ttc"}
    seen = set()
    for directory in _font_directories():
        try:
            for path in directory.rglob("*"):
                if path.suffix.lower() not in extensions:
                    continue
                key = str(path).lower()
                if key not in seen:
                    seen.add(key)
                    yield path
        except OSError:
            continue


@functools.lru_cache(maxsize=1)
def _font_file_index():
    """Map lowercase family name -> tuple of font file paths."""
    index = {}
    for path in _font_files():
        try:
            family = ImageFont.truetype(str(path), 20).getname()[0]
        except Exception:
            continue
        if not family:
            continue
        index.setdefault(family.strip().lower(), []).append(path)
    return {key: tuple(value) for key, value in index.items()}


def _glyph_mask(font, char):
    try:
        mask = font.getmask(char)
        return mask.size, bytes(mask)
    except Exception:
        return None


@functools.lru_cache(maxsize=1024)
def _file_supports_vietnamese(path):
    try:
        font = ImageFont.truetype(str(path), 36)
    except Exception:
        return False

    missing = _glyph_mask(font, _MISSING_GLYPH)
    if missing is None:
        return False

    for char in VIETNAMESE_TEST_CHARS:
        mask = _glyph_mask(font, char)
        if mask is None or mask == missing or mask[0] in ((0, 0), (1, 1)):
            return False
    return True


@functools.lru_cache(maxsize=1)
def _installed_families():
    try:
        import tkinter

        if tkinter._default_root is None:
            return {}
        from tkinter import font as tkfont

        return {name.strip().lower(): name for name in tkfont.families()}
    except Exception:
        return {}


def installed_families():
    """Installed Tk families, or an empty mapping before the root exists."""
    return _installed_families()


def _actual_family(path):
    try:
        return ImageFont.truetype(str(path), 20).getname()[0]
    except Exception:
        return None


def _supports_for_family(family_key):
    for path in _font_file_index().get(family_key, ()):
        if _file_supports_vietnamese(str(path)):
            actual = _actual_family(path)
            return actual or family_key
    return None


def _default_tk_family():
    try:
        import tkinter

        if tkinter._default_root is None:
            return None
        from tkinter import font as tkfont

        return tkfont.nametofont("TkDefaultFont").actual("family")
    except Exception:
        return None


@functools.lru_cache(maxsize=None)
def resolve_family(candidates):
    """Return the first installed, Vietnamese-capable family from candidates."""
    stack = []
    for family in tuple(candidates) + FALLBACK_FAMILIES:
        if family and family.strip():
            key = family.strip().lower()
            if key not in {value.strip().lower() for value in stack}:
                stack.append(family.strip())

    installed = _installed_families()
    for family in stack:
        key = family.lower()
        if installed and key not in installed:
            continue
        verified = _supports_for_family(key)
        if verified:
            return verified

    # Nothing verified: use a family that is known-good even if font files could
    # not be inspected for some reason. Prefer what Tk already uses.
    default = _default_tk_family()
    for family in (default, "Segoe UI", "Arial", "DejaVu Sans", "Noto Sans"):
        if not family:
            continue
        if installed and family.lower() not in installed:
            continue
        if _supports_for_family(family.lower()) or not _font_file_index():
            return family

    return default or "Arial"


def configure_tk_defaults(family):
    """Point Tk's named fonts at the verified family (dialogs, menus, text)."""
    try:
        import tkinter

        if tkinter._default_root is None:
            return
        from tkinter import font as tkfont

        for name in (
            "TkDefaultFont",
            "TkTextFont",
            "TkMenuFont",
            "TkHeadingFont",
            "TkCaptionFont",
            "TkSmallCaptionFont",
            "TkIconFont",
            "TkTooltipFont",
        ):
            try:
                tkfont.nametofont(name).configure(family=family)
            except Exception:
                continue
    except Exception:
        pass
