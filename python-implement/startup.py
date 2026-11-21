"""Start Word Lock with Windows using the per-user registry Run key.

The entry lives in:
    HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run

No administrator rights are needed because only the current user's hive is
touched. The registry is the single source of truth for whether the entry
exists; ``config.json`` stores the user's preference (START_WITH_WINDOWS).
"""

import sys
from pathlib import Path

APP_RUN_NAME = "WordLock"
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"

try:
    import winreg
except ImportError:  # pragma: no cover - Windows-only application
    winreg = None


def supported():
    """True when the registry is available (i.e. running on Windows)."""
    return winreg is not None


def launch_command():
    """Command line Windows runs at login.

    * Frozen PyInstaller build: ``sys.executable`` is the real .exe, whereas
      ``__file__`` points into the onefile extraction folder.
    * Source checkout: use pythonw.exe so no console window appears.
    """
    if getattr(sys, "frozen", False):
        return f'"{sys.executable}"'

    pythonw = Path(sys.executable).with_name("pythonw.exe")
    interpreter = pythonw if pythonw.is_file() else Path(sys.executable)
    script = Path(__file__).resolve().parent / "app.py"
    return f'"{interpreter}" "{script}"'


def _read():
    """Return the registered command line, or None when not enabled."""
    if winreg is None:
        return None
    try:
        with winreg.OpenKey(
            winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_READ
        ) as key:
            value, _kind = winreg.QueryValueEx(key, APP_RUN_NAME)
        return str(value)
    except (FileNotFoundError, OSError):
        return None


def is_enabled():
    """True when a Run entry for this app exists."""
    return _read() is not None


def enable():
    """Create or refresh the Run entry. Returns True on success."""
    if winreg is None:
        return False
    command = launch_command()
    if _read() == command:
        return True
    try:
        with winreg.CreateKeyEx(
            winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE
        ) as key:
            winreg.SetValueEx(key, APP_RUN_NAME, 0, winreg.REG_SZ, command)
        return True
    except OSError:
        return False


def disable():
    """Remove the Run entry. Returns True when it is gone afterwards."""
    if winreg is None:
        return False
    try:
        with winreg.OpenKey(
            winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE
        ) as key:
            winreg.DeleteValue(key, APP_RUN_NAME)
        return True
    except FileNotFoundError:
        return True
    except OSError:
        return False


def sync_path():
    """Heal a moved/renamed exe: refresh the entry only if it already exists.

    Called on launch so an enabled entry always points at the current exe,
    while never enabling startup for users who turned it off.
    """
    current = _read()
    if current is None:
        return False
    if current != launch_command():
        return enable()
    return True
