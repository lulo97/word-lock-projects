"""Core style model shared by every UI style in this folder.

A style is a plain, frozen dataclass: colours, fonts, borders and a few
typography switches.  The UI reads everything from the active style, so
switching looks means swapping one object (see :func:`ui_styles.set_current`).

Adding style #11:

    # ui_styles/styles/aurora.py
    from ..base import Style, Typography
    STYLE = Style(key="aurora", name="Aurora", ...)

    # then import it in ui_styles/__init__.py and add it to STYLES.
"""

from dataclasses import dataclass, field
from typing import Optional, Tuple

from . import fonts


@dataclass(frozen=True)
class Typography:
    """Family preferences plus a global size multiplier.

    ``display`` is used for the big word headings; it falls back to ``body``.
    Families are only accepted after the Vietnamese glyph check in fonts.py.
    """

    body: Tuple[str, ...] = ("Segoe UI", "Arial")
    display: Tuple[str, ...] = ()
    scale: float = 1.0
    uppercase_headings: bool = True


@dataclass(frozen=True)
class Style:
    # identity
    key: str
    name: str
    description: str = ""

    # window / theme
    appearance: str = "dark"                     # "dark" or "light"
    color_theme: str = "blue"                    # customtkinter theme
    gradient: bool = True
    background_top: str = "#16233f"
    background_bottom: str = "#04060c"
    background: Optional[str] = None             # solid fallback, default bottom

    # surfaces
    card: str = "#0b1324"
    card_border: str = "#243354"
    chip_bg: str = "#16233d"
    chip_hover: str = "#1d2f50"
    entry_bg: str = "#0f1a30"

    # text
    text_primary: str = "#f8fafc"
    text_body: str = "#dbe4f3"
    text_muted: str = "#93a4c3"

    # accents / feedback
    accent: str = "#60a5fa"
    accent_soft: str = "#7dd3fc"
    accent_hover: str = "#3b82f6"
    on_accent: str = "#07101f"
    error: str = "#f87171"
    success: str = "#4ade80"

    # explicit button colours (fall back to chip / accent when omitted)
    button_bg: Optional[str] = None
    button_hover: Optional[str] = None
    button_border: Optional[str] = None
    button_text: Optional[str] = None

    primary_bg: Optional[str] = None
    primary_hover: Optional[str] = None
    primary_text: Optional[str] = None

    # shape
    border_width: int = 1

    # countdown card
    overlay_alpha: float = 0.9

    typography: Typography = field(default_factory=Typography)

    # ---------- derived values ----------

    @property
    def background_color(self):
        return self.background or self.background_bottom

    @property
    def input_border_width(self):
        # CustomTkinter's inner Entry/Textbox is laid out one pixel over the
        # bottom border when the widget scaling is fractional (e.g. 1.25x).
        # A 1px border therefore disappears at the bottom, so inputs always
        # get at least 2px.
        return max(3, self.border_width)

    @property
    def secondary_button_bg(self):
        return self.button_bg or self.chip_bg

    @property
    def secondary_button_hover(self):
        return self.button_hover or self.chip_hover

    @property
    def secondary_button_border(self):
        return self.button_border or self.card_border

    @property
    def secondary_button_text(self):
        return self.button_text or self.accent_soft

    @property
    def primary_button_bg(self):
        return self.primary_bg or self.accent

    @property
    def primary_button_hover(self):
        return self.primary_hover or self.accent_hover

    @property
    def primary_button_text(self):
        return self.primary_text or self.on_accent

    def heading(self, text):
        """Apply the style's heading casing (e.g. terminal styles uppercase)."""
        text = str(text)
        return text.upper() if self.typography.uppercase_headings else text

    def font(self, size, weight="normal", display=False):
        """Return a Tk font tuple with a Vietnamese-safe family."""
        stack = self.typography.display if display and self.typography.display else self.typography.body
        family = fonts.resolve_family(tuple(stack))
        scaled = max(8, round(int(size) * self.typography.scale))
        return (family, scaled, weight)

    def family(self, display=False):
        stack = self.typography.display if display and self.typography.display else self.typography.body
        return fonts.resolve_family(tuple(stack))


# Standard type sizes used by the UI; styles may scale them via Typography.
SIZE = {
    "brand": 14,
    "word": 56,
    "hint": 18,
    "entry": 28,
    "feedback": 18,
    "info_word": 54,
    "heading": 13,
    "definition": 22,
    "example": 20,
    "translation": 18,
    "chip_label": 16,
    "chip": 14,
    "button": 15,
    "enter": 26,
    "small": 11,
    "body": 14,
}
