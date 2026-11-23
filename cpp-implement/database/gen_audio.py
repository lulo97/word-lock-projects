#!/usr/bin/env python3
"""Generate OGG audio for Kotoba words and example sentences.

Input:  database/words/**/*.json
Output:
    database/audio/<word>/word.ogg
    database/audio/<word>/ex_<n>.ogg
and the relative paths are written back into the JSON ("audio" fields).

The script is a manual, offline tool (llama.cpp TTS on :8080 + ffmpeg). It is
not imported or tested by the C++ app.

Usage:
    python database/gen_audio.py
    python database/gen_audio.py --word-mode carrier
    python database/gen_audio.py --force --only-word りんご

Single words are often pronounced badly in isolation. `--word-mode carrier`
synthesizes the word inside a short phrase ("えっと。<word>。えっと。") and keeps
the middle speech segment. Example sentences are always synthesized directly.
Every clip is loudness-normalized with ffmpeg loudnorm and encoded to OGG/Opus
(~40 kbit/s mono) instead of WAV so the database stays small. The C++ player
transcodes OGG to a temporary WAV through ffmpeg at playback time.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
import unicodedata
from pathlib import Path

from llm_client import LlmError, check_audio_server, request_tts

# Japanese filenames/words crash a cp1252 console without this.
for stream in (sys.stdout, sys.stderr):
    try:
        stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

ROOT = Path(__file__).resolve().parent.parent
DATABASE_DIR = ROOT / "database"
WORDS_DIR = DATABASE_DIR / "words"
AUDIO_DIR = DATABASE_DIR / "audio"
ERROR_LOG = DATABASE_DIR / "gen_audio.errors.log"

DEFAULT_ENDPOINT = "http://127.0.0.1:8080"
DEFAULT_CARRIER = "えっと。{word}。えっと。"
MIN_AUDIO_BYTES = 1024
LOUDNORM = "loudnorm=I=-16:TP=-1.5:LRA=11"
AUDIO_EXTENSION = ".ogg"
OGG_BITRATE = "40k"   # mono speech; a fraction of the original WAV size

DURATION_RE = re.compile(r"Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?)")
SILENCE_START_RE = re.compile(r"silence_start:\s*(-?\d+(?:\.\d+)?)")
SILENCE_END_RE = re.compile(r"silence_end:\s*(-?\d+(?:\.\d+)?)")


class AudioError(Exception):
    """TTS or ffmpeg failed for one clip."""


def normalize_word(value: str) -> str:
    return unicodedata.normalize("NFC", str(value)).strip()


def word_files() -> list[Path]:
    if not WORDS_DIR.is_dir():
        return []
    return sorted(WORDS_DIR.rglob("*.json"))


def entity_text(entry: dict) -> str:
    """The Japanese word to speak (kanji when present, else reading/kana)."""
    for key in ("word", "kanji", "reading", "hiragana"):
        value = entry.get(key)
        if isinstance(value, str) and value.strip():
            return value.strip()
    return ""


def example_text(example) -> str:
    if not isinstance(example, dict):
        return ""
    for key in ("jp", "ja"):
        value = example.get(key)
        if isinstance(value, str) and value.strip():
            return value.strip()
    return ""


def audio_dir_for(word: str) -> Path:
    return AUDIO_DIR / word


def relative_path(path: Path) -> str:
    return path.relative_to(DATABASE_DIR).as_posix()


def is_audio_file(path: Path | None) -> bool:
    return path is not None and path.is_file() and path.stat().st_size > MIN_AUDIO_BYTES


def resolve_audio(value) -> Path | None:
    if not isinstance(value, str) or not value.strip():
        return None
    path = Path(value.strip())
    return path if path.is_absolute() else DATABASE_DIR / path


def find_ffmpeg() -> str | None:
    found = shutil.which("ffmpeg")
    if found:
        return found
    for name in ("ffmpeg.exe", "ffmpeg"):
        candidate = Path(name)
        if candidate.is_file():
            return str(candidate.resolve())
    return None


def run_ffmpeg(ffmpeg: str, args: list[str], *, capture: bool = False,
               loglevel: str = "error") -> str:
    command = [ffmpeg, "-y", "-hide_banner", "-loglevel", loglevel, *args]
    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as error:
        raise AudioError(f"cannot run ffmpeg: {error}") from error
    if result.returncode != 0:
        detail = (result.stderr or "").strip() or f"exit code {result.returncode}"
        raise AudioError(f"ffmpeg failed: {detail[-500:]}")
    return result.stderr if capture else ""


def audio_duration(ffmpeg: str, wav: Path) -> float:
    """Read the duration from ffmpeg's banner (no ffprobe dependency)."""
    stderr = run_ffmpeg(
        ffmpeg,
        ["-i", str(wav), "-f", "null", "-"],
        capture=True,
        loglevel="info",   # the duration banner is printed at info level
    )
    match = DURATION_RE.search(stderr)
    if not match:
        raise AudioError("cannot determine audio duration")
    hours, minutes, seconds = match.groups()
    return int(hours) * 3600 + int(minutes) * 60 + float(seconds)


