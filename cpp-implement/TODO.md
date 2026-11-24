Below is a rewritten issue list with likely affected files, plus a recommended fix order. Paths are relative to `C:\Users\ADMIN\Desktop\te\cpp-implement`. These mappings are inferred from the symbol/file index, so verify actual class names and HTML structure before editing.

> Prerequisite: `app.cpp` and `src/ui/app/window_host.cpp` are marked `[syntax errors]` in the symbol dump. If those are real, fix build blockers first.

## Rewritten issues and likely files

### Issue 1 — Word/answer text does not wrap in Input and Info screens
**Rewritten:** Long word/answer text in the input window and info window must wrap inside its container instead of overflowing or being clipped. No horizontal overflow; lines must break within the available width.

**Likely files:**
- `assets/htmls/input.html`
- `assets/htmls/info.html`
- `assets/themes/ink/input.html`
- `assets/themes/ink/info.html`
- `assets/themes/sunrise/input.html`
- `assets/themes/sunrise/info.html`
- `src/ui/controls/text_area.cpp` / `.h` — `wrapLines`
- `src/ui/controls/text_input.cpp` / `.h` — only if the word is currently using single-line input
- `src/ui/layout/inline_text.cpp` — `breakLines`, `layoutInline`
- `src/ui/layout/measure.cpp` — `measureElement`, `measureHeightForWidth`
- `src/ui/layout/layout.cpp`
- `src/ui/layout/flex.cpp`
- `src/ui/style/class_rules.cpp` — width / max-width / whitespace / wrapping classes
- `src/utils/kotoba_app.cpp` — `ensure_input`, `show_input`, `on_answer_changed`, `ensure_info`, `show_info`

**Tests to update/add:** `src/tests/cases/input_basic`, new wrap case, `src/tests/features/input_kanji/steps.txt`, `src/tests/features/info_tags/steps.txt`.

---

### Issue 2 — Add Enter-to-exit in Info screen
**Rewritten:** Pressing Enter while the info window is focused must close/exit the info window. Optionally show a hint like “Press Enter to close”.

**Likely files:**
- `assets/htmls/info.html`
- `assets/themes/ink/info.html`
- `assets/themes/sunrise/info.html`
- `src/ui/app/events.cpp` — `EventState::onKeyDown`
- `src/ui/app/events.h`
- `src/ui/app/window_host.cpp` — `WindowHost::handle`, `wndProc`
- `src/ui/app/document.cpp` / `.h` — `handleMessage`, possibly add key callback binding
- `src/utils/kotoba_app.cpp` — `ensure_info`, `show_info`
- `src/tests/features/info_tags/steps.txt`

**Note:** There is no obvious public `Document::onKey` in the symbol index, so this may require adding a small key binding path or handling it in `WindowHost`/`KotobaCppApp`.

---

### Issue 3 — Info screen content box: unequal top/bottom margin, no vertical centering, no scroll
**Rewritten:** The info screen content container must have equal top and bottom margins, be vertically centered when content is shorter than the viewport, and become scrollable when content is taller than the viewport. Stop the dev/debug loop by using layout snapshots/tests rather than repeated manual debugging.

**Likely files:**
- `assets/htmls/info.html`
- `assets/themes/ink/info.html`
- `assets/themes/sunrise/info.html`
- `src/ui/layout/flex.cpp` — `layoutFlexColumn`, `distributeJustify`
- `src/ui/layout/layout.cpp`
- `src/ui/layout/box.cpp` — `maxScrollY`, `clientHeight`
- `src/ui/layout/measure.cpp`
- `src/ui/paint/scrollbar.cpp` — `needsScrollbar`, `drawScrollbar`
- `src/ui/paint/paint.cpp`
- `src/ui/app/events.cpp` — wheel/scroll handling
- `src/ui/app/document.cpp` — `dumpTree`, `layoutSnapshot`, `handleMessage`
- `src/ui/style/class_rules.cpp`
- `src/ui/style/spacing.cpp`
- `src/utils/kotoba_app.cpp` — `populateInfoTags`, `populateInfoAudio`, `ensure_info`, `poll_debug_commands`
- `src/tests/cases/overflow_scroll`
- `src/tests/features/info_tags/steps.txt`

**Debug support:** Use `Document::layoutSnapshot()` / `dumpTree()` and `debug_out.txt` rather than looping through manual dev/debug.

---

### Issue 4 — Example block: Vietnamese and Romaji do not wrap; large empty space remains
**Rewritten:** In the example block, Vietnamese and Romaji text must wrap within the available width and must not leave large unused horizontal space. The line-breaking logic must handle non-CJK/Latin text correctly.

