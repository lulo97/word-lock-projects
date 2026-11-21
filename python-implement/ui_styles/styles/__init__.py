"""The ten UI styles, one module per style, exported as a tuple."""

from . import candy, midnight, mono, ocean, paper, royal, sage, steel, sunset, terminal

STYLES = (
    midnight.STYLE,
    paper.STYLE,
    terminal.STYLE,
    sage.STYLE,
    sunset.STYLE,
    ocean.STYLE,
    mono.STYLE,
    royal.STYLE,
    candy.STYLE,
    steel.STYLE,
)

__all__ = [
    "STYLES",
    "candy",
    "midnight",
    "mono",
    "ocean",
    "paper",
    "royal",
    "sage",
    "steel",
    "sunset",
    "terminal",
]
