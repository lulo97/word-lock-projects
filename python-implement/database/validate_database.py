"""Validate every Word Lock .json entry in this folder.

Word files are stored one folder per first letter (database/a/apple.json),
so this script searches subfolders too.

Standalone: uses only the Python standard library.

Usage:
    python database/validate_database.py

Exit code 0 = all files valid, 1 = at least one invalid file.
A detailed report is written to database/validation_errors.log.
"""

import json
import sys
from datetime import datetime
from pathlib import Path

DATABASE_DIR = Path(__file__).resolve().parent
LOG_PATH = DATABASE_DIR / "validation_errors.log"

REQUIRED_TOP_LEVEL = [
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

STRING_LISTS = ["synonyms", "antonyms", "tags"]


def safe_console():
    """Avoid UnicodeEncodeError on Windows consoles with a legacy code page."""
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def type_name(value):
    return type(value).__name__


def preview(value, limit=60):
    text = repr(value)
    return text if len(text) <= limit else text[: limit - 3] + "..."


def check_string(value, path, allow_empty=False):
    """Return an error message, or None if the value is a valid string."""
    if not isinstance(value, str):
        return f"{path}: expected string, got {type_name(value)} ({preview(value)})"
    if not allow_empty and not value.strip():
        return f"{path}: must not be an empty string"
    return None


def check_string_list(value, path):
    errors = []
    if not isinstance(value, list):
        return [f"{path}: expected list of strings, got {type_name(value)}"]
    for index, item in enumerate(value):
        message = check_string(item, f"{path}[{index}]")
        if message:
            errors.append(message)
    return errors


def check_object(value, path, required_keys):
    """Validate a dict and its required keys, returning (errors, dict_or_None)."""
    if not isinstance(value, dict):
        return [f"{path}: expected object, got {type_name(value)}"], None

    errors = []
    for key in required_keys:
        if key not in value:
            errors.append(f"{path}.{key}: missing required key")
    return errors, value


def validate_entry(data, filename):
    """Validate one parsed JSON entry. Returns (errors, warnings)."""
    errors = []
    warnings = []

    if not isinstance(data, dict):
        return [f"$: top-level must be a JSON object, got {type_name(data)}"], []

    for key in REQUIRED_TOP_LEVEL:
        if key not in data:
            errors.append(f"$.{key}: missing required key")

    extra_keys = sorted(set(data) - set(REQUIRED_TOP_LEVEL))
    for key in extra_keys:
        warnings.append(f"$.{key}: unknown key (ignored by the app)")

    if "word" in data:
        message = check_string(data["word"], "$.word")
        if message:
            errors.append(message)
        elif filename.stem.lower() != data["word"].strip().lower():
            warnings.append(
                f"$.word: '{data['word']}' does not match filename "
                f"'{filename.name}'"
            )

    if "language" in data:
        language_errors, language = check_object(
            data["language"], "$.language", ["source", "target"]
        )
        errors.extend(language_errors)
        if language is not None:
            for key in ("source", "target"):
                if key in language:
                    message = check_string(
                        language[key], f"$.language.{key}", allow_empty=True
                    )
                    if message:
                        errors.append(message)

    if "pronunciation" in data:
        pronunciation_errors, pronunciation = check_object(
            data["pronunciation"], "$.pronunciation", ["ipa", "audio"]
        )
        errors.extend(pronunciation_errors)
        if pronunciation is not None:
            for key in ("ipa", "audio"):
                if key in pronunciation:
                    message = check_string(
                        pronunciation[key],
                        f"$.pronunciation.{key}",
                        allow_empty=True,
                    )
                    if message:
                        errors.append(message)

    if "part_of_speech" in data:
        message = check_string(
            data["part_of_speech"], "$.part_of_speech", allow_empty=True
        )
        if message:
            errors.append(message)

    if "definitions" in data:
        definitions = data["definitions"]
        if not isinstance(definitions, list):
            errors.append(
                f"$.definitions: expected list of objects, got {type_name(definitions)}"
            )
        elif not definitions:
            errors.append("$.definitions: must contain at least one definition")
        else:
            for index, definition in enumerate(definitions):
                path = f"$.definitions[{index}]"
                definition_errors, definition = check_object(
                    definition, path, ["english", "vietnamese"]
                )
                errors.extend(definition_errors)
                if definition is not None:
                    for key in ("english", "vietnamese"):
                        if key in definition:
                            message = check_string(
                                definition[key], f"{path}.{key}"
                            )
                            if message:
                                errors.append(message)

    if "examples" in data:
        examples = data["examples"]
        if not isinstance(examples, list):
            errors.append(
                f"$.examples: expected list of objects, got {type_name(examples)}"
            )
        else:
            for index, example in enumerate(examples):
                path = f"$.examples[{index}]"
                example_errors, example = check_object(
                    example, path, ["english", "vietnamese"]
                )
                errors.extend(example_errors)
                if example is not None:
                    for key in ("english", "vietnamese"):
                        if key in example:
                            message = check_string(example[key], f"{path}.{key}")
                            if message:
                                errors.append(message)

    for key in STRING_LISTS:
        if key in data:
            errors.extend(check_string_list(data[key], f"$.{key}"))

    return errors, warnings


def validate_file(path):
    """Read and validate one file. Returns (errors, warnings)."""
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        return [
            f"file is not valid UTF-8: {exc} (byte {exc.start} => {exc.reason})"
        ], []
    except OSError as exc:
        return [f"cannot read file: {exc}"], []

    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        return [
            f"invalid JSON: {exc.msg} at line {exc.lineno}, column {exc.colno} "
            f"(char {exc.pos})"
        ], []

    return validate_entry(data, path)


def main():
    safe_console()
    started = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    files = sorted(DATABASE_DIR.rglob("*.json"))

    lines = [
        f"[{started}] Word Lock database validation",
        f"Directory: {DATABASE_DIR}",
        f"Files found: {len(files)}",
        "-" * 72,
    ]
    print(lines[0])
    print(lines[1])
    print(lines[2])

    invalid_files = 0
    warning_count = 0

    for path in files:
        errors, warnings = validate_file(path)
        warning_count += len(warnings)

        if errors:
            invalid_files += 1
            lines.append(f"INVALID {path.name} ({len(errors)} error(s))")
            print(f"INVALID {path.name} ({len(errors)} error(s))")
            for message in errors:
                line = f"  ERROR {path.name}: {message}"
                lines.append(line)
                print(line)
        else:
            lines.append(f"OK      {path.name}")
            print(f"OK      {path.name}")

        for message in warnings:
            line = f"  WARN  {path.name}: {message}"
            lines.append(line)
            print(line)

    summary = (
        f"{len(files)} file(s) checked: "
        f"{len(files) - invalid_files} valid, "
        f"{invalid_files} invalid, "
        f"{warning_count} warning(s)"
    )
    lines.append("-" * 72)
    lines.append(summary)
    print("-" * 72)
    print(summary)

    try:
        LOG_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"Report written to {LOG_PATH}")
    except OSError as exc:
        print(f"Could not write log file: {exc}")

    return 1 if invalid_files else 0


if __name__ == "__main__":
    sys.exit(main())
