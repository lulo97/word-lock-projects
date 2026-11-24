Here are 5 theme concepts designed to sit alongside `ink` (dark slate + indigo) and `sunrise` (warm cream + amber + rose). Each is tuned for the same page set (`countdown.html`, `info.html`, `input.html`, `settings.html`, `tray_menu.html`) so you can clone the existing theme folder and only change classes/palette.

---

## 1. `sakura` — soft cherry blossom
**Vibe:** spring, gentle, feminine, wide tracking. Light UI like `sunrise`, but pink/plum instead of amber/rose.

**Palette**
- Page bg: `#fdf2f8` (pink-50) or `#fff1f2`
- Card: `#ffffff`, border `#fbcfe8` (pink-200), hairline `#fce7f3`
- Primary accent: `#db2777` (pink-600), hover `#be185d`
- Secondary accent: `#9d174d` (plum)
- Text: `#500724` / `#831843`, muted `#a1a1aa`
- Highlight chip: `#fbcfe8` with `#831843` text

**Shape / type**
- Radius: `rounded-2xl` on cards, `rounded-full` on buttons/chips
- Tracking wide, `font-light` / `font-medium` mix (not `extrabold` like sunrise)
- Shadows: soft pink glow `shadow-[0_10px_30px_rgba(219,39,119,0.15)]`

**Differentiator:** `sunrise` uses bold rose+amber; `sakura` is pastel, airier, with plum accents and no orange.

---

## 2. `sumi` — washi paper & ink
**Vibe:** Japanese calligraphy, high contrast, minimalist, sharp edges. A light counterpart to `ink`.

**Palette**
- Page bg: `#f5f0e6` (washi), card `#faf6ec` / `#fffdf7`
- Ink text: `#0c0a09` (near-black), muted `#57534e`
- Hairline border: `#d6cfc0`, divider `#e7e1d3`
- Accent (hanko seal red): `#b93b2e`, hover `#9a2f24`
- Secondary accent: `#1c1917` only — no color besides the seal red

**Shape / type**
- Radius: `rounded-sm` / `rounded-none` — no pills
- Hairline `border` (1px), no shadows or extremely tight ones
- `font-light` with `tracking-[0.2em]` on headings; body `tracking-wide`
- Buttons: outlined ink with red hover underline

**Differentiator:** Only light-on-ink theme; the single red accent makes it immediately distinct.

---

## 3. `matcha` — zen garden / green tea
**Vibe:** calm, organic, natural. Light background, sage/lime accents, bamboo.

**Palette**
- Page bg: `#f7fee7` (lime-50), card `#ffffff`
- Border: `#d9f99d` (lime-200), hairline `#ecfccb`
- Primary accent: `#65a30d` (lime-600), hover `#4d7c0f`
- Secondary: `#3f6212` (deep leaf), tertiary `#d97706` (amber for one warm accent)
- Text: `#1a2e05` / `#365314`, muted `#78716c`

**Shape / type**
- Radius: `rounded-xl` soft, buttons `rounded-full`
- Tracking normal/normal-wide; weights `font-semibold`
- Subtle inner shadow on inputs

**Differentiator:** Only green theme; keeps a small amber accent so it isn't purely monochrome.

---

## 4. `yoru` — midnight neon
**Vibe:** dreamy night, dark blue/purple with cyan and lavender glow. Darker and bluer than `ink`.

**Palette**
- Page bg: `#131033` (deep indigo-violet), card `#1e1b4b`
- Border: `#312e81` (indigo-900), hairline `#3730a3`
- Primary accent: `#22d3ee` (cyan-400), hover `#06b6d4`
- Secondary accent: `#a78bfa` (violet-400)
- Text: `#e0e7ff`, muted `#818cf8`
- Glow: `shadow-[0_0_24px_rgba(34,211,238,0.25)]` on primary buttons

**Shape / type**
- Radius: `rounded-lg`, but with a glowing border instead of solid
- `font-medium`, tracking normal
- Accent chip: cyan text on translucent indigo

**Differentiator:** `ink` is slate+indigo, `yoru` is violet+cyan with actual glow. Feels like a different time of day.

---

## 5. `taisho` — Taisho Roman retro
**Vibe:** early-1900s Japan café: deep teal/peacock, burgundy, gold, cream paper. Decorative but restrained.

**Palette**
- Page bg: `#f5efe1` (aged cream), card `#fffaf0` / `#fdf6e3`
- Primary: `#0f4c4c` (deep teal), hover `#115e59`
- Secondary: `#7f1d1d` (burgundy) or `#881337`
- Accent: `#b45309` (antique gold), used sparingly on borders/chips
- Text: `#1c1917`, muted `#57534e`
- Border: `#0f4c4c` hairlines + `#b45309` accent stripe

**Shape / type**
- Radius: `rounded-md` / `rounded-sm`; buttons rectangular with a double border
- Tracking wide, serif-ish body weight, `font-semibold` headings
- Chip style: gold border + teal text (or inverse)

**Differentiator:** Only theme with a jewel-tone dark accent on a light page — instantly recognizable, not close to `sunrise`/`sakura`/`matcha`.

---

## Quick comparison

| Theme    | Light/Dark | Primary accent       | Shape          | Feel          |
|----------|------------|----------------------|----------------|---------------|
| `sakura` | Light      | Pink / plum          | `rounded-full` | Soft, spring  |
| `sumi`   | Light      | Near-black + seal red| `rounded-none` | Minimal, washi|
| `matcha` | Light      | Lime / leaf green    | `rounded-xl`   | Calm, organic |
| `yoru`   | Dark       | Cyan + violet glow   | `rounded-lg`   | Night, neon   |
| `taisho` | Light      | Teal + burgundy + gold | `rounded-md` | Retro, rich   |

---

## Files to add per theme

For each theme, clone `assets/themes/ink/` → `assets/themes/<name>/` and restyle:

- `countdown.html`, `info.html`, `input.html`, `settings.html`, `tray_menu.html`

Then register:

- `assets/themes/registry.txt` — add the name in order
- `assets/themes/_contract.txt` — confirm all required IDs/classes still exist (the contract test checks this)
- `src/utils/theme_registry.cpp` / `theme_registry.h` — verify loader picks up the new folder; `ThemeRegistry::pick` should cycle in registry order
- `src/utils/kotoba_app.cpp` — `next_theme`, `themed_page_html`, `ensure_*_theme` (probably data-driven already)
- `src/tests/features/theme_contract/steps.txt` — add one step per new theme
- `src/tests/features/theme_switch/steps.txt` — extend the cycle assertion to cover 7 themes

**Recommended order to implement:** `sumi` → `sakura` → `matcha` → `taisho` → `yoru`. Light themes are quicker to port from `sunrise`; `sumi` also stress-tests sharp-radius rendering, and `yoru` last stresses glow/shadow rendering.