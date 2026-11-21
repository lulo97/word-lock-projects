"""Generate pronunciation audio for every word in the database.

Usage:
    python generate_audio.py
    python generate_audio.py --word apple
    python generate_audio.py --force

For each database/<letter>/<word>.json this script:
  1. creates database/<letter>/audios/
  2. asks the local llama.cpp TTS server for a WAV
  3. converts the WAV to OGG with ffmpeg
  4. stores the relative path in pronunciation.audio

Running it again skips words that already have a valid audio file.
"""

import argparse
import json
import shutil
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

DATABASE_DIR = Path(__file__).resolve().parent
DEFAULT_ENDPOINT = "http://127.0.0.1:8080"


class AudioError(Exception):
    """A recoverable audio generation failure for one word."""


def safe_console():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def iter_entries():
    """Yield (json_path, entry) for every word file in the database."""
    for path in sorted(DATABASE_DIR.rglob("*.json")):
        try:
            with open(path, encoding="utf-8") as handle:
                entry = json.load(handle)
        except (OSError, ValueError) as exc:
            print(f"  SKIP {path.name}: cannot read JSON ({exc})")
            continue
        if not isinstance(entry, dict) or not str(entry.get("word", "")).strip():
            print(f"  SKIP {path.name}: not a word entry")
            continue
        yield path, entry


def word_of(entry, path):
    word = str(entry.get("word", "")).strip().lower()
    return word or path.stem.strip().lower()


def audio_value(entry):
    pronunciation = entry.get("pronunciation")
    if not isinstance(pronunciation, dict):
        return ""
    return str(pronunciation.get("audio", "")).strip()


def resolve_audio(value):
    if not value:
        return None
    path = Path(value)
    if not path.is_absolute():
        path = DATABASE_DIR / path
    return path


def is_audio_file(path):
    return path is not None and path.is_file() and path.stat().st_size > 0


def relative_audio_path(path):
    return path.relative_to(DATABASE_DIR).as_posix()


