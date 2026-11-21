"""Small always-on-top countdown card shown in the bottom-right corner.

Colours and fonts follow the active ``ui_styles`` style; the app calls
``apply_style()`` whenever the user rotates styles.
"""

import math
import platform
import tkinter as tk

import ui_styles

TRANSPARENT_KEY = "#010203"

WIDTH = 246
HEIGHT = 74
MARGIN = 22


def _work_area(root):
    """Screen area excluding the Windows taskbar, when available."""
    if platform.system() == "Windows":
        try:
            import ctypes
            from ctypes import wintypes

            rect = wintypes.RECT()
            # SPI_GETWORKAREA
            if ctypes.windll.user32.SystemParametersInfoW(
                0x0030, 0, ctypes.byref(rect), 0
            ):
                return rect.left, rect.top, rect.right, rect.bottom
        except Exception:
            pass
    return 0, 0, root.winfo_screenwidth(), root.winfo_screenheight()


def format_remaining(seconds):
    """Format seconds as 5s, 04:59, or 1:04:59."""
    seconds = max(0, int(math.ceil(seconds)))
    hours, remainder = divmod(seconds, 3600)
    minutes, secs = divmod(remainder, 60)
    if hours:
        return f"{hours}:{minutes:02d}:{secs:02d}"
    if minutes:
        return f"{minutes:02d}:{secs:02d}"
    return f"{secs}s"


class CountdownOverlay:
    """A borderless, semi-transparent countdown card.

    Uses a plain Tk toplevel instead of CTkToplevel so overrideredirect,
    transparency, and focus behavior stay predictable.
    """

    def __init__(self, root, style=None):
        self.root = root
        self.style = style or ui_styles.current()
        self.window = None
        self.visible = False
        self._time_id = None
        self._create_window()

    def _create_window(self):
        style = self.style
        window = tk.Toplevel(self.root)
        window.withdraw()
        window.overrideredirect(True)
        window.configure(bg=TRANSPARENT_KEY)

        try:
            window.attributes("-topmost", True)
        except tk.TclError:
            pass
        try:
            window.attributes("-alpha", style.overlay_alpha)
        except tk.TclError:
            pass
        if platform.system() == "Windows":
            try:
                window.attributes("-transparentcolor", TRANSPARENT_KEY)
            except tk.TclError:
                pass

        self.canvas = tk.Canvas(
            window,
            width=WIDTH,
            height=HEIGHT,
            bg=TRANSPARENT_KEY,
            highlightthickness=0,
            bd=0,
        )
        self.canvas.pack()

        # Square card background with a thin border.
        self._card_id = self.canvas.create_rectangle(
            1,
            1,
            WIDTH - 1,
            HEIGHT - 1,
            fill=style.card,
            outline=style.card_border,
            width=1,
        )

        self._label_id = self.canvas.create_text(
            20,
            23,
            text=style.heading("Word show in"),
            anchor="w",
            fill=style.text_muted,
            font=style.font(10, "bold"),
        )
        self._time_id = self.canvas.create_text(
            18,
            49,
            text="05:00",
            anchor="w",
            fill=style.text_primary,
            font=style.font(24, "bold"),
        )

        # Small square clock face on the right.
        cx, cy, half = WIDTH - 30, 37, 11
        self._clock_ids = [
            self.canvas.create_rectangle(
                cx - half, cy - half, cx + half, cy + half,
                outline=style.accent_soft, width=2,
            ),
            self.canvas.create_line(cx, cy, cx, cy - 6, fill=style.accent_soft, width=2),
            self.canvas.create_line(cx, cy, cx + 5, cy + 2, fill=style.accent_soft, width=2),
        ]

        self.window = window

    def apply_style(self, style=None):
        """Recolour and re-font the countdown card without recreating it."""
        if style is not None:
            self.style = style
        style = self.style
        if self.window is None:
            return

        try:
            self.window.attributes("-alpha", style.overlay_alpha)
        except tk.TclError:
            pass

        self.canvas.itemconfigure(
            self._card_id,
            fill=style.card,
            outline=style.card_border,
        )
        self.canvas.itemconfigure(
            self._label_id,
            text=style.heading("Word show in"),
            fill=style.text_muted,
            font=style.font(10, "bold"),
        )
        self.canvas.itemconfigure(
            self._time_id,
            fill=style.text_primary,
            font=style.font(24, "bold"),
        )
        for item in self._clock_ids:
            try:
                self.canvas.itemconfigure(item, fill=style.accent_soft)
            except tk.TclError:
                pass
            try:
                self.canvas.itemconfigure(item, outline=style.accent_soft)
            except tk.TclError:
                pass

    def _place_bottom_right(self):
        left, top, right, bottom = _work_area(self.root)
        x = max(left + MARGIN, right - WIDTH - MARGIN)
        y = max(top + MARGIN, bottom - HEIGHT - MARGIN)
        self.window.geometry(f"{WIDTH}x{HEIGHT}+{x}+{y}")

    def set_remaining(self, seconds):
        if self._time_id is not None:
            self.canvas.itemconfigure(self._time_id, text=format_remaining(seconds))

    def show(self):
        if self.window is None or self.visible:
            return
        self._place_bottom_right()
        self.window.deiconify()
        self.visible = True
        try:
            self.window.attributes("-topmost", True)
            self.window.lift()
        except tk.TclError:
            pass

    def hide(self):
        if self.window is not None and self.visible:
            self.window.withdraw()
        self.visible = False

    def destroy(self):
        if self.window is not None:
            try:
                self.window.destroy()
            except tk.TclError:
                pass
            self.window = None
        self.visible = False
