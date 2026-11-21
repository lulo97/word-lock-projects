"""Generate pronunciation audio for every example sentence in the database.

Usage:
    python generate_audio_examples.py
    python generate_audio_examples.py --word abundant
    python generate_audio_examples.py --verify
    python generate_audio_examples.py --force

For each example in database/<letter>/<word>.json this script:
  1. creates database/<letter>/audios_example/
  2. asks the local llama.cpp TTS server for a WAV
  3. converts the WAV to OGG with ffmpeg
  4. stores the relative path in the example's "audio" field

The OGG file name is the normalised sentence, e.g.
"we are here" -> "we_are_here.ogg".

Running it again skips examples that already have a valid audio file.
"""

import argparse
import hashlib
import json
import re
import sys
import time
from pathlib import Path

import generate_audio as word_audio
from generate_audio import (
    DATABASE_DIR,
    AudioError,
    safe_console,
)

AUDIO_DIR_NAME = "audios_example"
MAX_STEM_LENGTH = 100
DEFAULT_N_PREDICT = 512


def normalize_filename(text):
    """Turn a sentence into a Windows-safe file stem.

    "We are here." -> "we_are_here"
    """
    lowered = str(text).strip().lower()
    stem = re.sub(r"[^a-z0-9]+", "_", lowered).strip("_")
    if not stem:
        stem = "example"
    if len(stem) > MAX_STEM_LENGTH:
        digest = hashlib.sha1(lowered.encode("utf-8")).hexdigest()[:8]
        stem = stem[:MAX_STEM_LENGTH].rstrip("_") + "_" + digest
    return stem


def example_text(example):
    if not isinstance(example, dict):
        return ""
    return str(example.get("english", "")).strip()


def example_audio_value(example):
    if not isinstance(example, dict):
        return ""
    return str(example.get("audio", "")).strip()


def resolve_audio(value):
    if not value:
        return None
    path = Path(value)
    if not path.is_absolute():
        path = DATABASE_DIR / path
    return path


def relative_audio_path(path):
    return path.relative_to(DATABASE_DIR).as_posix()


def save_json(path, entry):
    temp = path.with_suffix(".json.tmp")
    with open(temp, "w", encoding="utf-8") as handle:
        json.dump(entry, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    temp.replace(path)


def set_example_audio(json_path, entry, example, audio_path):
    """Set example["audio"] and save the JSON when it changed."""
    relative = relative_audio_path(audio_path)
    if example.get("audio") == relative:
        return False
    example["audio"] = relative
    save_json(json_path, entry)
    return True


def iter_examples(entries):
    """Yield (json_path, entry, index, example, text, target_ogg)."""
    for json_path, entry in entries:
        examples = entry.get("examples")
        if not isinstance(examples, list):
            continue
        audio_dir = json_path.parent / AUDIO_DIR_NAME
        for index, example in enumerate(examples):
            text = example_text(example)
            if not text:
                continue
            target = audio_dir / (normalize_filename(text) + ".ogg")
            yield json_path, entry, index, example, text, target


def process_example(json_path, entry, example, text, target_ogg, args, ffmpeg):
    audio_dir = target_ogg.parent
    audio_dir.mkdir(parents=True, exist_ok=True)
    target_wav = audio_dir / (target_ogg.stem + ".wav")

    existing = resolve_audio(example_audio_value(example))
    if not args.force and word_audio.is_audio_file(existing):
        try:
            shown = existing.relative_to(DATABASE_DIR).as_posix()
        except ValueError:
            shown = str(existing)
        print(f"  SKIP {target_ogg.name}: already has {shown}")
        return "skipped"

    if not args.force and word_audio.is_audio_file(target_ogg):
        changed = set_example_audio(json_path, entry, example, target_ogg)
        suffix = " (linked existing file)" if changed else ""
        print(f"  SKIP {target_ogg.name}: already exists{suffix}")
        return "skipped"

    if not args.force and word_audio.is_audio_file(target_wav):
        print(f"  Reusing existing WAV for {target_ogg.name}")
    else:
        print(f"  Generating TTS for {target_ogg.name}...")
        word_audio.generate_wav(text, target_wav, args)

    print(f"  Converting {target_ogg.name} to OGG...")
    word_audio.convert_to_ogg(target_wav, target_ogg, ffmpeg)
    set_example_audio(json_path, entry, example, target_ogg)

    if not args.keep_wav:
        try:
            target_wav.unlink()
        except OSError:
            pass

    print(f"  SAVED {target_ogg.name}: {relative_audio_path(target_ogg)}")
    return "generated"


def collect_entries(wanted):
    entries = []
    for path, entry in word_audio.iter_entries():
        word = word_audio.word_of(entry, path)
        if not wanted or word in wanted:
            entries.append((path, entry))
    return entries


def verify(entries):
    """Report examples that have no usable OGG file. Returns the missing list."""
    total = 0
    missing = []
    for json_path, entry in entries:
        for _, _, index, example, text, target_ogg in iter_examples([(json_path, entry)]):
            total += 1
            if word_audio.is_audio_file(target_ogg):
                continue
            missing.append((word_audio.word_of(entry, json_path), index, text, target_ogg))
    print("-" * 72)
    if not missing:
        print(f"CHECK OK: all {total} examples have audio files.")
        return missing
    print(f"CHECK FAILED: {len(missing)} of {total} examples have no audio file:")
    for word, index, text, target in missing:
        print(f"  - {word} example[{index}] -> {relative_audio_path(target)}")
        print(f"      {text}")
    return missing


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate OGG audio for Word Lock example sentences."
    )
    parser.add_argument(
        "--endpoint",
        default=word_audio.DEFAULT_ENDPOINT,
        help=f"llama.cpp server base URL (default: {word_audio.DEFAULT_ENDPOINT}).",
    )
    parser.add_argument("--voice", default="female", help="TTS voice (default: female).")
    parser.add_argument("--lang", default="en", help="TTS language (default: en).")
    parser.add_argument(
        "--n-predict",
        type=int,
        default=DEFAULT_N_PREDICT,
        help=f"Maximum audio tokens per sentence (default: {DEFAULT_N_PREDICT}).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=300.0,
        help="Request timeout in seconds (default: 300).",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=1,
        help="Extra TTS attempts after a failed request (default: 1).",
    )
    parser.add_argument(
        "--word",
        action="append",
        default=[],
        help="Only process examples of this word (can be repeated).",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Regenerate audio even when files already exist.",
    )
    parser.add_argument(
        "--keep-wav",
        action="store_true",
        help="Keep the intermediate WAV files next to the OGG files.",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=0.0,
        help="Seconds to wait between generated examples (default: 0).",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Only check that every example has an audio file; generate nothing.",
    )
    return parser.parse_args(argv)


