"""Midnight - the original deep-blue dashboard look."""

from ..base import Style, Typography

STYLE = Style(
    key="midnight",
    name="Midnight",
    description="Deep navy gradient, cool blue accents, clean sans-serif.",
    appearance="dark",
    color_theme="blue",
    gradient=True,
    background_top="#16233f",
    background_bottom="#04060c",
    card="#0b1324",
    card_border="#243354",
    chip_bg="#16233d",
    chip_hover="#1d2f50",
    entry_bg="#0f1a30",
    text_primary="#f8fafc",
    text_body="#dbe4f3",
    text_muted="#93a4c3",
    accent="#60a5fa",
    accent_soft="#7dd3fc",
    accent_hover="#3b82f6",
    on_accent="#07101f",
    error="#f87171",
    success="#4ade80",
    typography=Typography(
        body=("Segoe UI", "Arial", "Tahoma"),
        display=("Segoe UI", "Arial"),
        scale=1.0,
        uppercase_headings=True,
    ),
)
