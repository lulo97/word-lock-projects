"""Paper - warm light theme with a bookish serif display face."""

from ..base import Style, Typography

STYLE = Style(
    key="paper",
    name="Paper",
    description="Cream paper, ink text, serif headings, rust accent.",
    appearance="light",
    color_theme="blue",
    gradient=True,
    background_top="#fdf6e8",
    background_bottom="#ece0c9",
    card="#fffaf0",
    card_border="#d9c9a8",
    chip_bg="#f1e6cf",
    chip_hover="#e7d9bd",
    entry_bg="#fffdf7",
    text_primary="#2b241a",
    text_body="#4a3f2f",
    text_muted="#7b6a52",
    accent="#b45309",
    accent_soft="#92400e",
    accent_hover="#92400e",
    on_accent="#fffaf0",
    error="#b91c1c",
    success="#15803d",
    button_bg="#f1e6cf",
    button_hover="#e7d9bd",
    button_border="#d9c9a8",
    button_text="#92400e",
    overlay_alpha=0.95,
    typography=Typography(
        body=("Candara", "Corbel", "Segoe UI"),
        display=("Palatino Linotype", "Constantia", "Candara", "Segoe UI"),
        scale=1.0,
        uppercase_headings=True,
    ),
)
