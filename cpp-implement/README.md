# KotobaCpp (Kotoba Lock)

A Win32 tray app for Japanese word study. It shows a countdown, then an input
screen where you type the romaji of a randomly picked Japanese word, then an
info screen with the word's details.

The UI is not a browser: it is a small in-process HTML/CSS engine (`ui/`) that
parses a Tailwind-style subset of HTML, resolves styles, lays out the tree and
paints it with GDI+. The five screens are plain HTML files (`htmls/`) that are
embedded into the executable at build time, so the app is a single `.exe`.

## Building

Requires Windows with Visual Studio (MSVC + CMake). Then:

```
build.bat
```

Configures CMake, builds the Release configuration, produces `build\app.exe`
and the test tools, and launches the app.

- `app.cpp` is the main source; `ui/*.cpp` and `utils/*.cpp` are the framework.
- `htmls/*` and `database/words/*` are embedded as assets (`cmake/EmbedAssets.cmake`).
- `third_party/json.hpp` is nlohmann/json, used for the word database.

## Layout

```
app.cpp            the app: countdown / tray / settings / input / info windows
ui/                HTML/CSS engine
  core/            shared types (Color, RectF, Length), utf helpers
  dom/             node tree + tree operations
  parser/          HTML parser
  style/           Tailwind-style class resolution (palette, spacing, variants)
  layout/          flex/absolute layout
  paint/           GDI+ canvas, text drawing
  svg/             path parsing for vector shapes
  controls/        TextInput, TextArea, RangeInput, Checkbox
  app/             Document (tree+style+layout+events), WindowHost, ScriptRunner
utils/             word database, window helpers, tray, debug logger, ...
htmls/             the five screens (countdown, tray_menu, settings, input, info)
database/words/    word JSON files, one per word (embedded)
tests/             regression pipeline (see below)
ui_test.cpp        headless script runner (console entry point)
app_test.cpp       headless feature-test runner (drives KotobaCppApp)
imgs/              browser-based Tailwind design references for the screens
```

## Word format

`database/words/<name>.json`:

```json
{
  "kanji": "りんご",
  "hiragana": "りんご",
  "romaji": "ringo",
  "pos": "Noun",
  "meaning_ja": "...",
  "meaning_ja_romaji": "...",
  "meaning_vi": "...",
  "audio": "",
  "examples": [ { "ja": "...", "romaji": "...", "vi": "..." } ],
  "tags": [ "..." ]
}
```

`input.html` / `info.html` reference these fields as `{{kanji}}`,
`{{romaji}}`, `{{example1_ja}}`, ... placeholders, filled in at window
creation. `{{key}}` placeholders that have no matching field are left literal
and reported by the logger/validator.

## Debug tooling

Built for agent-driven debugging: every tool produces files an agent can read.

- **`Document::dumpTree(path)`** — rich JSON dump of the tree: per-element
  rect, resolved style, state, `classIssues` (unknown/unsupported classes),
  `occludedBy`/`notHittable` (why a click misses), control values, and
  `problems` from `validate()`.
- **`Document::screenshot(path, w, h, scale, overlay)`** — offscreen PNG,
  optional debug-box overlay. Works on hidden windows.
- **`Document::validate()`** — structural invariants: duplicate ids, dangling
  `EventState` pointers, NaN/negative rects, out-of-range scroll, unresolved
  placeholders.
- **`kotoba_debug.log`** — file logger with categories, enabled by
  `KOTOBA_LOG=all` (or a comma list). Hooked into style resolution, clicks,
  timers, missing HTML, unresolved placeholders, app state transitions,
  chrome freeze/resume. Each line is flushed, so it survives crashes.
- **`ui_test.exe`** — headless script runner. Drives a `Document` directly
  (no window, no `handleMessage`) with deterministic mouse/keyboard input and
  answers with JSON dumps, PNG screenshots, `validate()` after every step and
  a pass/fail exit code.
- **`app_test.exe`** — headless feature-test runner. Drives a real (hidden)
  `KotobaCppApp` through a `steps.txt` of `app` + ScriptRunner commands:
  `app init` (windows, no message loop), `app tick [n]` (simulated countdown +
  one-shot timers), `app word <file>`, `app assert <key> == <value>` (keys in
  `stateJson()`, incl. `visible <window>`), `app tray-event double-click|
  left-click|right-click` (real tray-icon events). One folder per feature
  under `tests/features/`; every feature ships a `steps.txt` that fails
  before the change and passes after.
- **Command-file control of the running app** — with `--debug` or
  `KOTOBA_DEBUG=1`, the app polls `debug_cmd.txt` every 250 ms, runs the same
  `ScriptRunner` commands against a bound window, and writes the report to
  `debug_out.txt`. `app state` / `app tick N` / `app word <file>` drive app
  logic without waiting for real timers (debug tick is 10 ms).

### Script language

Shared by `ui_test.exe` and the running-app poller (`ui/app/script.cpp`):

```
size 800x600              # viewport
dpi 1.5                   # scale
load page.html            # (ui_test only) fill {{key}} with --word, load
click #id / hover #id / focus #id
type #id text             # type characters into a focused input
click-at #id fx fy         # click at fractional position
click-char #id N           # click exactly on character boundary N
key Enter / Backspace / Tab / ...
wheel x y delta
dump out.json              # rich JSON dump
shot out.png [overlay]     # PNG screenshot
assert exists #id / visible #id
assert text|value #id == X
assert caret #id == N
assert rect #id w>0         # x/y/w/h with < > <= >= ==
golden expected.layout.json [update]   # compact layout snapshot
```

`golden` compares a flat `element path -> [x,y,w,h]` map with a 0.5 px
tolerance and reports every mismatch. `golden <file> update` (re)records the
baseline; a missing file without `update` fails. Case files resolve
`load`/`golden` paths relative to the script's own directory, so a case
folder is self-contained.

## Tests

`tests/run_tests.bat` runs the whole pipeline and writes `tests/out/report.txt`;
exit code is nonzero on any failure.

| Layer | What it catches | Where |
|---|---|---|
| Golden layout snapshots | layout/style/parser regressions | `tests/cases/*/` (8 cases: flex center/row/column/grow, absolute, padding/margin, overflow, input caret) |
| Feature tests | app state-machine regressions (stop/start toggle, tray events) | `tests/features/*/` via `app_test.exe` |
| Metamorphic DPI check | any code path that ignores scale | `ui_test.exe --dpi-matrix <page>` over every case |
| Unit tests | pure functions (spacing, palette, classes, utf, parser, path, template) | `tests/unit/unit_tests.cpp` |
| Seeded fuzz + `validate()` | crashes, hangs, dangling pointers | `tests/fuzz/fuzz.cpp` (parser fuzz + event storm) |

Usage:

```
build\ui_test.exe --word tests\word.json --script tests\cases\flex_center\steps.txt
build\unit_tests.exe
build\fuzz.exe --seed 1 --iters 2000
```

Convention: after any engine change, run `run_tests.bat`, read
`tests/out/report.txt`, and on a failing case open the case's `shot`/dump
files. Never run `golden ... update` (blessing a baseline) unless a human has
checked the screenshot.

## Environment variables

| Variable | Effect |
|---|---|
| `KOTOBA_LOG=all` (or `app,event,style,timer`) | enables `kotoba_debug.log` categories |
| `KOTOBA_DEBUG=1` (or `--debug`) | command-file polling, 10 ms countdown tick |
| `KOTOBA_WORD_ID=database/words/<name>.json` | fixed word instead of random pick |
| `KOTOBA_SEED=N` | deterministic random pick |
