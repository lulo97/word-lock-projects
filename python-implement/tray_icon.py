import threading

import pystray
from PIL import Image, ImageDraw

from config import APP_NAME, ASSETS_DIR


def _tray_image():
    for name in ("icon.png", "icon.ico"):
        path = ASSETS_DIR / name
        try:
            if path.exists():
                return Image.open(path).convert("RGBA")
        except Exception:
            pass

    # Fallback icon if the generated assets are missing.
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((4, 4, 60, 60), fill="#3b82f6")
    return image


def start_tray(
    on_open,
    on_next_style,
    on_settings,
    on_exit,
    is_running,
    on_toggle_running,
    is_countdown_on,
    on_toggle_countdown,
):
    """Create the tray icon and run it in a background thread.

    The tray callbacks must not touch Tk directly; the app receives them as
    queue events. The checked/text callbacks read plain values, which is
    thread-safe.
    """

    def running_label(item):
        return "Stop app" if is_running() else "Start app"

    menu = pystray.Menu(
        pystray.MenuItem(
            "Open app",
            lambda icon, item: on_open(),
            default=True,
            enabled=lambda item: is_running(),
        ),
        pystray.MenuItem("Next UI style", lambda icon, item: on_next_style()),
        pystray.MenuItem("Settings", lambda icon, item: on_settings()),
        pystray.MenuItem(running_label, lambda icon, item: on_toggle_running()),
        pystray.MenuItem(
            "Show countdown",
            lambda icon, item: on_toggle_countdown(),
            checked=lambda item: is_countdown_on(),
        ),
        pystray.Menu.SEPARATOR,
        pystray.MenuItem("Exit", lambda icon, item: on_exit()),
    )
    icon = pystray.Icon("WordLock", _tray_image(), APP_NAME, menu)
    threading.Thread(target=icon.run, daemon=True).start()
    return icon
