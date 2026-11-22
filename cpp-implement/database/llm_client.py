"""Shared HTTP helpers for the Kotoba database generators.

Only the Python standard library is used, so the tools run without a virtual
environment. The request shapes are copied from database/ref-code:

* chat completions (word generation) -> llama.cpp on :9931
* /tts + /props (audio generation)   -> llama.cpp on :8080

Nothing here is imported by the C++ app; these are manual, offline tools.
"""

from __future__ import annotations

import json
import re
import urllib.error
import urllib.request


class LlmError(RuntimeError):
    """A request failed or the server returned something unusable."""


def _trim(endpoint: str) -> str:
    return str(endpoint).strip().rstrip("/")


def chat_url(endpoint: str) -> str:
    """Normalize a base URL to the OpenAI-compatible chat endpoint."""
    base = _trim(endpoint)
    if base.endswith("/v1/chat/completions"):
        return base
    if base.endswith("/v1"):
        return f"{base}/chat/completions"
    return f"{base}/v1/chat/completions"


def tts_url(endpoint: str) -> str:
    base = _trim(endpoint)
    return base if base.endswith("/tts") else f"{base}/tts"


def props_url(endpoint: str) -> str:
    base = _trim(endpoint)
    if base.endswith("/tts"):
        base = base[: -len("/tts")]
    if base.endswith("/v1"):
        base = base[: -len("/v1")]
    return f"{base}/props"


def _request(url: str, *, data: bytes | None, timeout: float,
             headers: dict[str, str] | None = None) -> bytes:
    request = urllib.request.Request(
        url,
        data=data,
        headers=headers or {},
        method="POST" if data is not None else "GET",
    )
    try:
        with urllib.request.urlopen(request, timeout=max(1.0, timeout)) as response:
            return response.read()
    except urllib.error.HTTPError as error:
        detail = error.read().decode("utf-8", "replace")[:500]
        raise LlmError(f"HTTP {error.code} from {url}: {detail}") from error
    except urllib.error.URLError as error:
        raise LlmError(f"cannot reach {url}: {error.reason}") from error
    except TimeoutError as error:
        raise LlmError(f"timeout after {timeout:.0f}s: {url}") from error


def request_chat(endpoint: str, model: str, messages: list[dict[str, str]],
                 timeout: float = 300.0, max_tokens: int = 2048) -> str:
    """POST a chat completion and return the assistant message content."""
    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.0,
        "max_tokens": max_tokens,
        "stream": False,
    }
    body = _request(
        chat_url(endpoint),
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        timeout=timeout,
    )
    try:
        data = json.loads(body.decode("utf-8", "replace"))
    except json.JSONDecodeError as error:
        raise LlmError(f"unexpected response: {body[:300]!r}") from error
    try:
        content = data["choices"][0]["message"]["content"]
    except (KeyError, IndexError, TypeError) as error:
        raise LlmError(f"unexpected response shape: {body[:300]!r}") from error
    if not isinstance(content, str) or not content.strip():
        raise LlmError("server returned an empty response")
    return content


_FENCE_RE = re.compile(r"```(?:json)?\s*(.*?)```", re.IGNORECASE | re.DOTALL)


def extract_json(text: str):
    """Parse JSON from a model reply, tolerating fences and prose."""
    source = str(text).strip()
    fence = _FENCE_RE.search(source)
    if fence:
        source = fence.group(1).strip()
    try:
        return json.loads(source)
    except json.JSONDecodeError:
        pass
    start = source.find("{")
    end = source.rfind("}")
    if start == -1 or end <= start:
        raise LlmError("response did not contain a JSON object")
    try:
        return json.loads(source[start:end + 1])
    except json.JSONDecodeError as error:
        raise LlmError(f"invalid JSON in response: {error}") from error


def is_wav_bytes(data: bytes) -> bool:
    return (
        len(data) >= 64
        and data[:4] == b"RIFF"
        and data[8:12] == b"WAVE"
    )


def check_audio_server(endpoint: str, timeout: float = 15.0) -> None:
    """Fail early when llama.cpp has no audio modality (needs --mmproj)."""
    body = _request(props_url(endpoint), data=None, timeout=timeout)
    try:
        data = json.loads(body.decode("utf-8", "replace"))
    except json.JSONDecodeError as error:
        raise LlmError(f"unexpected /props response: {body[:300]!r}") from error
    modalities = data.get("modalities") if isinstance(data, dict) else None
    if not isinstance(modalities, dict) or not modalities.get("audio"):
        raise LlmError(
            "llama.cpp has no audio modality; start it with --mmproj"
        )


def request_tts(endpoint: str, text: str, *, lang: str = "ja",
                voice: str = "female", n_predict: int = 128,
                timeout: float = 300.0) -> bytes:
    """POST text to llama.cpp /tts and return the WAV bytes."""
    payload = {
        "input": text,
        "lang": lang,
        "voice": voice,
        "n_predict": n_predict,
    }
    body = _request(
        tts_url(endpoint),
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        timeout=timeout,
    )
    if not is_wav_bytes(body):
        preview = body[:200].decode("utf-8", "replace")
        raise LlmError(f"server did not return WAV audio: {preview!r}")
    return body
