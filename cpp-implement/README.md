# KotobaCpp (Kotoba Lock)

A Win32 tray app for Japanese word study. It shows a countdown, then an input
screen where you type the romaji of a randomly picked Japanese word, then an
info screen with the word's details.

The UI is not a browser: it is a small in-process HTML/CSS engine (`src/ui/`) that
parses a Tailwind-style subset of HTML, resolves styles, lays out the tree and
paints it with GDI+. The five screens are plain HTML files (`assets/htmls/`) that are
embedded into the executable at build time, so the app is a single `.exe`.

## Building

Requires Windows with Visual Studio (MSVC + CMake). Then:

```
build.bat
```

Configures CMake, builds the Release configuration, produces `build\app.exe`
and the test tools, and launches the app.

- `app.cpp` is the main source; `src/ui/*.cpp` and `src/utils/*.cpp` are the
  framework. The app proper lives in `src/utils/kotoba_app.{h,cpp}` (driven
  headlessly by `src/tests/app_test.cpp`).
- `assets/htmls/*` and `database/words/*` are embedded as assets
  (`cmake/EmbedAssets.cmake`).
- `src/third_party/json.hpp` is nlohmann/json, used for the word database.

## Layout

```
app.cpp            entry point; the app proper is src/utils/kotoba_app.{h,cpp}
src/ui/            HTML/CSS engine
  core/            shared types (Color, RectF, Length), utf helpers
  dom/             node tree + tree operations
  parser/          HTML parser
  style/           Tailwind-style class resolution (palette, spacing, variants)
  layout/          flex/absolute layout
  paint/           GDI+ canvas, text drawing
  svg/             path parsing for vector shapes
  controls/        TextInput, TextArea, RangeInput, Checkbox
  app/             Document (tree+style+layout+events), WindowHost, ScriptRunner
src/utils/         word database + even picker, config/progress stores,
                     window helpers, tray, debug logger, ...
database/*.py      manual Python generators (words, audio, validation)
assets/htmls/      the five screens (countdown, tray_menu, settings, input, info)
assets/themes/     UI styles for all five screens (registry.txt, _contract.txt,
                     ink/{countdown,settings,tray_menu,input,info}.html,
                     sunrise/{countdown,settings,tray_menu,input,info}.html)
database/words.txt user word list, one Japanese word per line
database/words/    word JSON files, one per word (embedded)
database/audio/    generated OGG audio (embedded when present)
src/tests/         regression pipeline (see below)
src/tests/ui_test.cpp  headless script runner (console entry point)
src/tests/app_test.cpp headless feature-test runner (drives KotobaCppApp)
assets/imgs/       browser-based Tailwind design references for the screens
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
`{{romaji}}`, ... placeholders, filled in at window creation. Example rows on
the info screen are cloned from `<template id="example-row">` and filled from
`examples[]` at runtime. `{{key}}` placeholders that have no matching field are
left literal and reported by the logger/validator.

The generator writes a newer schema (`word`, `reading`, `romaji`, `meaning`,
`type`, `audio`, `examples[]` with `jp`/`reading`/`romaji`/`en`/`audio`). The
C++ app reads both schemas, so old and new word files can coexist.

## Adding words

The generators are manual Python tools; the app never runs them.

```
1. edit database/words.txt            (one Japanese word per line, # comments)
2. python database/gen_words.py       (llama.cpp :9931 -> database/words/*.json)
3. python database/gen_audio.py       (llama.cpp :8080 TTS + ffmpeg -> OGG)
4. python database/validate_db.py     (schema, romaji, duplicates, audio files)
```

- `gen_words.py` skips words with a valid JSON; `--force` regenerates.
  Failures are logged to `database/gen_words.errors.log`.
- `gen_audio.py` writes `database/audio/<word>/word.ogg` and `ex_<n>.ogg`,
  updates the JSON paths, skips existing non-empty files (`--force`), and
  loudness-normalizes every clip. `--word-mode carrier` synthesizes a single
  word inside a short phrase and trims the middle segment.
- The info screen's `#speak` and per-example `listen-N` buttons play those
  clips (`src/utils/play_audio.cpp` transcodes OGG and applies `config.volume`).
  Missing files grey the button out instead of failing.
- `validate_db.py` exits non-zero on any invalid file and writes
  `database/validate_db.errors.log`. At runtime the C++ app applies a light
  check of its own: a broken word is logged (`dbg::log("db", ...)`) and
  excluded from the picker instead of crashing the app.

## Adding a UI style

The five screens are styled by a per-word UI style. A style is pure data: a
folder `assets/themes/<name>/` with `countdown.html`, `settings.html`,
`tray_menu.html`, `input.html` and `info.html`, listed one per line in
`assets/themes/registry.txt`. There is no C++ per style: bindings are written
only against the element ids in `assets/themes/_contract.txt` (`#countdown`,
`#tray-stop`, `#volume`, `#word-jp`, `#answer`, `#speak`,
`template#example-row` with `.jp`/`.romaji`/`.en`/`.listen`, ...). A page
missing from a style folder falls back to the matching `assets/htmls/` page.

```
1. cp -r assets/themes/ink assets/themes/<new>      (then restyle the classes)
2. add <new> to assets/themes/registry.txt
3. reconfigure CMake (assets are embedded at configure time) and rebuild
4. powershell -File src\tests\run.ps1 -Filter theme_contract   (must pass)
```

With `config.randomUi` on, each new word gets a random style that never
repeats the previous one; with it off, the first style in `registry.txt` is
used. The `theme_contract` feature iterates the whole registry, so a new
style is verified by the existing test with no new steps file.

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
- **`logs/kotoba_debug.log`** — file logger with categories, enabled by
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
  under `src/tests/features/`; every feature ships a `steps.txt` that fails
  before the change and passes after.
- **Command-file control of the running app** — with `--debug` or
  `KOTOBA_DEBUG=1`, the app polls `debug_cmd.txt` every 250 ms, runs the same
  `ScriptRunner` commands against a bound window, and writes the report to
  `debug_out.txt`. `app state` / `app tick N` / `app word <file>` drive app
  logic without waiting for real timers (debug tick is 10 ms).

### Script language

One command registry (`src/ui/app/script.cpp`), shared by `ui_test.exe`,
`app_test.exe` and the running-app poller: the built-in document commands
plus the `app`/`window` commands registered by
`KotobaCppApp::registerCommands()`. Unknown commands, unknown assert kinds,
unknown keys and short argument lists are all **failures**, never silent
passes; failures report `FAIL <file>:<line>: ...` (clickable in VS/VS Code).

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
assert exists #id / visible #id        # visible: hidden by own display/zero
                                       # width OR any ancestor's display:none
assert not exists #id / not visible #id
assert text|value #id == X      # the operator must be ==, exactly
assert caret #id == N
assert rect #id w>0             # x/y/w/h with < > <= >= == (== has 0.5 px slack)
golden expected.layout.json [update]   # compact layout snapshot
include <path>                # run another script (relative to this one);
                               # shared setups live in src/tests/features/common/
expect-fail <command>         # passes only if the inner command FAILS
                               # (guards against vacuous assertions)
app init / tick [n] / word <file> / add-word <file> / state
app assert <key> == <value>   # key in stateJson(), incl. `visible <window>`,
                               # config <key>, attempts <word>, attemptsTotal,
                               # attemptsMax, picked_unique <n> (== true|false),
                               # dbRejected, wordCount, audio (playing|idle),
                               # audio_last <path>, audio_ready, audio_disabled
app config <key> <value>      # set + persist (countdownSeconds, volume, ...)
app config reload             # re-read from disk (persistence check)
app progress reset            # clear attempt counts
app pick <n>                  # run the even picker n times (test seam)
app tray-event double-click | left-click | right-click
window <countdown|tray|input|info|settings>
```

`golden` compares a flat `element path -> [x,y,w,h]` map with a 0.5 px
tolerance and reports every mismatch. A missing golden is a failure; re-
recording the baseline is deliberate - `golden <file> update` in a steps
file, or `UPDATE_GOLDEN=1` in the environment (set by `run.ps1 -Update`).
Case files resolve `load`/`golden` paths relative to the script's own
directory, so a case folder is self-contained.

## Tests

`src/tests/run.ps1` is the single test driver (PowerShell, not batch): it
discovers every test, runs it, and writes `src/tests/out/report.txt` with a
failure summary at the bottom; exit code is nonzero on any failure or any
missing script/exe.

```
powershell -ExecutionPolicy Bypass -File src\tests\run.ps1                # everything
powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -Filter stop   # fast inner loop
powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -Update        # re-record goldens
powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -Seed random   # nightly fuzz
```

Discovery: `src/tests/cases/*/steps*.txt` (every script in a case folder runs,
not just `steps.txt`), `src/tests/features/*/steps.txt`, unit tests, the DPI
matrix, fuzz and the one real-process `smoke.ps1`.

| Layer | What it catches | Where |
|---|---|---|
| Golden layout snapshots | layout/style/parser regressions | `src/tests/cases/*/` (8 cases: flex center/row/column/grow, absolute, padding/margin, overflow, input caret) |
| Feature tests | app state-machine regressions (stop/start toggle, tray events) | `src/tests/features/*/` via `app_test.exe` |
| Metamorphic DPI check | any code path that ignores scale | `ui_test.exe --dpi-matrix <page>` over every case |
| Unit tests | pure functions (spacing, palette, classes, utf, parser, path, template) | `src/tests/unit/unit_tests.cpp` |
| Seeded fuzz + `validate()` | crashes, hangs, dangling pointers | `src/tests/fuzz/fuzz.cpp` (parser fuzz + event storm) |

Usage:

```
build\ui_test.exe --word src\tests\word.json --script src\tests\cases\flex_center\steps.txt
build\unit_tests.exe
build\fuzz.exe --seed 1 --iters 2000
```

Convention: after any engine change, run `run.ps1`, read
`src/tests/out/report.txt`, and on a failing case open the case's `shot`/dump
files. Never run with `-Update` (blessing a baseline) unless a human has
checked the screenshot.

Scaffolding: `powershell -File src\tests\new_feature.ps1 <name>` copies the
checklist template into `src/tests/features/<name>/steps.txt` and prints the
`run.ps1 -Filter <name>` command to watch it fail first.

## Environment variables

| Variable | Effect |
|---|---|
| `KOTOBA_LOG=all` (or `app,event,style,timer`) | enables `logs/kotoba_debug.log` categories |
| `KOTOBA_DEBUG=1` (or `--debug`) | command-file polling, 10 ms countdown tick |
| `KOTOBA_WORD_ID=database/words/<name>.json` | fixed word instead of random pick |
| `KOTOBA_SEED=N` | deterministic random pick |
