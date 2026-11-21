"""Generate missing Word Lock dictionary entries with a local llama.cpp server.

Usage:
    python generate.py --input words.txt

`words.txt` contains one English word per line. Existing words are skipped;
missing words are sent to llama.cpp and saved as database/<first-letter>/<word>.json.

Run this from the project root or from the database folder:
    python database/generate.py --input database/words.txt
    python generate.py --input words.txt

The llama.cpp server must be listening on http://127.0.0.1:9931 (configurable
with --endpoint).
"""

import argparse
import json
import re
import socket
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

DATABASE_DIR = Path(__file__).resolve().parent

REQUIRED_KEYS = [
    "word",
    "language",
    "pronunciation",
    "part_of_speech",
    "definitions",
    "examples",
    "synonyms",
    "antonyms",
    "tags",
]

WORD_RE = re.compile(r"^[a-z][a-z0-9'_-]*$")

FALLBACK_PROMPT = """You generate one English-to-Vietnamese dictionary entry.

Return exactly one valid JSON object and nothing else: no markdown fences, no
comments, no explanation before or after it.

Schema:
{
  "word": string,
  "language": {"source": "en", "target": "vi"},
  "pronunciation": {"ipa": string, "audio": ""},
  "part_of_speech": string,
  "definitions": [{"english": string, "vietnamese": string}],
  "examples": [{"english": string, "vietnamese": string}],
  "synonyms": [string],
  "antonyms": [string],
  "tags": [string]
}

Rules:
- `word` must be exactly the requested lowercase word.
- Give at least one clear English definition and its natural Vietnamese translation.
- Give two natural example sentences and their Vietnamese translations.
- Include the IPA transcription wrapped in slashes, for example /ˈzebrə/.
- Use empty arrays when there are no synonyms, antonyms, or tags.
- Do not add extra top-level keys. All strings must be non-empty except
  `pronunciation.audio` and any intentionally empty lists.
"""


class GenerateError(Exception):
    """A recoverable generation failure for one word."""


def safe_console():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def resolve_input(raw):
    candidate = Path(raw).expanduser()
    if candidate.is_absolute():
        return candidate if candidate.is_file() else None

    for base in (Path.cwd(), Path(__file__).resolve().parent):
        path = (base / candidate).resolve()
        if path.is_file():
            return path
    return None


def read_words(path):
    """Read, normalize, and de-duplicate words while preserving file order."""
    words = []
    seen = set()
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        word = line.strip().lower()
        if not word or word.startswith("#"):
            continue
        if not WORD_RE.fullmatch(word):
            print(f"  SKIP line {line_number}: invalid word {line.strip()!r}")
            continue
        if word not in seen:
            seen.add(word)
            words.append(word)
    return words


def load_existing_words():
    """Collect words already present anywhere in the database."""
    existing = set()
    for path in DATABASE_DIR.rglob("*.json"):
        word = ""
        try:
            with open(path, encoding="utf-8") as handle:
                data = json.load(handle)
            if isinstance(data, dict):
                word = str(data.get("word", "")).strip().lower()
        except (OSError, ValueError):
            pass
        existing.add(word or path.stem.strip().lower())
    return existing


def build_messages(prompt, word, previous_error=None):
    messages = [
        {"role": "system", "content": prompt},
        {"role": "user", "content": f"Generate the JSON for the word: {word}"},
    ]
    if previous_error:
        messages.append(
            {
                "role": "user",
                "content": (
                    f"The previous reply was invalid: {previous_error}. "
                    "Return only corrected JSON for the word."
                ),
            }
        )
    return messages


def chat_url(endpoint):
    endpoint = endpoint.rstrip("/")
    if endpoint.endswith("/v1/chat/completions"):
        return endpoint
    if endpoint.endswith("/v1"):
        return endpoint + "/chat/completions"
    return endpoint + "/v1/chat/completions"


