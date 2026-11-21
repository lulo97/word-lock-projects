import sys
from pathlib import Path

APP_NAME = "Word Lock"
APP_ID = "WordLock"
AUTO_SHOW_DELAY_MS = 60000 * 1

# Playback gain relative to the Windows master volume. 2.0 = twice as loud.
SOUND_VOLUME = 2.0


def resource_path(relative):
    """Resolve bundled data both in a normal checkout and inside a PyInstaller exe."""
    base = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return base / relative


DATABASE_DIR = resource_path("database")
ASSETS_DIR = resource_path("assets")
