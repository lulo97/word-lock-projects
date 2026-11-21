/**
 * The ten UI styles, ported one-for-one from the Python app's ui_styles
 * package. Colours, fonts, border widths and the countdown card colours all
 * come from here. Switching a style is a single object swap.
 */

const TYPOGRAPHY_DEFAULT = {
  body: ["Segoe UI", "Arial"],
  display: [],
  scale: 1.0,
  uppercase_headings: true,
};

function typography(overrides) {
  return { ...TYPOGRAPHY_DEFAULT, ...(overrides || {}) };
}

const STYLES = [
  {
    key: "midnight",
    name: "Midnight",
    description: "Deep navy gradient, cool blue accents, clean sans-serif.",
    appearance: "dark",
    color_theme: "blue",
    gradient: true,
    background_top: "#16233f",
    background_bottom: "#04060c",
    background: null,
    card: "#0b1324",
    card_border: "#243354",
    chip_bg: "#16233d",
    chip_hover: "#1d2f50",
    entry_bg: "#0f1a30",
    text_primary: "#f8fafc",
    text_body: "#dbe4f3",
    text_muted: "#93a4c3",
    accent: "#60a5fa",
    accent_soft: "#7dd3fc",
    accent_hover: "#3b82f6",
    on_accent: "#07101f",
    error: "#f87171",
    success: "#4ade80",
    button_bg: null,
    button_hover: null,
    button_border: null,
    button_text: null,
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.9,
    typography: typography({
      body: ["Segoe UI", "Arial", "Tahoma"],
      display: ["Segoe UI", "Arial"],
    }),
  },
  {
    key: "paper",
    name: "Paper",
    description: "Cream paper, ink text, serif headings, rust accent.",
    appearance: "light",
    color_theme: "blue",
    gradient: true,
    background_top: "#fdf6e8",
    background_bottom: "#ece0c9",
    background: null,
    card: "#fffaf0",
    card_border: "#d9c9a8",
    chip_bg: "#f1e6cf",
    chip_hover: "#e7d9bd",
    entry_bg: "#fffdf7",
    text_primary: "#2b241a",
    text_body: "#4a3f2f",
    text_muted: "#7b6a52",
    accent: "#b45309",
    accent_soft: "#92400e",
    accent_hover: "#92400e",
    on_accent: "#fffaf0",
    error: "#b91c1c",
    success: "#15803d",
    button_bg: "#f1e6cf",
    button_hover: "#e7d9bd",
    button_border: "#d9c9a8",
    button_text: "#92400e",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.95,
    typography: typography({
      body: ["Candara", "Corbel", "Segoe UI"],
      display: ["Palatino Linotype", "Constantia", "Candara", "Segoe UI"],
    }),
  },
  {
    key: "terminal",
    name: "Terminal",
    description: "Black-green console, neon phosphor text, monospace.",
    appearance: "dark",
    color_theme: "green",
    gradient: true,
    background_top: "#04140c",
    background_bottom: "#000000",
    background: null,
    card: "#04120b",
    card_border: "#14532d",
    chip_bg: "#0b2417",
    chip_hover: "#14532d",
    entry_bg: "#020c07",
    text_primary: "#d1fae5",
    text_body: "#a7f3d0",
    text_muted: "#4ade80",
    accent: "#22c55e",
    accent_soft: "#4ade80",
    accent_hover: "#16a34a",
    on_accent: "#02150b",
    error: "#f87171",
    success: "#4ade80",
    button_bg: "#0b2417",
    button_hover: "#14532d",
    button_border: "#14532d",
    button_text: "#4ade80",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.94,
    typography: typography({
      body: ["Consolas", "Courier New", "DejaVu Sans Mono", "Segoe UI"],
      display: ["Consolas", "Courier New", "Segoe UI"],
    }),
  },
  {
    key: "sage",
    name: "Sage",
    description: "Misty green wash, white cards, crisp square corners.",
    appearance: "light",
    color_theme: "green",
    gradient: true,
    background_top: "#f2f8f0",
    background_bottom: "#d7e8d5",
    background: null,
    card: "#ffffff",
    card_border: "#bfd8bf",
    chip_bg: "#e3f0e1",
    chip_hover: "#d3e8d0",
    entry_bg: "#f8fcf7",
    text_primary: "#14301a",
    text_body: "#2f4a34",
    text_muted: "#5f7a64",
    accent: "#2f855a",
    accent_soft: "#276749",
    accent_hover: "#276749",
    on_accent: "#ffffff",
    error: "#c53030",
    success: "#2f855a",
    button_bg: "#e3f0e1",
    button_hover: "#d3e8d0",
    button_border: "#bfd8bf",
    button_text: "#276749",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.95,
    typography: typography({
      body: ["Corbel", "Candara", "Segoe UI"],
      display: ["Corbel", "Constantia", "Candara", "Segoe UI"],
      uppercase_headings: false,
    }),
  },
  {
    key: "sunset",
    name: "Sunset",
    description: "Plum and ember gradient, coral highlights, soft humanist type.",
    appearance: "dark",
    color_theme: "blue",
    gradient: true,
    background_top: "#2b0b2f",
    background_bottom: "#451a0b",
    background: null,
    card: "#1d0b18",
    card_border: "#7c2d5e",
    chip_bg: "#3b1330",
    chip_hover: "#55204a",
    entry_bg: "#140713",
    text_primary: "#fff5f0",
    text_body: "#fbdccb",
    text_muted: "#d3a58f",
    accent: "#fb7185",
    accent_soft: "#fdba74",
    accent_hover: "#f43f5e",
    on_accent: "#2b0b12",
    error: "#fecaca",
    success: "#86efac",
    button_bg: "#3b1330",
    button_hover: "#55204a",
    button_border: "#7c2d5e",
    button_text: "#fdba74",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.92,
    typography: typography({
      body: ["Candara", "Corbel", "Segoe UI"],
      display: ["Candara", "Constantia", "Segoe UI"],
      uppercase_headings: false,
    }),
  },
  {
    key: "ocean",
    name: "Ocean",
    description: "Deep sea gradient, cyan accent, compact technical headings.",
    appearance: "dark",
    color_theme: "blue",
    gradient: true,
    background_top: "#052f3d",
    background_bottom: "#01131f",
    background: null,
    card: "#05222d",
    card_border: "#0f4c5c",
    chip_bg: "#073442",
    chip_hover: "#0b4a5c",
    entry_bg: "#031a23",
    text_primary: "#ecfeff",
    text_body: "#cffafe",
    text_muted: "#7dd3fc",
    accent: "#22d3ee",
    accent_soft: "#67e8f9",
    accent_hover: "#06b6d4",
    on_accent: "#012028",
    error: "#fb7185",
    success: "#34d399",
    button_bg: "#073442",
    button_hover: "#0b4a5c",
    button_border: "#0f4c5c",
    button_text: "#67e8f9",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.9,
    typography: typography({
      body: ["Bahnschrift", "Segoe UI", "Arial"],
      display: ["Bahnschrift", "Segoe UI", "Arial"],
    }),
  },
  {
    key: "mono",
    name: "Mono",
    description: "White and graphite blueprint, monospace, hard square edges.",
    appearance: "light",
    color_theme: "dark-blue",
    gradient: true,
    background_top: "#ffffff",
    background_bottom: "#e5e7eb",
    background: null,
    card: "#ffffff",
    card_border: "#9ca3af",
    chip_bg: "#f3f4f6",
    chip_hover: "#e5e7eb",
    entry_bg: "#ffffff",
    text_primary: "#111827",
    text_body: "#374151",
    text_muted: "#6b7280",
    accent: "#111827",
    accent_soft: "#1f2937",
    accent_hover: "#374151",
    on_accent: "#ffffff",
    error: "#b91c1c",
    success: "#166534",
    button_bg: "#f3f4f6",
    button_hover: "#e5e7eb",
    button_border: "#9ca3af",
    button_text: "#111827",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 2,
    overlay_alpha: 0.96,
    typography: typography({
      body: ["Courier New", "Consolas", "DejaVu Sans Mono", "Segoe UI"],
      display: ["Courier New", "Consolas", "Segoe UI"],
      scale: 0.98,
    }),
  },
  {
    key: "royal",
    name: "Royal",
    description: "Violet court, gold trim, serif display, tall square cards.",
    appearance: "dark",
    color_theme: "dark-blue",
    gradient: true,
    background_top: "#1b1035",
    background_bottom: "#09040f",
    background: null,
    card: "#160f2b",
    card_border: "#4c1d95",
    chip_bg: "#241744",
    chip_hover: "#32205c",
    entry_bg: "#120c24",
    text_primary: "#f5f3ff",
    text_body: "#ddd6fe",
    text_muted: "#a78bfa",
    accent: "#fbbf24",
    accent_soft: "#fcd34d",
    accent_hover: "#f59e0b",
    on_accent: "#1b1035",
    error: "#fb7185",
    success: "#4ade80",
    button_bg: "#241744",
    button_hover: "#32205c",
    button_border: "#4c1d95",
    button_text: "#fcd34d",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.92,
    typography: typography({
      body: ["Constantia", "Corbel", "Segoe UI"],
      display: ["Palatino Linotype", "Constantia", "Segoe UI"],
      scale: 1.02,
    }),
  },
  {
    key: "candy",
    name: "Candy",
    description: "Rose-pink light theme, friendly square shapes, no shouting caps.",
    appearance: "light",
    color_theme: "blue",
    gradient: true,
    background_top: "#fff1f7",
    background_bottom: "#ffe4f0",
    background: null,
    card: "#ffffff",
    card_border: "#f9a8d4",
    chip_bg: "#fce7f3",
    chip_hover: "#fbcfe8",
    entry_bg: "#fff7fb",
    text_primary: "#3b0a26",
    text_body: "#5f2145",
    text_muted: "#9d5c7d",
    accent: "#db2777",
    accent_soft: "#be185d",
    accent_hover: "#be185d",
    on_accent: "#ffffff",
    error: "#dc2626",
    success: "#059669",
    button_bg: "#fce7f3",
    button_hover: "#fbcfe8",
    button_border: "#f9a8d4",
    button_text: "#be185d",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.96,
    typography: typography({
      body: ["Candara", "Corbel", "Segoe UI"],
      display: ["Candara", "Corbel", "Segoe UI"],
      uppercase_headings: false,
    }),
  },
  {
    key: "steel",
    name: "Steel",
    description: "Cool slate grey, tight ergonomic type, orange accent.",
    appearance: "light",
    color_theme: "blue",
    gradient: true,
    background_top: "#eef2f7",
    background_bottom: "#cbd5e1",
    background: null,
    card: "#f8fafc",
    card_border: "#94a3b8",
    chip_bg: "#e2e8f0",
    chip_hover: "#cbd5e1",
    entry_bg: "#ffffff",
    text_primary: "#0f172a",
    text_body: "#334155",
    text_muted: "#64748b",
    accent: "#ea580c",
    accent_soft: "#c2410c",
    accent_hover: "#c2410c",
    on_accent: "#ffffff",
    error: "#dc2626",
    success: "#15803d",
    button_bg: "#e2e8f0",
    button_hover: "#cbd5e1",
    button_border: "#94a3b8",
    button_text: "#c2410c",
    primary_bg: null,
    primary_hover: null,
    primary_text: null,
    border_width: 1,
    overlay_alpha: 0.95,
    typography: typography({
      body: ["Tahoma", "Verdana", "Segoe UI"],
      display: ["Verdana", "Tahoma", "Segoe UI"],
      scale: 0.96,
    }),
  },
];