def speech_segments(ffmpeg: str, wav: Path) -> list[tuple[float, float]]:
    """Return non-silent intervals via ffmpeg silencedetect."""
    total = audio_duration(ffmpeg, wav)
    stderr = run_ffmpeg(
        ffmpeg,
        [
            "-i", str(wav),
            "-af", "silencedetect=noise=-35dB:d=0.25",
            "-f", "null", "-",
        ],
        capture=True,
        loglevel="info",   # silencedetect logs are printed at info level
    )
    silences: list[tuple[float, float]] = []
    start: float | None = None
    for line in stderr.splitlines():
        if match := SILENCE_START_RE.search(line):
            start = max(0.0, float(match.group(1)))
        elif match := SILENCE_END_RE.search(line):
            if start is not None:
                silences.append((start, min(total, float(match.group(1)))))
                start = None
    if start is not None:
        silences.append((start, total))

    segments: list[tuple[float, float]] = []
    cursor = 0.0
    for begin, end in silences:
        if begin > cursor + 0.01:
            segments.append((cursor, begin))
        cursor = max(cursor, end)
    if total > cursor + 0.01:
        segments.append((cursor, total))
    return segments


def pick_word_segment(segments: list[tuple[float, float]]) -> tuple[float, float] | None:
    """Pick the carrier's middle speech segment (the word itself)."""
    if not segments:
        return None
    if len(segments) >= 3:
        return segments[1]
    if len(segments) == 2:
        return max(segments, key=lambda segment: segment[1] - segment[0])
    return segments[0]


def write_ogg(ffmpeg: str, raw: Path, target: Path,
              segment: tuple[float, float] | None = None) -> None:
    """Normalize loudness, optionally trim, then encode to OGG/Opus."""
    target.parent.mkdir(parents=True, exist_ok=True)
    temp = target.with_name(target.name + ".tmp" + AUDIO_EXTENSION)
    args = ["-i", str(raw)]
    if segment is not None:
        begin, end = segment
        args += ["-ss", f"{begin:.3f}", "-t", f"{max(0.05, end - begin):.3f}"]
    args += [
        "-af", LOUDNORM,
        "-ar", "48000",   # Opus always decodes at 48 kHz
        "-ac", "1",
        "-c:a", "libopus",
        "-b:a", OGG_BITRATE,
        str(temp),
    ]
    run_ffmpeg(ffmpeg, args)
    if not is_audio_file(temp):
        temp.unlink(missing_ok=True)
        raise AudioError("ffmpeg produced an empty OGG file")
    os.replace(temp, target)


def transcode_to_ogg(ffmpeg: str, source: Path, target: Path) -> None:
    """Re-encode an existing clip (e.g. legacy .wav) into the OGG target."""
    target.parent.mkdir(parents=True, exist_ok=True)
    temp = target.with_name(target.name + ".tmp" + AUDIO_EXTENSION)
    run_ffmpeg(
        ffmpeg,
        [
            "-i", str(source),
            "-af", LOUDNORM,
            "-ar", "48000",
            "-ac", "1",
            "-c:a", "libopus",
            "-b:a", OGG_BITRATE,
            str(temp),
        ],
    )
    if not is_audio_file(temp):
        temp.unlink(missing_ok=True)
        raise AudioError("ffmpeg produced an empty OGG file")
    os.replace(temp, target)
    # Drop the superseded file when it is a legacy WAV next to the new clip.
    if source.parent == target.parent and source.name != target.name:
        source.unlink(missing_ok=True)