def run(args):
    wanted = {word.strip().lower() for word in args.word if word.strip()}
    entries = collect_entries(wanted)

    if wanted:
        found = {word_audio.word_of(entry, path) for path, entry in entries}
        for missing in sorted(wanted - found):
            print(f"WARNING: word not found in database: {missing}")

    if args.verify:
        print(f"Database: {DATABASE_DIR}")
        print(f"Words checked: {len(entries)}")
        return 1 if verify(entries) else 0

    ffmpeg = word_audio.find_ffmpeg()
    if ffmpeg is None:
        print("ERROR: ffmpeg was not found in PATH.")
        return 1

    try:
        word_audio.check_server(args.endpoint, min(args.timeout, 15.0))
    except AudioError as exc:
        print(f"ERROR: {exc}")
        return 1

    jobs = list(iter_examples(entries))
    print(f"Database: {DATABASE_DIR}")
    print(f"Words: {len(entries)}  Examples to process: {len(jobs)}")
    print("-" * 72)

    generated = 0
    skipped = 0
    failed = []

    for index, (json_path, entry, example_index, example, text, target) in enumerate(
        jobs, 1
    ):
        word = word_audio.word_of(entry, json_path)
        print(f"[{index}/{len(jobs)}] {word} example[{example_index}]")
        try:
            result = process_example(
                json_path, entry, example, text, target, args, ffmpeg
            )
        except (AudioError, OSError) as exc:
            print(f"  FAILED {target.name}: {exc}")
            failed.append((word, target.name, str(exc)))
            continue

        if result == "generated":
            generated += 1
            if args.delay > 0 and index < len(jobs):
                time.sleep(args.delay)
        else:
            skipped += 1

    print("-" * 72)
    print(
        f"Done: {generated} generated, {skipped} skipped, {len(failed)} failed"
    )
    if failed:
        print("Failed examples:")
        for word, name, error in failed:
            print(f"  - {word} / {name}: {error}")

    missing = verify(entries)
    if failed or missing:
        return 1
    return 0


def main(argv=None):
    safe_console()
    args = parse_args(argv)
    if args.retries < 0:
        args.retries = 0
    try:
        return run(args)
    except KeyboardInterrupt:
        print("\nInterrupted.")
        return 130


if __name__ == "__main__":
    sys.exit(main())