// Standard type sizes used by the UI; a style scales them via typography.scale.
const SIZE = {
  brand: 14,
  word: 56,
  hint: 18,
  entry: 28,
  feedback: 18,
  info_word: 54,
  heading: 13,
  definition: 22,
  example: 20,
  translation: 18,
  chip_label: 16,
  chip: 14,
  button: 15,
  enter: 26,
  small: 11,
  body: 14,
};

const BY_KEY = new Map(STYLES.map((style) => [style.key, style]));

function currentDefault() {
  return STYLES[0];
}

function getStyle(key) {
  return BY_KEY.get(String(key || "").trim().toLowerCase()) || null;
}

function randomStyle(exclude) {
  const excludeKey = exclude && exclude.key ? exclude.key : exclude;
  const candidates = STYLES.filter((style) => style.key !== excludeKey);
  return (candidates.length ? candidates : STYLES)[
    Math.floor(Math.random() * (candidates.length || STYLES.length))
  ];
}

function fontStack(stack) {
  const extras = ["Yu Gothic UI", "Meiryo", "Segoe UI", "sans-serif"];
  const seen = new Set();
  const families = [];
  for (const family of [...(stack || []), ...extras]) {
    if (!family) continue;
    const key = String(family).trim().toLowerCase();
    if (!key || seen.has(key)) continue;
    seen.add(key);
    families.push(String(family).trim());
  }
  return families
    .map((family) =>
      /[\s]/.test(family) && !family.startsWith('"') ? `"${family}"` : family,
    )
    .join(", ");
}