def generate_clip(text: str, target: Path, args, ffmpeg: str, *,
                  carrier: bool) -> None:
    """Request TTS for `text`, optionally trim a carrier, then normalize."""
    spoken = args.carrier_template.replace("{word}", text) if carrier else text
    last_error: Exception | None = None
    for attempt in range(1, args.retries + 2):
        if attempt > 1:
            print(f"  retry {attempt - 1}/{args.retries}...")
        raw = target.with_name(target.stem + ".raw.wav")
        try:
            wav = request_tts(
                args.endpoint,
                spoken,
                lang=args.lang,
                voice=args.voice,
                n_predict=args.n_predict,
                timeout=args.timeout,
            )
            raw.parent.mkdir(parents=True, exist_ok=True)
            raw.write_bytes(wav)
            segment = None
            if carrier:
                segment = pick_word_segment(speech_segments(ffmpeg, raw))
                if segment is None:
                    print("  warning: no speech segment found; keeping full clip")
            write_ogg(ffmpeg, raw, target, segment)
            raw.unlink(missing_ok=True)
            return
        except (LlmError, AudioError, OSError) as error:
            last_error = error
            print(f"  error: {error}")
        finally:
            raw = target.with_name(target.stem + ".raw.wav")
            raw.unlink(missing_ok=True)
    raise AudioError(str(last_error or "audio generation failed"))


