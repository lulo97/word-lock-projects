import threading

import psutil


def is_chrome(name):
    name = (name or "").lower()
    return (
        name.startswith("chrome")
        or name.startswith("google chrome")
        or name.startswith("chromium")
    )


class ChromeBlocker:
    """Freeze every Chrome process while the word gate is on screen."""

    def __init__(self):
        self._lock = threading.Lock()
        self._suspended = {}
        self._should_freeze = False

    def _freeze_now(self):
        """Suspend every Chrome process. Caller must hold the lock."""
        for proc in psutil.process_iter(["pid", "name"]):
            try:
                if not is_chrome(proc.info["name"]):
                    continue
                proc.suspend()
                self._suspended[proc.pid] = proc
            except psutil.Error:
                pass

    def _resume_now(self):
        """Resume the Chrome processes frozen by this app. Caller must hold the lock."""
        for proc in reversed(list(self._suspended.values())):
            try:
                proc.resume()
            except psutil.Error:
                pass
        self._suspended.clear()

    def _sync(self):
        """Bring Chrome in line with the latest requested state."""
        with self._lock:
            if self._should_freeze:
                if not self._suspended:
                    self._freeze_now()
            else:
                self._resume_now()

    def _set(self, frozen):
        self._should_freeze = bool(frozen)
        threading.Thread(target=self._sync, daemon=True).start()

    def freeze(self):
        self._set(True)

    def unfreeze(self):
        self._set(False)

    def shutdown(self):
        """Synchronously release Chrome (used on quit)."""
        self._should_freeze = False
        with self._lock:
            self._resume_now()