/**
 * Turn a style into CSS custom properties consumed by every window.
 */
function cssVariables(style) {
  const typo = style.typography || TYPOGRAPHY_DEFAULT;
  const bodyStack =
    typo.body && typo.body.length ? typo.body : TYPOGRAPHY_DEFAULT.body;
  const displayStack =
    typo.display && typo.display.length ? typo.display : bodyStack;
  const borderWidth = style.border_width || 1;

  const secondaryBg = style.button_bg || style.chip_bg;
  const secondaryHover = style.button_hover || style.chip_hover;
  const secondaryBorder = style.button_border || style.card_border;
  const secondaryText = style.button_text || style.accent_soft;
  const primaryBg = style.primary_bg || style.accent;
  const primaryHover = style.primary_hover || style.accent_hover;
  const primaryText = style.primary_text || style.on_accent;

  return {
    "--bg": style.background || style.background_bottom,
    "--bg-top": style.background_top,
    "--bg-bottom": style.background_bottom,
    "--bg-image": style.gradient
      ? `linear-gradient(to bottom, ${style.background_top} 0%, ${style.background_bottom} 100%)`
      : "none",
    "--card": style.card,
    "--card-border": style.card_border,
    "--chip-bg": style.chip_bg,
    "--chip-hover": style.chip_hover,
    "--entry-bg": style.entry_bg,
    "--text-primary": style.text_primary,
    "--text-body": style.text_body,
    "--text-muted": style.text_muted,
    "--accent": style.accent,
    "--accent-soft": style.accent_soft,
    "--accent-hover": style.accent_hover,
    "--on-accent": style.on_accent,
    "--error": style.error,
    "--success": style.success,
    "--secondary-bg": secondaryBg,
    "--secondary-hover": secondaryHover,
    "--secondary-border": secondaryBorder,
    "--secondary-text": secondaryText,
    "--primary-bg": primaryBg,
    "--primary-hover": primaryHover,
    "--primary-text": primaryText,
    "--border-width": `${borderWidth}px`,
    "--input-border-width": `${Math.max(3, borderWidth)}px`,
    "--overlay-alpha": String(
      style.overlay_alpha === undefined ? 0.9 : style.overlay_alpha,
    ),
    "--font-body": fontStack(bodyStack),
    "--font-display": fontStack(displayStack),
    "--scale": String(typo.scale || 1),
    "--heading-transform": typo.uppercase_headings ? "uppercase" : "none",
    "--scrollbar-color": style.card_border,
    "--scrollbar-hover": style.chip_hover,
  };
}

/**
 * Renderer payload: the style plus its precomputed CSS custom properties.
 * Keeping the computation in the main process removes the duplicated mapping
 * that used to live in src/theme.js.
 */
function describe(style) {
  return { ...style, css: cssVariables(style) };
}

module.exports = {
  STYLES,
  SIZE,
  getStyle,
  randomStyle,
  currentDefault,
  cssVariables,
  describe,
  fontStack,
};