def save_json(path: Path, entry: dict) -> None:
    temp = path.with_name(path.name + ".tmp")
    temp.write_text(
        json.dumps(entry, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    os.replace(temp, path)


def set_field(container: dict, key: str, value: str) -> bool:
    if container.get(key) == value:
        return False
    container[key] = value
    return True


def process_entry(path: Path, entry: dict, args, ffmpeg: str) -> tuple[int, int]:
    """Generate one word + its examples. Returns (generated, skipped)."""
    word = normalize_word(entity_text(entry))
    if not word:
        print(f"  SKIP {path.name}: no word/kanji field")
        return 0, 0

    target_dir = audio_dir_for(word)
    generated = skipped = 0
    changed = False

    word_target = target_dir / ("word" + AUDIO_EXTENSION)
    existing = resolve_audio(entry.get("audio"))
    if not args.force and is_audio_file(word_target):
        print(f"  word: already has {relative_path(word_target)}")
        skipped += 1
    elif not args.force and is_audio_file(existing) and existing != word_target:
        print(f"  word: converting {relative_path(existing)} -> "
              f"{relative_path(word_target)}")
        transcode_to_ogg(ffmpeg, existing, word_target)
        generated += 1
    else:
        print(f"  word: generating {relative_path(word_target)}")
        generate_clip(word, word_target, args, ffmpeg,
                      carrier=args.word_mode == "carrier")
        generated += 1
    changed |= set_field(entry, "audio", relative_path(word_target))

    examples = entry.get("examples")
    if isinstance(examples, list):
        for index, example in enumerate(examples, start=1):
            if not isinstance(example, dict):
                continue
            text = example_text(example)
            if not text:
                continue
            target = target_dir / f"ex_{index}{AUDIO_EXTENSION}"
            existing = resolve_audio(example.get("audio"))
            if not args.force and is_audio_file(target):
                print(f"  ex_{index}: already has {relative_path(target)}")
                skipped += 1
            elif not args.force and is_audio_file(existing) and existing != target:
                print(f"  ex_{index}: converting {relative_path(existing)} -> "
                      f"{relative_path(target)}")
                transcode_to_ogg(ffmpeg, existing, target)
                generated += 1
            else:
                print(f"  ex_{index}: generating {relative_path(target)}")
                try:
                    generate_clip(text, target, args, ffmpeg, carrier=False)
                except AudioError as error:
                    raise AudioError(f"example {index}: {error}") from error
                generated += 1
            changed |= set_field(example, "audio", relative_path(target))

    if changed:
        save_json(path, entry)
    return generated, skipped


def log_failure(name: str, message: str) -> None:
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with ERROR_LOG.open("a", encoding="utf-8") as log:
        log.write(f"[{stamp}] {name}: {message}\n")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate OGG audio for Kotoba words and examples."
    )
    parser.add_argument("--endpoint", default=DEFAULT_ENDPOINT,
                        help=f"llama.cpp TTS base URL (default: {DEFAULT_ENDPOINT})")
    parser.add_argument("--voice", default="female",
                        help="TTS voice (default: female)")
    parser.add_argument("--lang", default="ja",
                        help="TTS language (default: ja)")
    parser.add_argument("--n-predict", type=int, default=128,
                        help="max audio tokens per clip (default: 128)")
    parser.add_argument("--timeout", type=float, default=300.0,
                        help="request timeout in seconds (default: 300)")
    parser.add_argument("--retries", type=int, default=1,
                        help="extra TTS attempts after a failure (default: 1)")
    parser.add_argument("--word-mode", choices=("direct", "carrier"),
                        default="direct",
                        help="single-word synthesis: direct or carrier "
                             "(word inside a phrase, trimmed)")
    parser.add_argument("--carrier-template", default=DEFAULT_CARRIER,
                        help=f'carrier phrase with {{word}} (default: "{DEFAULT_CARRIER}")')
    parser.add_argument("--only-word", action="append", default=[],
                        metavar="WORD",
                        help="only process this word (may be repeated)")
    parser.add_argument("--delay", type=float, default=0.0,
                        help="seconds between entries (default: 0)")
    parser.add_argument("--force", action="store_true",
                        help="regenerate even when non-empty audio exists")
    return parser.parse_args(argv)


def run(args: argparse.Namespace) -> int:
    ffmpeg = find_ffmpeg()
    if ffmpeg is None:
        print("ERROR: ffmpeg was not found in PATH.")
        return 2

    if args.retries < 0:
        args.retries = 0

    try:
        check_audio_server(args.endpoint, timeout=min(args.timeout, 15.0))
    except LlmError as error:
        print(f"ERROR: {error}")
        return 2

    wanted = {normalize_word(word) for word in args.only_word if word.strip()}
    entries: list[tuple[Path, dict]] = []
    for path in word_files():
        try:
            entry = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError) as error:
            print(f"SKIP {path.name}: cannot read JSON ({error})")
            continue
        if not isinstance(entry, dict):
            print(f"SKIP {path.name}: not a word object")
            continue
        if wanted and normalize_word(entity_text(entry)) not in wanted:
            continue
        entries.append((path, entry))

    print(f"Database: {DATABASE_DIR}")
    print(f"Words:    {len(entries)}")
    print("-" * 72)

    generated = skipped = 0
    failed: list[tuple[str, str]] = []
    for index, (path, entry) in enumerate(entries, start=1):
        name = entity_text(entry) or path.stem
        print(f"[{index}/{len(entries)}] {name}")
        try:
            made, skipped_here = process_entry(path, entry, args, ffmpeg)
            generated += made
            skipped += skipped_here
        except (AudioError, OSError) as error:
            print(f"  FAILED: {error}")
            log_failure(name, str(error))
            failed.append((name, str(error)))
        if args.delay > 0 and index < len(entries):
            time.sleep(args.delay)

    print("-" * 72)
    print(
        f"Done: {generated} generated, {skipped} skipped, {len(failed)} failed"
    )
    if failed:
        print("Failed words:")
        for name, message in failed:
            print(f"  - {name}: {message}")
        print(f"Details: {ERROR_LOG.relative_to(ROOT).as_posix()}")
        return 1
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        return run(args)
    except KeyboardInterrupt:
        print("\nInterrupted.")
        return 130


if __name__ == "__main__":
    sys.exit(main())
