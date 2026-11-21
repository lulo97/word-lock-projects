"""Modal Settings window for editing config.json without touching the file."""

import re
import tkinter as tk

import customtkinter as ctk

import settings
import startup
import ui_styles
import window

WIDTH = 580
HEIGHT = 730


def _parse_words(text):
    """Accept one word per line, or comma/semicolon/space separated."""
    return [part for part in re.split(r"[\s,;]+", text.strip()) if part]


class SettingsDialog:
    """Modal editor for AUTO_SHOW_DELAY_MS, SOUND_VOLUME, START_WITH_WINDOWS
    and IGNORE_WORDS."""

    def __init__(self, parent, on_close):
        self.parent = parent
        self.on_close = on_close
        self._closed = False
        self.style = ui_styles.current()

        data = settings.load()
        self.window = tk.Toplevel(parent)
        self.window.title("Word Lock Settings")
        self.window.configure(bg=self.style.background_color)
        self.window.resizable(False, False)
        self.window.protocol("WM_DELETE_WINDOW", self.close)
        window.set_window_icon(self.window)
        window.square_corners(self.window)

        self._build(data)
        self.window.update_idletasks()
        self._center()
        self.window.deiconify()
        self.window.lift()
        try:
            self.window.attributes("-topmost", True)
            self.window.after(700, self._release_topmost)
        except tk.TclError:
            pass
        self.window.after(100, self._initial_focus)

    # ---------- layout ----------

    def _center(self):
        # Measure the real (DPI-scaled) content instead of scaling a fixed
        # height. The action row lives outside the scroll area, so Cancel /
        # Save are never clipped at 100%, 125%, 150% or custom scaling.
        try:
            scaling = ctk.ScalingTracker.get_window_scaling(self.window)
        except Exception:
            scaling = 1.0

        self.window.update_idletasks()
        width = max(round(WIDTH * scaling), self.card.winfo_reqwidth())
        content_height = (
            self.body.winfo_reqheight()
            + self.footer.winfo_reqheight()
            + round(32 * scaling)
        )
        height = max(round(HEIGHT * scaling), content_height)

        screen_w = self.window.winfo_screenwidth()
        screen_h = self.window.winfo_screenheight()
        width = min(width, max(1, screen_w - 24))
        height = min(height, max(1, screen_h - 80))

        x = max(0, (screen_w - width) // 2)
        y = max(0, (screen_h - height) // 2 - 40)
        self.window.geometry(f"{width}x{height}+{x}+{y}")
        self.window.after(20, self._sync_scrollbar)

    def _sync_scrollbar(self):
        """Only show the settings scrollbar when the content actually scrolls."""
        if self._closed:
            return
        try:
            body = self.body
            if body._parent_canvas.yview() == (0.0, 1.0):
                body._scrollbar.grid_remove()
            else:
                body._scrollbar.grid()
        except (AttributeError, tk.TclError):
            pass

    def _build(self, data):
        style = self.style
        self.card = card = ctk.CTkFrame(
            self.window,
            fg_color=style.card,
            border_width=style.border_width,
            border_color=style.card_border,
        )
        card.pack(fill="both", expand=True)

        # Fixed footer: always visible regardless of DPI or content height.
        self.footer = footer = ctk.CTkFrame(card, fg_color="transparent")
        footer.pack(side="bottom", fill="x", padx=28, pady=(8, 24))

        self.body = body = ctk.CTkScrollableFrame(
            card,
            fg_color="transparent",
            border_width=0,
            scrollbar_fg_color="transparent",
            scrollbar_button_color=style.card_border,
            scrollbar_button_hover_color=style.chip_hover,
        )
        body.pack(fill="both", expand=True)
        body._parent_canvas.bind(
            "<Configure>", lambda event: self._sync_scrollbar(), add="+"
        )

        ctk.CTkLabel(
            body,
            text=style.heading("Settings"),
            text_color=style.accent_soft,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["brand"], "bold"),
            anchor="w",
        ).pack(fill="x", padx=24, pady=(24, 2))
        ctk.CTkLabel(
            body,
            text=str(settings.CONFIG_FILE),
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["small"]),
            anchor="w",
            wraplength=WIDTH - 64,
        ).pack(fill="x", padx=24, pady=(0, 4))
        ctk.CTkLabel(
            body,
            text=f"UI style: {style.name} — a new random style is picked after each completed word.",
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["small"]),
            anchor="w",
            wraplength=WIDTH - 64,
        ).pack(fill="x", padx=24, pady=(0, 18))

        ctk.CTkLabel(
            body,
            text="AUTO_SHOW_DELAY_MS",
            text_color=style.text_body,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["body"], "bold"),
            anchor="w",
        ).pack(fill="x", padx=24)
        self.delay_entry = ctk.CTkEntry(
            body,
            width=WIDTH - 48,
            height=42,
            font=style.font(18),
            justify="left",
            fg_color=style.entry_bg,
            border_color=style.accent,
            border_width=style.input_border_width,
            text_color=style.text_primary,
        )
        self.delay_entry.pack(fill="x", padx=24, pady=(8, 4))
        self.delay_entry.insert(0, str(settings.auto_show_delay_ms()))
        ctk.CTkLabel(
            body,
            text="Milliseconds before the word gate opens. 1000 = 1 second, 300000 = 5 minutes.",
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(12),
            anchor="w",
            wraplength=WIDTH - 64,
        ).pack(fill="x", padx=24, pady=(0, 14))

        volume_header = ctk.CTkFrame(body, fg_color="transparent")
        volume_header.pack(fill="x", padx=24)
        ctk.CTkLabel(
            volume_header,
            text="SOUND_VOLUME",
            text_color=style.text_body,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["body"], "bold"),
            anchor="w",
        ).pack(side="left")
        self.volume_value_label = ctk.CTkLabel(
            volume_header,
            text="",
            text_color=style.accent_soft,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["body"], "bold"),
            anchor="e",
        )
        self.volume_value_label.pack(side="right")

        self.volume_var = tk.DoubleVar(value=settings.sound_volume())
        self.volume_slider = ctk.CTkSlider(
            body,
            from_=settings.MIN_SOUND_VOLUME,
            to=settings.MAX_SOUND_VOLUME,
            number_of_steps=40,
            variable=self.volume_var,
            command=self._update_volume_label,
            height=22,
            fg_color=style.chip_bg,
            progress_color=style.accent,
            button_color=style.accent,
            button_hover_color=style.accent_hover,
            border_width=0,
        )
        self.volume_slider.pack(fill="x", padx=24, pady=(8, 2))
        self._update_volume_label(self.volume_var.get())
        ctk.CTkLabel(
            body,
            text="Playback gain relative to the Windows master volume. 2.0 = twice as loud.",
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(12),
            anchor="w",
            wraplength=WIDTH - 64,
        ).pack(fill="x", padx=24, pady=(0, 18))

        ctk.CTkLabel(
            body,
            text="START_WITH_WINDOWS",
            text_color=style.text_body,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["body"], "bold"),
            anchor="w",
        ).pack(fill="x", padx=24)
        self.startup_var = tk.BooleanVar(value=bool(data.get("START_WITH_WINDOWS")))
        self.startup_switch = ctk.CTkSwitch(
            body,
            text="Launch Word Lock when I sign in to Windows",
            variable=self.startup_var,
            onvalue=True,
            offvalue=False,
            font=style.font(16),
            text_color=style.text_body,
            fg_color=style.chip_bg,
            progress_color=style.accent,
            button_color=style.accent,
            button_hover_color=style.accent_hover,
            border_width=0,
        )
        self.startup_switch.pack(fill="x", padx=24, pady=(8, 2))
        ctk.CTkLabel(
            body,
            text=(
                "Adds an entry to the current user's registry Run key "
                "(no admin rights needed). If the exe is moved or renamed, the "
                "entry is updated the next time Word Lock starts."
            ),
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(12),
            anchor="w",
            wraplength=WIDTH - 64,
        ).pack(fill="x", padx=24, pady=(0, 18))

        ctk.CTkLabel(
            body,
            text="IGNORE_WORDS",
            text_color=style.text_body,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["body"], "bold"),
            anchor="w",
        ).pack(fill="x", padx=24)
        ctk.CTkLabel(
            body,
            text="One word per line, or separate with commas. These words will never be shown.",
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(12),
            anchor="w",
            wraplength=WIDTH - 64,
        ).pack(fill="x", padx=24, pady=(2, 8))

        self.ignore_box = ctk.CTkTextbox(
            body,
            width=WIDTH - 48,
            height=210,
            border_width=style.input_border_width,
            border_color=style.card_border,
            fg_color=style.entry_bg,
            text_color=style.text_primary,
            font=style.font(16),
            scrollbar_button_color=style.card_border,
            scrollbar_button_hover_color=style.chip_hover,
        )
        self.ignore_box.pack(fill="x", padx=24, pady=(0, 8))
        self.ignore_box.insert("1.0", "\n".join(data["IGNORE_WORDS"]))

        self.status_label = ctk.CTkLabel(
            footer,
            text="",
            text_color=style.error,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["body"]),
            anchor="w",
            wraplength=WIDTH - 64,
        )
        self.status_label.pack(fill="x")

        buttons = ctk.CTkFrame(footer, fg_color="transparent")
        buttons.pack(fill="x", pady=(12, 0))

        ctk.CTkButton(
            buttons,
            text="Cancel",
            width=140,
            height=48,
            fg_color="transparent",
            border_width=style.border_width,
            border_color=style.card_border,
            hover_color=style.chip_hover,
            text_color=style.text_body,
            font=style.font(ui_styles.SIZE["button"]),
            command=self.close,
        ).pack(side="left")

        ctk.CTkButton(
            buttons,
            text="Save config",
            width=180,
            height=48,
            fg_color=style.primary_button_bg,
            hover_color=style.primary_button_hover,
            text_color=style.primary_button_text,
            font=style.font(ui_styles.SIZE["button"], "bold"),
            command=self.save,
        ).pack(side="right")

    # ---------- behavior ----------

    def _update_volume_label(self, value):
        try:
            value = float(value)
        except (TypeError, ValueError):
            return
        self.volume_value_label.configure(text=f"x{value:.1f}")

    def _release_topmost(self):
        if self._closed:
            return
        try:
            self.window.attributes("-topmost", False)
        except tk.TclError:
            pass

    def _initial_focus(self):
        if self._closed:
            return
        window.force_foreground(self.window)
        try:
            self.window.grab_set()
        except tk.TclError:
            pass
        try:
            self.delay_entry.focus_force()
        except tk.TclError:
            pass

    def _set_error(self, message):
        self.status_label.configure(text=message, text_color=self.style.error)

    def save(self):
        raw_delay = self.delay_entry.get().strip()
        try:
            delay = int(float(raw_delay))
        except ValueError:
            self._set_error("AUTO_SHOW_DELAY_MS must be a number in milliseconds.")
            return
        if delay <= 0:
            self._set_error("AUTO_SHOW_DELAY_MS must be greater than 0.")
            return

        words = _parse_words(self.ignore_box.get("1.0", "end"))
        try:
            volume = round(float(self.volume_var.get()), 1)
        except (TypeError, ValueError):
            self._set_error("SOUND_VOLUME must be a number.")
            return

        # Apply the registry change first so config.json never claims a state
        # the registry does not have.
        want_startup = bool(self.startup_var.get())
        if want_startup:
            applied = startup.enable()
            if not applied:
                self._set_error(
                    "Could not add Word Lock to Windows startup (registry write failed)."
                )
                return
        else:
            removed = startup.disable()
            if not removed:
                self._set_error(
                    "Could not remove Word Lock from Windows startup (registry write failed)."
                )
                return

        try:
            settings.save_config(delay, words, volume, want_startup)
        except OSError as exc:
            self._set_error(f"Could not save config.json: {exc}")
            return

        self.close()

    def focus(self):
        """Bring an already-open settings window back to the front."""
        if self._closed:
            return
        try:
            self.window.deiconify()
            self.window.lift()
            self.window.attributes("-topmost", True)
            self.window.after(700, self._release_topmost)
        except tk.TclError:
            pass
        window.force_foreground(self.window)
        try:
            self.delay_entry.focus_force()
        except tk.TclError:
            pass

    def close(self):
        if self._closed:
            return
        self._closed = True
        try:
            self.window.grab_release()
        except tk.TclError:
            pass
        try:
            self.window.destroy()
        except tk.TclError:
            pass
        if self.on_close is not None:
            self.on_close()
