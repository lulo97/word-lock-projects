#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
pi_edit.py - exact-text file editor with detailed failure diagnosis.
Pure Python 3.8+, standard library only. Works on Windows 11 / Linux / macOS.

USAGE
  python pi_edit.py FILE --edits edits.json
  type edits.json | python pi_edit.py FILE --edits -          (cmd)
  Get-Content edits.json -Raw | python pi_edit.py FILE --edits -   (PowerShell)
  python pi_edit.py FILE --old-file old.txt --new-file new.txt
  python pi_edit.py FILE --old "foo(1)" --new "foo(2)"        (single line only)

RECOMMENDED FOR AGENTS - escape-free block format (no JSON, backslashes verbatim):
  python pi_edit.py src/tests/run.ps1 --edits - <<'EOF'
  @@OLD
  -File tests\run.ps1 -Seed 42
  @@NEW
  -File src\tests\run.ps1 -Seed 42
  @@END
  @@OLD
  $reportDir = Join-Path $root "tests\out"
  @@NEW
  $reportDir = Join-Path $root "src\tests\out"
  @@END
  EOF
  Text between the markers is copied exactly (no escaping at all). Lines are
  joined with \n and there is no trailing newline unless you add a blank line.
  The marker lines must be alone on their line. Empty @@NEW section = delete.

edits.json (all of these shapes are accepted):
  {"path": "src/run.ps1", "edits": [{"oldText": "...", "newText": "..."}]}
  [{"oldText": "...", "newText": "..."}, {"oldText": "...", "newText": "..."}]
  {"oldText": "...", "newText": "..."}

BEHAVIOUR
  * Every oldText is matched against the ORIGINAL file (not after earlier edits).
  * Each oldText must match exactly once. Overlapping edits are rejected.
  * All-or-nothing: if any edit fails, nothing is written, and EVERY failing edit
    is reported, each with the exact cause (closest region, first differing
    column, whitespace / quote / case / line-ending analysis, hints).
  * CRLF / LF / CR are detected. oldText and newText may use any line ending;
    the file's own style is preserved (mixed files keep each untouched line's
    ending). Override with --eol lf|crlf. BOM and UTF-16 are preserved.
  * --tolerant: if the exact match fails, retry ignoring trailing whitespace and
    typographic characters (smart quotes, en/em dashes, NBSP, zero-width).
    The edit is applied to the file's real text and the tolerance is reported.

OPTIONS
  --dry-run  --backup  --json  --ascii  --no-diff  --encoding NAME  --eol MODE

EXIT CODES
  0 ok   1 one or more edits failed (nothing written)   2 usage / I/O error

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

