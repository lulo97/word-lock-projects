# UI styles

Ten complete looks for Word Lock, one module per style:

| key        | Look                                                      |
|------------|-----------------------------------------------------------|
| `midnight` | Deep navy gradient, blue accents (original look)          |
| `paper`    | Warm cream, ink text, Palatino serif headings, rust accent|
| `terminal` | Black-green console, phosphor green, monospace, square    |
| `sage`     | Misty light green, white cards, square corners             |
| `sunset`   | Plum-to-ember gradient, coral highlights                  |
| `ocean`    | Deep teal water, cyan glow, Bahnschrift headings          |
| `mono`     | White/graphite blueprint, Courier, hard edges             |
| `royal`    | Violet court, gold trim, serif display                    |
| `candy`    | Rose-pink light theme, friendly square shapes             |
| `steel`    | Cool slate grey, tight ergonomic type, orange accent      |

Each style declares its own colours (background gradient, card, chips,
buttons, text, accents), fonts, border widths and the countdown
card colours.

## Swapping at runtime

```python
import ui_styles

ui_styles.set_current("ocean")          # by key
ui_styles.set_current(some_style_obj)   # or by object
ui_styles.next_style()                  # next in registry order
ui_styles.random_style(exclude="ocean") # random, avoiding one key
```

Then rebuild the visible UI:

```python
app.ui.apply_style(ui_styles.current())
app.overlay.apply_style(ui_styles.current())
```

In the app this happens automatically:

- at launch the active style is `midnight`;
- pressing Enter on a word card picks a **random different style**;
- the tray menu has **Next UI style** for an instant preview.

## Fonts and Vietnamese text

`fonts.py` never trusts a font family blindly. For every style family it:

1. checks that Tk has the family installed,
2. opens the matching font file with Pillow and renders `ếứạữđĐệốồộầẩỹ…`,
3. rejects the family if those glyphs render as the missing-glyph box,
4. falls back through a curated list (`Segoe UI`, `Arial`, `Tahoma`, …).

Tk's named fonts (`TkDefaultFont`, `TkMenuFont`, …) are pointed at the verified
family too, so native dialogs and menus render Vietnamese correctly. All JSON
and Python files are read/written as UTF-8, so Vietnamese text is never
re-encoded through a legacy code page.

Because of this check, Georgia / Trebuchet / Impact and similar fonts that are
missing Vietnamese precomposed glyphs are skipped automatically even if a
style lists them first.

## Adding style #11

Create `ui_styles/styles/aurora.py`:

```python
from ..base import Style, Typography

STYLE = Style(
    key="aurora",
    name="Aurora",
    appearance="dark",
    background_top="#071a2b",
    background_bottom="#020812",
    card="#08172a",
    card_border="#1d4ed8",
    chip_bg="#0c2140",
    entry_bg="#061426",
    text_primary="#eff6ff",
    text_body="#dbeafe",
    text_muted="#93c5fd",
    accent="#38bdf8",
    accent_soft="#7dd3fc",
    accent_hover="#0ea5e9",
    on_accent="#04121f",
    error="#f87171",
    success="#4ade80",
    typography=Typography(
        body=("Candara", "Segoe UI"),
        display=("Candara", "Segoe UI"),
        uppercase_headings=True,
    ),
)
```

Import it in `ui_styles/styles/__init__.py` and add `aurora.STYLE` to `STYLES`.
No UI code changes are required.