**Likely files:**
- `assets/htmls/info.html`
- `assets/themes/ink/info.html`
- `assets/themes/sunrise/info.html`
- `src/ui/controls/text_area.cpp` — `wrapLines`
- `src/ui/layout/inline_text.cpp` — `breakLines`, `collectInlineRuns`, `hasCjkText`
- `src/ui/layout/measure.cpp`
- `src/ui/layout/layout.cpp`
- `src/ui/paint/text_draw.cpp`
- `src/ui/style/class_rules.cpp`
- `src/utils/word_db.cpp` — `fill_template`
- `src/utils/kotoba_app.cpp` — `populateInfoTags`, `ensure_info`
- `src/tests/features/info_tags/steps.txt`
- `src/tests/features/listen_example/steps.txt`

---

### Issue 5 — Add more styles to UI themes
**Rewritten:** Expand the UI theme style set with additional reusable classes/variants/palette entries across themes, while keeping the theme contract valid.

**Likely files:**
- `assets/themes/ink/countdown.html`
- `assets/themes/ink/info.html`
- `assets/themes/ink/input.html`
- `assets/themes/ink/settings.html`
- `assets/themes/ink/tray_menu.html`
- `assets/themes/sunrise/*.html`
- `assets/themes/registry.txt`
- `assets/themes/_contract.txt`
- `src/ui/style/class_rules.cpp`
- `src/ui/style/class_table.cpp`
- `src/ui/style/variants.cpp`
- `src/ui/style/palette.cpp`
- `src/ui/style/style.cpp`
- `src/utils/theme_registry.cpp`
- `src/utils/kotoba_app.cpp` — theme functions
- `src/tests/features/theme_contract/steps.txt`
- `src/tests/features/theme_switch/steps.txt`

---

### Issue 6 — Settings window buttons: Done → Cancel, non-primary; Confirm primary/right; Cancel left; space-between
**Rewritten:** In the settings window, rename “Done” to “Cancel”. Cancel must only close the settings window and do nothing else. Cancel must be visually secondary/less primary than Confirm. Confirm must remain primary and be right-aligned; Cancel left-aligned; use space-between justification. Read STYLES.md to add more 5 styles.

**Likely files:**
- `assets/htmls/settings.html`
- `assets/themes/ink/settings.html`
- `assets/themes/sunrise/settings.html`
- `src/utils/kotoba_app.cpp` — `wire_settings`, `apply_settings`, `show_settings`
- `src/ui/style/class_rules.cpp` — button variants / flex justify
- `src/ui/layout/flex.cpp` — justify-between behavior
- `src/ui/app/bindings.cpp`
- `src/ui/app/document.cpp` — click bindings
- `src/tests/features/settings_countdown/steps.txt`
- `src/tests/features/settings_ignore_words/steps.txt`
- `src/tests/features/settings_random_ui/steps.txt`
- `src/tests/features/settings_volume/steps.txt`

---

### Issue 7 — Missing space in `Romaij:{{romaij}}` in Input and Info screens
**Rewritten:** Template labels must render as `Romaij: {{romaij}}` with one space after the colon, consistently in input and info screens.

**Likely files:**
- `assets/htmls/input.html`
- `assets/htmls/info.html`
- `assets/themes/ink/input.html`
- `assets/themes/ink/info.html`
- `assets/themes/sunrise/input.html`
- `assets/themes/sunrise/info.html`
- `src/utils/word_db.cpp` — `fill_template` only if placeholder formatting is affected
- `src/tests/features/info_tags/steps.txt`
- `src/tests/features/input_kanji/steps.txt`

**Note:** If the intended label is “Romaji”, fix spelling consistently at the same time.

## Suggested fix order

1. **Fix build blockers first**  
   Resolve `app.cpp` / `src/ui/app/window_host.cpp` syntax errors if real. Do not start layout debugging on a broken build.

2. **Issue 7 — Template spacing**  
   Lowest risk, quick win. Fix `Romaij: {{romaij}}` in all default/theme HTML templates. Run info/input feature tests.

3. **Issue 1 — Core text wrapping in Input/Info**  
   Fix shared wrapping first: `text_area` / `inline_text` / `measure`. Add a deterministic wrap test case. This is foundational for Issue 4.

4. **Issue 4 — Example block Vietnamese/Romaji wrapping**  
   Reuse the wrapping fix, then adjust example-block HTML/classes and `fill_template` output. Verify with `info_tags` and `listen_example`.

5. **Issue 3 — Info container margins, vertical centering, scroll**  
   Do this after wrapping is stable, because wrapping changes content height. Use `layoutSnapshot`/`dumpTree` and add/update overflow tests.

6. **Issue 2 — Enter-to-exit Info**  
   Small behavior change, but do it after the info layout is stable. Add key handling in `events`/`window_host`/`kotoba_app`.

7. **Issue 6 — Settings Cancel/Confirm buttons**  
   HTML + binding + style changes. Update settings feature tests.

8. **Issue 5 — Expand theme styles**  
   Do last, after functional layout/behavior fixes, so new styles do not mask layout bugs. Validate with `theme_contract` and `theme_switch`.

**After each step:** run `src/tests/unit/unit_tests.cpp`, `src/tests/ui_test.cpp`, `src/tests/app_test.cpp`, and the relevant feature `steps.txt`. Add regression steps for wrapping, info Enter-exit, info scroll/centering, and settings button behavior.