def request_llm(endpoint, model, messages, timeout, max_tokens):
    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.0,
        "max_tokens": max_tokens,
        "stream": False,
    }
    request = urllib.request.Request(
        chat_url(endpoint),
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")[:500]
        raise GenerateError(f"HTTP {exc.code}: {detail}") from exc
    except (urllib.error.URLError, socket.timeout, TimeoutError) as exc:
        raise GenerateError(f"cannot reach llama.cpp: {exc}") from exc

    try:
        data = json.loads(body)
        content = data["choices"][0]["message"]["content"]
    except (ValueError, KeyError, IndexError, TypeError) as exc:
        raise GenerateError(f"unexpected llama.cpp response: {body[:300]}") from exc

    if not isinstance(content, str) or not content.strip():
        raise GenerateError("llama.cpp returned an empty response")
    return content


def extract_json(text):
    """Parse JSON, tolerating markdown fences or surrounding prose."""
    text = text.strip()
    fence = re.search(r"```(?:json)?\s*(.*?)```", text, re.DOTALL | re.IGNORECASE)
    if fence:
        text = fence.group(1).strip()

    try:
        return json.loads(text)
    except ValueError:
        pass

    start = text.find("{")
    end = text.rfind("}")
    if start == -1 or end <= start:
        raise GenerateError("response did not contain a JSON object")
    try:
        return json.loads(text[start:end + 1])
    except ValueError as exc:
        raise GenerateError(f"invalid JSON in response: {exc}") from exc


def validate_entry(entry, word, target_path):
    """Check the generated object before it is written to disk."""
    if not isinstance(entry, dict):
        raise GenerateError("LLM output is not a JSON object")

    returned = str(entry.get("word", "")).strip().lower()
    if returned != word:
        raise GenerateError(
            f"LLM returned word {returned!r}, expected {word!r}"
        )
    entry["word"] = word

    missing = [key for key in REQUIRED_KEYS if key not in entry]
    if missing:
        raise GenerateError("missing required keys: " + ", ".join(missing))

    try:
        from validate_database import validate_entry as validate_schema
        errors, _warnings = validate_schema(entry, target_path)
    except ImportError:
        return

    if errors:
        raise GenerateError("; ".join(errors[:5]))


def generate_entry(word, target_path, prompt, args):
    """Ask the LLM until it returns a valid entry or retries run out."""
    last_error = None
    for attempt in range(1, args.retries + 2):
        if attempt > 1:
            print(f"  Retry {attempt - 1}/{args.retries} for {word!r}...")
        try:
            content = request_llm(
                args.endpoint,
                args.model,
                build_messages(prompt, word, last_error),
                args.timeout,
                args.max_tokens,
            )
            entry = extract_json(content)
            validate_entry(entry, word, target_path)
            return entry
        except GenerateError as exc:
            last_error = str(exc)
            print(f"  Invalid response: {last_error}")
    raise GenerateError(last_error or "generation failed")


def save_entry(entry, target_path):
    target_path.parent.mkdir(parents=True, exist_ok=True)
    temp = target_path.with_suffix(".json.tmp")
    with open(temp, "w", encoding="utf-8") as handle:
        json.dump(entry, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    temp.replace(target_path)


def run(args):
    input_path = resolve_input(args.input)
    if input_path is None:
        print(f"ERROR: input file not found: {args.input}")
        return 1

    prompt = FALLBACK_PROMPT

    words = read_words(input_path)
    existing = load_existing_words()
    print(f"Database: {DATABASE_DIR}")
    print(f"Words file: {input_path}")
    print(f"Words found: {len(words)} | already in database: {len(existing)}")
    print("-" * 72)

    generated = 0
    skipped = 0
    failed = []

    for index, word in enumerate(words, 1):
        prefix = f"[{index}/{len(words)}] {word}"
        target_path = DATABASE_DIR / word[0] / f"{word}.json"

        if word in existing or target_path.exists():
            print(f"{prefix}: already exists, skipping")
            skipped += 1
            continue

        print(f"{prefix}: generating...")
        try:
            entry = generate_entry(word, target_path, prompt, args)
            save_entry(entry, target_path)
        except GenerateError as exc:
            print(f"{prefix}: FAILED - {exc}")
            failed.append((word, str(exc)))
            continue
        except OSError as exc:
            print(f"{prefix}: FAILED to write file - {exc}")
            failed.append((word, str(exc)))
            continue

        existing.add(word)
        generated += 1
        print(f"{prefix}: saved {target_path.relative_to(DATABASE_DIR)}")

        if args.delay > 0 and index < len(words):
            time.sleep(args.delay)

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


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate missing Word Lock dictionary entries with llama.cpp."
    )
    parser.add_argument(
        "--input",
        required=True,
        help='Text file containing one word per line (e.g. "words.txt").',
    )
    parser.add_argument(
        "--endpoint",
        default="http://127.0.0.1:9931",
        help="llama.cpp server base URL (default: http://127.0.0.1:9931).",
    )
    parser.add_argument(
        "--model",
        default="local-model",
        help="Model name sent to llama.cpp (default: local-model).",
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
        default=2,
        help="Extra attempts after an invalid response (default: 2).",
    )
    parser.add_argument(
        "--max-tokens",
        type=int,
        default=2048,
        help="Maximum tokens per response (default: 2048).",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=0.0,
        help="Seconds to wait between generated words (default: 0).",
    )
    return parser.parse_args(argv)


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