"""
import argparse
import bisect
import difflib
import json
import os
import re
import stat
import sys
import tempfile
import unicodedata

# --------------------------------------------------------------------------- #
# Display helpers
# --------------------------------------------------------------------------- #
SYM_UNI = {" ": "\u00b7", "\t": "\u2192", "\u00a0": "\u237d", "\r": "\u240d"}
SYM_ASCII = {" ": ".", "\t": "<TAB>", "\u00a0": "<NBSP>", "\r": "<CR>"}
SYM = dict(SYM_UNI)

INVISIBLE = "\u200b\u200c\u200d\u2060\ufeff\u00ad"
ODD_SPACES = "\u00a0\u2002\u2003\u2009\u202f\u3000"
_FOLD_SRC = {
    "\u2018": "'", "\u2019": "'", "\u201a": "'", "\u201b": "'",
    "\u201c": '"', "\u201d": '"', "\u201e": '"', "\u201f": '"',
    "\u2010": "-", "\u2011": "-", "\u2012": "-", "\u2013": "-",
    "\u2014": "-", "\u2015": "-", "\u2212": "-",
    "\u00a0": " ", "\u2002": " ", "\u2003": " ", "\u2009": " ",
    "\u202f": " ", "\u3000": " ", "\u2026": "...",
}
FOLD = {ord(k): v for k, v in _FOLD_SRC.items()}
for _c in INVISIBLE:
    FOLD[ord(_c)] = ""


def fold(s):
    return s.translate(FOLD)


def vis_char(c, show_space):
    if c == " ":
        return SYM[" "] if show_space else " "
    if c in SYM:
        return SYM[c]
    if ord(c) < 32 or c in INVISIBLE:
        return "<U+%04X>" % ord(c)
    return c


def vis_all(s):
    return "".join(vis_char(c, True) for c in s)


def vis(s):
    """Show a line; spaces are only made visible at the edges."""
    m = re.match(r"( *)(.*?)( *)\Z", s, re.S)
    lead, mid, trail = m.groups()
    return (SYM[" "] * len(lead) + "".join(vis_char(c, False) for c in mid)
            + SYM[" "] * len(trail))


def uname(c):
    try:
        return unicodedata.name(c)
    except ValueError:
        return "CONTROL CHARACTER"


def q(s, detail=True):
    out = "'" + vis_all(s) + "'"
    odd = [c for c in s if ord(c) > 126 or ord(c) < 32]
    if detail and odd and len(s) <= 4:
        out += " [" + ", ".join("U+%04X %s" % (ord(c), uname(c)) for c in odd) + "]"
    return out


def clip(s, n=90):
    return s if len(s) <= n else s[: n - 3] + "..."


def ws_desc(w):
    if not w:
        return "none"
    parts = []
    n_sp, n_tab, n_nb = w.count(" "), w.count("\t"), w.count("\u00a0")
    if n_sp:
        parts.append("%d space%s" % (n_sp, "" if n_sp == 1 else "s"))
    if n_tab:
        parts.append("%d tab%s" % (n_tab, "" if n_tab == 1 else "s"))
    if n_nb:
        parts.append("%d non-breaking space%s" % (n_nb, "" if n_nb == 1 else "s"))
    rest = len(w) - n_sp - n_tab - n_nb
    if rest:
        parts.append("%d other whitespace char%s" % (rest, "" if rest == 1 else "s"))
    return ", ".join(parts)


class ToolError(Exception):
    pass


# --------------------------------------------------------------------------- #
# File loading / writing (BOM, encoding, line endings)
# --------------------------------------------------------------------------- #
class Doc(object):
    pass


def load_doc(path, encoding):
    try:
        with open(path, "rb") as f:
            raw = f.read()
    except FileNotFoundError:
        raise ToolError("File not found: %s" % path)
    except PermissionError:
        raise ToolError("Permission denied reading %s (locked or read-only?)" % path)
    except IsADirectoryError:
        raise ToolError("%s is a directory, not a file" % path)
    d = Doc()
    d.path, d.raw, d.bom, d.enc = path, raw, b"", encoding
    body = raw
    if raw.startswith(b"\xef\xbb\xbf"):
        d.bom, body, d.enc = raw[:3], raw[3:], "utf-8"
    elif raw.startswith(b"\xff\xfe"):
        d.bom, body, d.enc = raw[:2], raw[2:], "utf-16-le"
    elif raw.startswith(b"\xfe\xff"):
        d.bom, body, d.enc = raw[:2], raw[2:], "utf-16-be"
    try:
        s = body.decode(d.enc)
    except UnicodeDecodeError as e:
        raise ToolError(
            "Cannot decode %s as %s: invalid byte 0x%02x at offset %d. The file is "
            "binary or uses another encoding. Try --encoding cp1252 (or latin-1)."
            % (path, d.enc, body[e.start], e.start + len(d.bom)))
    crlf = s.count("\r\n")
    d.counts = {"CRLF": crlf, "LF": s.count("\n") - crlf, "CR": s.count("\r") - crlf}
    present = [k for k, v in d.counts.items() if v]
    d.mixed = len(present) > 1
    dom = max(d.counts, key=lambda k: d.counts[k]) if present else "LF"
    d.eol_name = dom
    d.eol = {"CRLF": "\r\n", "LF": "\n", "CR": "\r"}[dom]
    parts = re.split(r"(\r\n|\n|\r)", s)
    d.lines, d.ends = parts[0::2], parts[1::2]
    d.text = "\n".join(d.lines)
    d.starts = [0]
    for ln in d.lines[:-1]:
        d.starts.append(d.starts[-1] + len(ln) + 1)
    return d


def line_of(doc, off):
    return bisect.bisect_right(doc.starts, off) - 1


def rebuild(doc, new_text, eol_mode):
    new_lines = new_text.split("\n")
    if eol_mode == "keep" and doc.mixed:
        sm = difflib.SequenceMatcher(None, doc.lines, new_lines, autojunk=False)
        ends = [None] * (len(new_lines) - 1)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                for off in range(j2 - j1):
                    j, i = j1 + off, i1 + off
                    if j < len(ends) and i < len(doc.ends):
                        ends[j] = doc.ends[i]
        ends = [e if e is not None else doc.eol for e in ends]
        return "".join(l + e for l, e in zip(new_lines[:-1], ends)) + new_lines[-1]
    eol = doc.eol if eol_mode == "keep" else {"lf": "\n", "crlf": "\r\n"}[eol_mode]
    return new_text.replace("\n", eol)


def write_atomic(path, data, backup_raw=None):
    if backup_raw is not None:
        with open(path + ".bak", "wb") as f:
            f.write(backup_raw)
    directory = os.path.dirname(os.path.abspath(path))
    fd, tmp = tempfile.mkstemp(prefix=".pi_edit_", dir=directory)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        try:
            os.chmod(tmp, stat.S_IMODE(os.stat(path).st_mode))
        except OSError:
            pass
        os.replace(tmp, path)
    except PermissionError:
        raise ToolError("Permission denied writing %s (open in another program, "
                        "or read-only?). Nothing was written." % path)
    finally:
        if os.path.exists(tmp):
            try:
                os.remove(tmp)
            except OSError:
                pass


# --------------------------------------------------------------------------- #
# Locating oldText
# --------------------------------------------------------------------------- #
def tolerant_spans(doc, old):
    ol = old.split("\n")
    trailing = len(ol) > 1 and ol[-1] == ""
    if trailing:
        ol = ol[:-1]
    if not any(x.strip() for x in ol):
        return []
    tgt = [fold(x).rstrip() for x in ol]
    fl = [fold(x).rstrip() for x in doc.lines]
    k, res = len(tgt), []
    for i in range(len(fl) - k + 1):
        if fl[i:i + k] == tgt:
            last = i + k - 1
            end = doc.starts[last] + len(doc.lines[last])
            if trailing and last < len(doc.lines) - 1:
                end += 1
            res.append((doc.starts[i], end))
    return res


def locate(doc, old, tolerant):
    spans, i = [], doc.text.find(old)
    while i != -1:
        spans.append((i, i + len(old)))
        i = doc.text.find(old, i + 1)
    if spans:
        return spans, "exact"
    if tolerant:
        sp = tolerant_spans(doc, old)
        if sp:
            return sp, "tolerant"
    return [], None


# --------------------------------------------------------------------------- #
# Diagnosis: why does oldText not match?
# --------------------------------------------------------------------------- #
def line_matches(a, b, lead, trail):
    if lead and trail:
        return a in b
    if lead:
        return b.endswith(a)
    if trail:
        return b.startswith(a)
    return a == b


def pos_score(old_lines, lines, s):
    tot = w = 0.0
    for j, a in enumerate(old_lines):
        i = s + j
        b = lines[i] if 0 <= i < len(lines) else ""
        a2, b2 = a.strip(), b.strip()
        wt = max(len(a2), 1)
        if not a2 and not b2:
            r = 1.0
        elif a2 and a2 in b2:
            r = 1.0
        else:
            r = difflib.SequenceMatcher(None, a2, b2, autojunk=False).ratio()
        tot += r * wt
        w += wt
    return tot / w if w else 0.0


def closest_window(lines, old_lines):
    n, k = len(lines), len(old_lines)
    if n == 0:
        return None
    stripped = [l.strip() for l in lines]
    index = {}
    for i, s in enumerate(stripped):
        if s:
            index.setdefault(s, []).append(i)
    nonblank = [(j, l.strip()) for j, l in enumerate(old_lines) if l.strip()]
    votes = {}
    for j, s in nonblank:
        for i in index.get(s, ()):
            votes[i - j] = votes.get(i - j, 0) + 1
    if not votes:                      # fragments: containment
        for j, s in nonblank[:20]:
            for i, t in enumerate(stripped):
                if t and (s in t or (len(t) >= 8 and t in s)):
                    votes[i - j] = votes.get(i - j, 0) + 1
    if not votes and nonblank:         # fuzzy on the most informative line
        j, s = max(nonblank, key=lambda x: len(x[1]))
        sm = difflib.SequenceMatcher(None, autojunk=False)
        sm.set_seq2(s)
        scored = []
        for i, t in enumerate(stripped):
            if not t:
                continue
            sm.set_seq1(t)
            if sm.real_quick_ratio() < 0.45 or sm.quick_ratio() < 0.45:
                continue
            scored.append((sm.ratio(), i))
        scored.sort(reverse=True)
        for _, i in scored[:5]:
            votes[i - j] = 1
    if not votes:
        return None
    best = None
    for s0 in sorted(votes, key=lambda x: -votes[x])[:8]:
        s = max(0, min(s0, n - 1))
        sc = pos_score(old_lines, lines, s0)
        if best is None or sc > best[2]:
            best = (s, min(n, s + k), sc, s0)
    s, e, sc, s0 = best
    s = max(0, min(s0, n - 1))
    return s, min(n, s + k), sc


def explain_pair(oi, fl, a, b, lead, trail):
    """Explain how oldText line `a` differs from file line `b`."""
    ops = difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes()
    if lead and ops and ops[0][0] == "insert":
        ops = ops[1:]
    if trail and ops and ops[-1][0] == "insert":
        ops = ops[:-1]
    bad = [o for o in ops if o[0] != "equal"]
    bs = ops[0][3] if ops else 0
    be = ops[-1][4] if ops else len(b)
    piece = b[bs:be]
    f = {"kind": "text", "old_line": oi + 1, "file_line": fl, "a": a, "b": b}
    if bad:
        t, i1, i2, j1, j2 = bad[0]
        f["col_old"], f["col_file"] = i1 + 1, j1 + 1
    else:
        f["col_old"] = f["col_file"] = 1
    detail = []

    lead_a, lead_b = a[:len(a) - len(a.lstrip())], piece[:len(piece) - len(piece.lstrip())]
    trail_a, trail_b = a[len(a.rstrip()):], piece[len(piece.rstrip()):]
    if a.strip() == piece.strip() and a != piece:
        parts = []
        if lead_a != lead_b and not lead:
            parts.append("indentation: oldText has %s, file has %s"
                         % (ws_desc(lead_a), ws_desc(lead_b)))
        if trail_a != trail_b and not trail:
            parts.append("trailing whitespace: oldText has %s, file has %s"
                         % (ws_desc(trail_a), ws_desc(trail_b)))
        if parts:
            f["kind"] = "whitespace"
            f["headline"] = ("whitespace-only difference on file line %d (%s)"
                             % (fl, "; ".join(parts)))
            f["col_old"] = f["col_file"] = (
                1 if "indentation" in parts[0] else len(a.rstrip()) + 1)
            return f, detail
    if re.sub(r"\s+", " ", a).strip() == re.sub(r"\s+", " ", piece).strip():
        f["kind"] = "spacing"
        f["headline"] = ("spacing inside file line %d differs (tabs vs spaces, or a "
                         "different number of spaces between words)" % fl)
        return f, detail
    if fold(a) == fold(piece):
        f["kind"] = "typographic"
        f["headline"] = ("typographic character mismatch on file line %d "
                         "(look-alike characters)" % fl)
        for t, i1, i2, j1, j2 in bad[:4]:
            detail.append("column %d: oldText has %s, file has %s"
                          % (i1 + 1, q(a[i1:i2]), q(b[j1:j2])))
        return f, detail
    if a.lower() == piece.lower():
        f["kind"] = "case"
        f["headline"] = "letter-case differs on file line %d" % fl
        for t, i1, i2, j1, j2 in bad[:4]:
            detail.append("column %d: oldText has %s, file has %s"
                          % (i1 + 1, q(a[i1:i2]), q(b[j1:j2])))
        return f, detail
    # generic character-level description
    for n, (t, i1, i2, j1, j2) in enumerate(bad[:4]):
        if t == "replace":
            msg = "column %d: oldText has %s where the file has %s" % (
                i1 + 1, q(a[i1:i2]), q(b[j1:j2]))
            head = ("text differs at column %d of file line %d: oldText has %s, "
                    "file has %s" % (i1 + 1, fl, q(a[i1:i2]), q(b[j1:j2])))
        elif t == "delete":
            if j1 >= be:
                msg = ("column %d: oldText continues with %s but the file text ends "
                       "after %s" % (i1 + 1, q(a[i1:i2]), q(a[:i1][-12:] or "")))
                head = ("oldText has extra text %s at column %d that is not in the "
                        "file (file line %d ends after %s)"
                        % (q(a[i1:i2]), i1 + 1, fl, q(a[:i1][-12:])))
            else:
                msg = "column %d: oldText has extra %s that is not in the file" % (
                    i1 + 1, q(a[i1:i2]))
                head = ("oldText has extra text %s at column %d that is not in file "
                        "line %d" % (q(a[i1:i2]), i1 + 1, fl))
        else:
            msg = "column %d: the file has %s here that oldText is missing" % (
                i1 + 1, q(b[j1:j2]))
            head = ("oldText is missing %s at column %d of file line %d"
                    % (q(b[j1:j2]), i1 + 1, fl))
        if n == 0:
            f["headline"] = head
        detail.append(msg)
    if "headline" not in f:
        f["headline"] = "lines differ in an unidentified way (file line %d)" % fl
    return f, detail


def describe_ops(ops, old_lines, window, s, k):
    findings = []

    def missing(oi):
        a = old_lines[oi]
        if a == "" and oi == len(old_lines) - 1:
            h = ("oldText ends with a newline, but the closest file region ends "
                 "there (end of file without a trailing newline)")
        elif a.strip() == "":
            h = "oldText line %d is a blank line that the file does not have here" % (oi + 1)
        else:
            h = ("oldText line %d is not in the file at this position: %s"
                 % (oi + 1, q(clip(a), False)))
        return {"kind": "missing", "headline": h, "old_line": oi + 1,
                "file_line": None, "col_old": 1, "col_file": 1, "a": a, "b": None}

    def extra(fj):
        b = window[fj]
        if b.strip() == "":
            h = "the file has an extra blank line (file line %d) that oldText omits" % (s + fj + 1)
        else:
            h = ("the file has an extra line (file line %d) that oldText omits: %s"
                 % (s + fj + 1, q(clip(b), False)))
        return {"kind": "extra", "headline": h, "old_line": None,
                "file_line": s + fj + 1, "col_old": 1, "col_file": 1, "a": None, "b": b}

    for tag, i1, i2, j1, j2 in ops:
        if tag == "equal":
            continue
        if tag == "replace":
            n = min(i2 - i1, j2 - j1)
            for t in range(n):
                oi, fj = i1 + t, j1 + t
                lead = (oi == 0)
                trail = (oi == len(old_lines) - 1 and len(window) == k)
                f, det = explain_pair(oi, s + fj + 1, old_lines[oi], window[fj], lead, trail)
                f["detail"] = det
                findings.append(f)
            for t in range(n, i2 - i1):
                findings.append(missing(i1 + t))
            for t in range(n, j2 - j1):
                findings.append(extra(j1 + t))
        elif tag == "delete":
            for t in range(i1, i2):
                findings.append(missing(t))
        else:
            for t in range(j1, j2):
                findings.append(extra(t))
    for f in findings:
        f.setdefault("detail", [])
    return findings


def window_ops(old_lines, window):
    k = len(old_lines)
    old_cmp = list(old_lines)
    if old_cmp and window and line_matches(old_lines[0], window[0], True, k == 1):
        old_cmp[0] = window[0]
    if k > 1 and len(window) == k and line_matches(old_lines[-1], window[-1], False, True):
        old_cmp[-1] = window[-1]
    return old_cmp, difflib.SequenceMatcher(None, old_cmp, window, autojunk=False).get_opcodes()


def render_comparison(ops, old_lines, window, s):
    rows = []
    for tag, i1, i2, j1, j2 in ops:
        if tag == "equal":
            for t in range(j2 - j1):
                rows.append((" ", s + j1 + t + 1, window[j1 + t], False))
        else:
            for t in range(i1, i2):
                rows.append(("-", None, old_lines[t], True))
            for t in range(j1, j2):
                rows.append(("+", s + t + 1, window[t], True))
    keep = [False] * len(rows)
    for i, r in enumerate(rows):
        if r[3]:
            for d in range(-2, 3):
                if 0 <= i + d < len(rows):
                    keep[i + d] = True
    out, skipped = [], False
    for i, r in enumerate(rows):
        if keep[i]:
            out.append("  %s %5s | %s" % (r[0], r[1] if r[1] else "", vis(r[2])))
            skipped = False
        elif not skipped:
            out.append("      ..... | ...")
            skipped = True
    return out[:60]


def global_hints(doc, old, new):
    h, text = [], doc.text
    bad = sorted(set(c for c in old if (c in INVISIBLE or c in ODD_SPACES) and c not in text))
    if bad:
        h.append("oldText contains invisible/odd characters the file does not have: %s. "
                 "Retype the text or use --tolerant." % ", ".join(q(c) for c in bad))
    typo = sorted(set(c for c in old if ord(c) in FOLD and FOLD[ord(c)] != ""
                      and c not in text and c not in ODD_SPACES))
    if typo:
        h.append("oldText uses typographic characters absent from the file (%s); the "
                 "file probably has plain ASCII versions." % ", ".join(q(c) for c in typo))
    if ("\\n" in old or "\\t" in old) and "\n" not in old and "\\n" not in text:
        h.append("oldText contains a literal backslash-n / backslash-t but no real "
                 "newline: the JSON/shell escaping was probably applied twice.")
    nonblank = [l for l in old.split("\n") if l.strip()]
    if nonblank and all(re.match(r"^\s*\d+\s*(?:[:|\u2502\u2192\t])", l) for l in nonblank):
        h.append("every oldText line starts with a line number (like 12: or 12|): "
                 "those prefixes are display output and are not part of the file.")
    if "\t" in old and "\t" not in text:
        h.append("oldText contains tab characters but the file contains no tabs "
                 "(it is indented with spaces).")
    if "\t" in text and "\t" not in old and re.search(r"(?m)^ {2,}\S", old):
        h.append("the file uses tab indentation but oldText is indented with spaces.")
    if new and new in text and new != old:
        h.append("newText already exists in the file: this edit may have been applied "
                 "already (re-read the file).")
    if doc.eol_name != "LF" or doc.mixed:
        h.append("line endings are NOT the cause: the file uses %s%s and oldText line "
                 "endings are normalised before matching."
                 % (doc.eol_name, " (mixed)" if doc.mixed else ""))
    return h


def report_not_found(doc, idx, old, new, tolerant_on):
    old_lines = old.split("\n")
    k = len(old_lines)
    L, data = [], {"index": idx, "status": "not_found"}
    best = closest_window(doc.lines, old_lines)
    findings, cause = [], None
    if best is not None:
        s, e, score = best
        window = doc.lines[s:e]
        wtxt = "\n".join(window)
        if len(old) + len(wtxt) <= 20000:
            sim = difflib.SequenceMatcher(None, old, wtxt, autojunk=False).ratio()
        else:
            sim = score
        old_cmp, ops = window_ops(old_lines, window)
        findings = describe_ops(ops, old_lines, window, s, k)
    if best is None or sim < 0.50 or not findings:
        cause = ("no sufficiently similar text exists in the file (wrong file, or the "
                 "file changed since it was read)")
        data.update(cause=cause, closest=None)
        L.append("edits[%d]: oldText not found in %s" % (idx, doc.path))
        L.append("  Cause: " + cause)
        if best is not None:
            L.append("  Best partial guess: lines %d-%d (%d%% similar)" % (s + 1, e, int(sim * 100)))
    else:
        first = findings[0]
        cause = first["headline"]
        data.update(cause=cause, closest={"lines": [s + 1, e], "similarity": round(sim, 3)},
                    first_difference={"old_line": first["old_line"], "file_line": first["file_line"],
                                      "column": first["col_file"]})
        L.append("edits[%d]: oldText not found in %s" % (idx, doc.path))
        L.append("  Cause: " + cause)
        L.append("  Closest text in file: lines %d-%d (%d%% similar)" % (s + 1, e, int(sim * 100)))
        if first["a"] is not None and first["b"] is not None:
            lab_o = "oldText line %d" % first["old_line"]
            lab_f = "file    line %d" % first["file_line"]
            w = max(len(lab_o), len(lab_f))
            pre_o = len(vis_all(first["a"][:first["col_old"] - 1]))
            pre_f = len(vis_all(first["b"][:first["col_file"] - 1]))
            L.append("  First difference:")
            L.append("    %s | %s" % (lab_o.ljust(w), vis(first["a"])))
            L.append("    %s | %s^ column %d" % (" " * w, " " * pre_o, first["col_old"]))
            L.append("    %s | %s" % (lab_f.ljust(w), vis(first["b"])))
            L.append("    %s | %s^ column %d" % (" " * w, " " * pre_f, first["col_file"]))
        for d in first["detail"]:
            L.append("    - " + d)
        if len(findings) > 1:
            L.append("  Other differences (%d more):" % (len(findings) - 1))
            for f in findings[1:6]:
                L.append("    - " + f["headline"])
            if len(findings) > 6:
                L.append("    - ... and %d more" % (len(findings) - 6))
        L.append("  Comparison ('-' = oldText only, '+' = file only):")
        L.extend("  " + r for r in render_comparison(ops, old_lines, window, s))
    hints = global_hints(doc, old, new)
    if not tolerant_on and tolerant_spans(doc, old):
        hints.insert(0, "this would match with --tolerant (differences are only trailing "
                        "whitespace / typographic characters).")
    if hints:
        L.append("  Hints:")
        for h in hints:
            L.append("    * " + h)
    data["hints"] = hints
    data["report"] = "\n".join(L)
    return data["report"], data


def report_ambiguous(doc, idx, old, spans):
    where = [line_of(doc, a) for a, _ in spans]
    L = ["edits[%d]: oldText matches %d places in %s (lines %s). It must match exactly once."
         % (idx, len(spans), doc.path, ", ".join(str(w + 1) for w in where[:12])
            + (", ..." if len(where) > 12 else "")),
         "  Cause: not unique. Add neighbouring lines to oldText so only one place matches.",
         "  Surrounding context of each match:"]
    for n, (a, b) in enumerate(spans[:6]):
        sl, el = line_of(doc, a), line_of(doc, max(a, b - 1))
        prev_l = doc.lines[sl - 1] if sl > 0 else "(start of file)"
        next_l = doc.lines[el + 1] if el + 1 < len(doc.lines) else "(end of file)"
        L.append("    #%d lines %d-%d" % (n + 1, sl + 1, el + 1))
        L.append("        line before: %s" % vis(clip(prev_l)))
        L.append("        line after : %s" % vis(clip(next_l)))
    data = {"index": idx, "status": "ambiguous", "matches": [w + 1 for w in where],
            "cause": "oldText is not unique", "report": "\n".join(L)}
    return data["report"], data


# --------------------------------------------------------------------------- #
# Input parsing
# --------------------------------------------------------------------------- #
KEYS_OLD = ("oldText", "old_text", "oldString", "old_string", "old")
KEYS_NEW = ("newText", "new_text", "newString", "new_string", "new")


def norm_nl(s):
    return s.replace("\r\n", "\n").replace("\r", "\n")


def parse_edits(obj):
    if isinstance(obj, dict):
        items = obj["edits"] if "edits" in obj else [obj]
    else:
        items = obj
    if not isinstance(items, list) or not items:
        raise ToolError("edits must be a non-empty list of {oldText, newText} objects")
    out = []
    for i, it in enumerate(items):
        if not isinstance(it, dict):
            raise ToolError("edits[%d] must be an object with oldText/newText" % i)
        o = next((it[k] for k in KEYS_OLD if k in it), None)
        n = next((it[k] for k in KEYS_NEW if k in it), None)
        if o is None:
            raise ToolError("edits[%d]: missing 'oldText' (found keys: %s)" % (i, ", ".join(it) or "none"))
        if n is None:
            raise ToolError("edits[%d]: missing 'newText' (use \"\" to delete text)" % i)
        if not isinstance(o, str) or not isinstance(n, str):
            raise ToolError("edits[%d]: oldText and newText must be strings" % i)
        out.append((norm_nl(o), norm_nl(n)))
    return out


def read_text_arg(path):
    try:
        if path == "-":
            return sys.stdin.buffer.read().decode("utf-8-sig")
        with open(path, "rb") as f:
            return f.read().decode("utf-8-sig")
    except (OSError, UnicodeDecodeError) as e:
        raise ToolError("Cannot read %s: %s" % (path, e))


def parse_blocks(raw):
    """Escape-free format:  @@OLD / @@NEW / @@END marker lines, text copied verbatim."""
    edits, mode, old, new, n = [], None, [], [], 0
    for ln, line in enumerate(raw.replace("\r\n", "\n").replace("\r", "\n").split("\n"), 1):
        tag = line.rstrip()
        if tag == "@@OLD":
            if mode is not None:
                raise ToolError("block format, line %d: @@OLD inside an unfinished edit[%d] "
                                "(missing @@END?)" % (ln, n))
            mode, old, new = "old", [], []
        elif tag == "@@NEW":
            if mode != "old":
                raise ToolError("block format, line %d: @@NEW without a preceding @@OLD" % ln)
            mode = "new"
        elif tag == "@@END":
            if mode != "new":
                raise ToolError("block format, line %d: @@END before @@NEW" % ln)
            edits.append(("\n".join(old), "\n".join(new)))
            n += 1
            mode = None
        elif mode == "old":
            old.append(line)
        elif mode == "new":
            new.append(line)
        elif line.strip():
            raise ToolError("block format, line %d: text outside @@OLD/@@NEW/@@END: %r"
                            % (ln, clip(line, 40)))
    if mode is not None:
        raise ToolError("block format: edit[%d] is not closed with @@END" % n)
    if not edits:
        raise ToolError("block format: no edits found")
    return edits


def collect_edits(args):
    file_from_json = None
    if args.edits:
        raw = read_text_arg(args.edits)
        if raw.lstrip().startswith("@@OLD"):
            return parse_blocks(raw), None
        try:
            obj = json.loads(raw)
        except json.JSONDecodeError as e:
            raise ToolError("Invalid JSON in %s: %s (line %d, column %d). Backslashes (Windows "
                            "paths!) are the usual cause: JSON needs \\\\ for each one and tool/shell "
                            "layers often eat them. Use the escape-free block format "
                            "instead (@@OLD / @@NEW / @@END, see --help)."
                            % (args.edits if args.edits != "-" else "stdin", e.msg, e.lineno, e.colno))
        if isinstance(obj, dict) and isinstance(obj.get("path"), str):
            file_from_json = obj["path"]
        return parse_edits(obj), file_from_json
    old = read_text_arg(args.old_file) if args.old_file else args.old
    new = read_text_arg(args.new_file) if args.new_file else args.new
    if old is None or new is None:
        raise ToolError("Provide --edits, or both (--old | --old-file) and (--new | --new-file).")
    return [(norm_nl(old), norm_nl(new))], None


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def apply_edits(doc, edits, args):
    spans, results, errors = [], [], []
    for idx, (old, new) in enumerate(edits):
        if old == "":
            data = {"index": idx, "status": "empty_old", "cause": "oldText is empty",
                    "report": "edits[%d]: oldText is empty, so there is nothing to locate. "
                              "(To create or overwrite a whole file, write it directly.)" % idx}
            errors.append(data)
            continue
        found, mode = locate(doc, old, args.tolerant)
        if not found:
            _, data = report_not_found(doc, idx, old, new, args.tolerant)
            errors.append(data)
        elif len(found) > 1:
            _, data = report_ambiguous(doc, idx, old, found)
            errors.append(data)
        else:
            a, b = found[0]
            info = {"index": idx, "status": "ok", "mode": mode,
                    "lines": [line_of(doc, a) + 1, line_of(doc, max(a, b - 1)) + 1]}
            if mode == "tolerant":
                s, e = line_of(doc, a), line_of(doc, max(a, b - 1)) + 1
                window = doc.lines[s:e]
                ol = old.split("\n")
                if ol and ol[-1] == "" and len(ol) > 1:
                    ol = ol[:-1]
                _, ops = window_ops(ol, window)
                info["tolerated"] = [f["headline"] for f in describe_ops(ops, ol, window, s, len(ol))]
            spans.append((a, b, new, idx))
            results.append(info)
    # overlap check
    ordered = sorted(spans, key=lambda x: x[0])
    for p, c in zip(ordered, ordered[1:]):
        if p[1] > c[0]:
            lp = (line_of(doc, p[0]) + 1, line_of(doc, max(p[0], p[1] - 1)) + 1)
            lc = (line_of(doc, c[0]) + 1, line_of(doc, max(c[0], c[1] - 1)) + 1)
            msg = ("edits[%d] (lines %d-%d) and edits[%d] (lines %d-%d) overlap. Merge them into "
                   "one edit." % (p[3], lp[0], lp[1], c[3], lc[0], lc[1]))
            errors.append({"index": c[3], "status": "overlap", "cause": "overlapping edits",
                           "report": msg})
    return ordered, results, errors


def main(argv=None):
    ap = argparse.ArgumentParser(description="Exact-text file editor with detailed diagnostics.",
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__)
    ap.add_argument("file", nargs="?", help="file to edit (optional if the JSON has \"path\")")
    ap.add_argument("--edits", help="JSON file with edits, or - for stdin")
    ap.add_argument("--old"), ap.add_argument("--new")
    ap.add_argument("--old-file"), ap.add_argument("--new-file")
    ap.add_argument("--tolerant", action="store_true")
    ap.add_argument("--eol", choices=["keep", "lf", "crlf"], default="keep")
    ap.add_argument("--encoding", default="utf-8")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--backup", action="store_true")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument("--ascii", action="store_true", help="ASCII-only whitespace markers")
    ap.add_argument("--no-diff", action="store_true")
    args = ap.parse_args(argv)

    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass
    if args.ascii:
        SYM.clear()
        SYM.update(SYM_ASCII)

    try:
        edits, json_path = collect_edits(args)
        path = args.file or json_path
        if not path:
            raise ToolError("No file given (pass FILE or include \"path\" in the JSON).")
        doc = load_doc(path, args.encoding)
        ordered, results, errors = apply_edits(doc, edits, args)
    except ToolError as e:
        if args.json:
            print(json.dumps({"ok": False, "error": str(e)}, indent=2))
        else:
            print("ERROR: %s" % e)
        return 2

    meta = "%s%s, %s%s" % (doc.eol_name, " (mixed: %s)" % ", ".join(
        "%s=%d" % kv for kv in doc.counts.items() if kv[1]) if doc.mixed else "",
        doc.enc, " +BOM" if doc.bom else "")

    if errors:
        errors.sort(key=lambda d: d["index"])
        if args.json:
            print(json.dumps({"ok": False, "path": path, "file_format": meta,
                              "errors": errors, "applied": []}, indent=2))
        else:
            print("FAILED: %d of %d edit(s) could not be applied. Nothing was written. [%s]\n"
                  % (len(errors), len(edits), meta))
            for d in errors:
                print(d["report"])
                print()
        return 1

    new_text = doc.text
    for a, b, new, _ in reversed(ordered):
        new_text = new_text[:a] + new + new_text[b:]
    if new_text == doc.text:
        msg = "No changes: every newText is identical to its oldText."
        print(json.dumps({"ok": False, "error": msg}) if args.json else "ERROR: " + msg)
        return 1
    out_text = rebuild(doc, new_text, args.eol)
    out_bytes = doc.bom + out_text.encode(doc.enc)
    if not args.dry_run:
        try:
            write_atomic(path, out_bytes, doc.raw if args.backup else None)
        except ToolError as e:
            print(json.dumps({"ok": False, "error": str(e)}) if args.json else "ERROR: %s" % e)
            return 2

    diff = list(difflib.unified_diff(doc.lines, new_text.split("\n"),
                                     "a/" + os.path.basename(path),
                                     "b/" + os.path.basename(path), n=2, lineterm=""))
    eol_note = ""
    if args.eol != "keep" and args.eol.upper() != doc.eol_name:
        eol_note = "line endings converted %s -> %s" % (doc.eol_name, args.eol.upper())
    if args.json:
        print(json.dumps({"ok": True, "dry_run": args.dry_run, "path": path, "file_format": meta,
                          "applied": sorted(results, key=lambda r: r["index"]),
                          "eol_note": eol_note, "diff": "\n".join(diff)}, indent=2))
        return 0
    print("%s %s  [%s]  %d edit(s)" % ("DRY RUN (not written):" if args.dry_run else "OK:",
                                       path, meta, len(edits)))
    for r in sorted(results, key=lambda r: r["index"]):
        line = "  edits[%d]: lines %d-%d replaced (%s match)" % (
            r["index"], r["lines"][0], r["lines"][1], r["mode"])
        print(line)
        for t in r.get("tolerated", []):
            print("      tolerated: " + t)
    if doc.mixed and args.eol == "keep":
        print("  note: file has mixed line endings; untouched lines kept their original ending, "
              "new lines use %s." % doc.eol_name)
    if eol_note:
        print("  note: " + eol_note)
    if not args.no_diff:
        print()
        print("\n".join(diff[:300]))
        if len(diff) > 300:
            print("... (%d more diff lines)" % (len(diff) - 300))
    return 0


if __name__ == "__main__":
    sys.exit(main())