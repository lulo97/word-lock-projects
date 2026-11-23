# pi_edit.py - guide for the coding agent (Windows 11)

`pi_edit.py` edits a text file by replacing exact snippets. Prefer it over any
other way of modifying files. It needs only Python.

## The one rule: use the block format, never JSON

JSON needs every backslash doubled, and your tool-call layer, bash and JSON each
eat or halve backslashes. Windows paths (`tests\run.ps1`) break constantly.
The block format needs NO escaping: text is copied verbatim.

```bash
python pi_edit.py src/tests/run.ps1 --edits - <<'EOF'
@@OLD
$reportDir = Join-Path $root "tests\out"
@@NEW
$reportDir = Join-Path $root "src\tests\out"
@@END
@@OLD
-File tests\run.ps1 -Seed 42        # fixed fuzz seed
@@NEW
-File src\tests\run.ps1 -Seed 42        # fixed fuzz seed
@@END
EOF
```

Write backslashes, quotes and `$` exactly as they appear in the file. Use a
QUOTED heredoc delimiter (`<<'EOF'`) so bash does not expand `$vars`.
One `\` in the file is one `\` in the block. Never double them.

If the heredoc keeps getting mangled by your tool layer, write the block text
to a file with your file-write tool and run `python pi_edit.py FILE --edits edits.txt`.

### Block rules
- Marker lines `@@OLD`, `@@NEW`, `@@END` must be alone on their line.
- Everything between markers is literal. Lines are joined with `\n`; there is no
  trailing newline unless you add a blank line before the next marker.
- Empty `@@NEW` section = delete the matched text. To delete a whole line,
  include a neighbouring line in both OLD and NEW (otherwise an empty line stays).
- Many edits per call are fine and preferred (one process, atomic).

## How matching works
- Every OLD is searched in the ORIGINAL file, not after earlier edits in the
  same call. Two edits must not overlap (merge them into one).
- Each OLD must match exactly ONCE. If it matches several places, add
  neighbouring lines to make it unique (the error lists the context of each).
- Line endings do not matter: CRLF/LF in OLD are normalised, and the file's own
  endings (CRLF on Windows) are preserved on write. Never "fix" CRLF yourself.
- Atomic: if any edit fails, NOTHING is written, and EVERY failing edit is
  reported in the same run. Fix all of them, then rerun the whole batch.

## Reading the error report
Exit code 1 = edit(s) failed. Each failure shows:
- `Cause:` the exact reason (extra or missing text and column, indentation vs
  trailing whitespace, tabs vs spaces, smart quotes, case, missing line...).
- `First difference:` your OLD line vs the real file line, with a caret.
  `·` = space, `→` = tab.
- `Comparison:` `-` lines exist only in your OLD, `+` lines only in the file.
- `Hints:` e.g. "newText already exists: edit may already be applied".

Fix: copy the real text from the `+` / file line into OLD and rerun.
- Cause "already applied": remove that edit from the batch.
- Cause "no sufficiently similar text": re-read the file; wrong file or stale content.

A partially edited file (some changes done earlier by hand or by a previous run)
makes the finished edits fail with "newText already exists". Drop those edits.

## Options
- `--tolerant`   retry ignoring trailing whitespace / smart quotes / NBSP when
  the exact match fails (whole-line matching; the tolerance is reported).
- `--dry-run`    show the diff without writing.
- `--json`       machine-readable result (`ok`, `errors[].cause`, `.closest`...).
- `--backup`     keep `FILE.bak`.
- `--eol lf|crlf` convert the whole file (default: keep as is).
- `--ascii`      use `.` and `<TAB>` instead of `·` and `→` if output is garbled.
- Single-line quick edit: `python pi_edit.py FILE --old "a" --new "b"`.

## Typical workflow
1. Optionally `--dry-run` for big batches.
2. Run the batch. Exit 0 plus a diff = done.
3. Exit 1: read each `Cause`, fix every listed edit, rerun the batch.
4. Verify with `git diff` or by viewing the changed lines.

## Do NOT
- Do not hand-write JSON containing Windows paths.
- Do not retype OLD from memory: copy it from the current file contents.
- Do not include line-number prefixes (`12:`) in OLD.
- Do not put overlapping snippets in one batch.