def save_json(path, entry):
    temp = path.with_suffix(".json.tmp")
    with open(temp, "w", encoding="utf-8") as handle:
        json.dump(entry, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    temp.replace(path)


def set_audio_field(json_path, entry, audio_path):
    """Set pronunciation.audio if it changed, and save the JSON."""
    relative = relative_audio_path(audio_path)
    pronunciation = entry.get("pronunciation")
    if not isinstance(pronunciation, dict):
        pronunciation = {}
        entry["pronunciation"] = pronunciation
    if pronunciation.get("audio") == relative:
        return False
    pronunciation["audio"] = relative
    save_json(json_path, entry)
    return True


def tts_url(endpoint):
    endpoint = endpoint.rstrip("/")
    return endpoint if endpoint.endswith("/tts") else endpoint + "/tts"


def props_url(endpoint):
    endpoint = endpoint.rstrip("/")
    if endpoint.endswith("/tts"):
        endpoint = endpoint[:-4]
    if endpoint.endswith("/v1"):
        endpoint = endpoint[:-3]
    return endpoint + "/props"


def check_server(endpoint, timeout):
    """Fail early with a helpful message when the server lacks audio support."""
    request = urllib.request.Request(props_url(endpoint), method="GET")
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            data = json.loads(response.read().decode("utf-8", errors="replace"))
    except (urllib.error.URLError, socket.timeout, TimeoutError, ValueError) as exc:
        raise AudioError(f"cannot reach llama.cpp at {endpoint}: {exc}") from exc

    modalities = data.get("modalities") if isinstance(data, dict) else None
    if not isinstance(modalities, dict) or not modalities.get("audio"):
        raise AudioError(
            "llama.cpp has no audio modality. Start llama-server with --mmproj."
        )


def request_tts(word, args):
    payload = {
        "input": word,
        "lang": args.lang,
        "voice": args.voice,
        "n_predict": args.n_predict,
    }
    request = urllib.request.Request(
        tts_url(args.endpoint),
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=args.timeout) as response:
            body = response.read()
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")[:300]
        raise AudioError(f"HTTP {exc.code}: {detail}") from exc
    except (urllib.error.URLError, socket.timeout, TimeoutError) as exc:
        raise AudioError(f"TTS request failed: {exc}") from exc

    if len(body) < 100 or not body.startswith(b"RIFF") or b"WAVE" not in body[:16]:
        preview = body[:200].decode("utf-8", errors="replace")
        raise AudioError(f"server did not return WAV audio: {preview!r}")
    return body


def generate_wav(word, wav_path, args):
    last_error = None
    for attempt in range(1, args.retries + 2):
        if attempt > 1:
            print(f"  Retry {attempt - 1}/{args.retries} for {word!r}...")
        try:
            wav_path.write_bytes(request_tts(word, args))
            return
        except AudioError as exc:
            last_error = str(exc)
            print(f"  TTS error: {last_error}")
    raise AudioError(last_error or "TTS generation failed")


def convert_to_ogg(wav_path, ogg_path, ffmpeg):
    command = [
        ffmpeg,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(wav_path),
        "-c:a",
        "libvorbis",
        "-q:a",
        "4",
        str(ogg_path),
    ]
    result = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        raise AudioError(
            "ffmpeg failed: "
            + (result.stderr.strip() or f"exit code {result.returncode}")
        )
    if not is_audio_file(ogg_path):
        raise AudioError("ffmpeg did not create a usable OGG file")


def process_entry(json_path, entry, args, ffmpeg):
    word = word_of(entry, json_path)
    audio_dir = json_path.parent / "audios"
    audio_dir.mkdir(parents=True, exist_ok=True)

    target_ogg = audio_dir / f"{word}.ogg"
    target_wav = audio_dir / f"{word}.wav"

    existing = resolve_audio(audio_value(entry))
    if not args.force and is_audio_file(existing):
        try:
            shown = existing.relative_to(DATABASE_DIR).as_posix()
        except ValueError:
            shown = str(existing)
        print(f"  SKIP {word}: already has {shown}")
        return "skipped"

    if not args.force and is_audio_file(target_ogg):
        changed = set_audio_field(json_path, entry, target_ogg)
        suffix = " (linked existing file)" if changed else ""
        print(f"  SKIP {word}: already has {relative_audio_path(target_ogg)}{suffix}")
        return "skipped"

    if not args.force and is_audio_file(target_wav):
        print(f"  Reusing existing WAV for {word}")
    else:
        print(f"  Generating TTS for {word}...")
        generate_wav(word, target_wav, args)

    print(f"  Converting {word} to OGG...")
    convert_to_ogg(target_wav, target_ogg, ffmpeg)
    set_audio_field(json_path, entry, target_ogg)

    if not args.keep_wav:
        try:
            target_wav.unlink()
        except OSError:
            pass

    print(f"  SAVED {word}: {relative_audio_path(target_ogg)}")
    return "generated"


def find_ffmpeg():
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg:
        return ffmpeg
    for name in ("ffmpeg.exe", "ffmpeg"):
        candidate = Path(name)
        if candidate.is_file():
            return str(candidate.resolve())
    return None


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate OGG pronunciation audio for Word Lock database words."
    )
    parser.add_argument(
        "--endpoint",
        default=DEFAULT_ENDPOINT,
        help=f"llama.cpp server base URL (default: {DEFAULT_ENDPOINT}).",
    )
    parser.add_argument("--voice", default="female", help="TTS voice (default: female).")
    parser.add_argument("--lang", default="en", help="TTS language (default: en).")
    parser.add_argument(
        "--n-predict",
        type=int,
        default=128,
        help="Maximum audio tokens per word (default: 128).",
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
        help="Only process this word (can be repeated).",
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
        help="Seconds to wait between generated words (default: 0).",
    )
    return parser.parse_args(argv)


def run(args):
    ffmpeg = find_ffmpeg()
    if ffmpeg is None:
        print("ERROR: ffmpeg was not found in PATH.")
        return 1

    try:
        check_server(args.endpoint, min(args.timeout, 15.0))
    except AudioError as exc:
        print(f"ERROR: {exc}")
        return 1

    wanted = {word.strip().lower() for word in args.word if word.strip()}
    entries = [
        (path, entry)
        for path, entry in iter_entries()
        if not wanted or word_of(entry, path) in wanted
    ]

    if wanted:
        found = {word_of(entry, path) for path, entry in entries}
        for missing in sorted(wanted - found):
            print(f"WARNING: word not found in database: {missing}")

    print(f"Database: {DATABASE_DIR}")
    print(f"Words to process: {len(entries)}")
    print("-" * 72)

    generated = 0
    skipped = 0
    failed = []

    for index, (json_path, entry) in enumerate(entries, 1):
        word = word_of(entry, json_path)
        print(f"[{index}/{len(entries)}] {word}")
        try:
            result = process_entry(json_path, entry, args, ffmpeg)
        except (AudioError, OSError) as exc:
            print(f"  FAILED {word}: {exc}")
            failed.append((word, str(exc)))
            continue

        if result == "generated":
            generated += 1
            if args.delay > 0 and index < len(entries):
                time.sleep(args.delay)
        else:
            skipped += 1

    print("-" * 72)
    print(
        f"Done: {generated} generated, {skipped} skipped, "
        f"{len(failed)} failed"
    )
    if failed:
        print("Failed words:")
        for word, error in failed:
            print(f"  - {word}: {error}")
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
