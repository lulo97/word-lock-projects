"""Word gate and dictionary card.

All colours, fonts and shapes come from the active object in ``ui_styles``.
``WordGateUI.apply_style()`` rebuilds the visible screen, which is all that is
needed to switch the entire look at runtime.
"""

import time
import tkinter as tk

import customtkinter as ctk
from PIL import Image, ImageTk

import dictionary
import ui_styles
import window
from config import APP_NAME
from ui_styles import fonts as style_fonts

AUTO_ACCEPT_DELAY_MS = 500
SPEAK_ANIMATION_MS = 320
SPEAK_DEBOUNCE_MS = 350

WRAP_LENGTH = 900
INFO_WIDTH = 980
INFO_HEIGHT = 540


def _rgb(hex_color):
    hex_color = hex_color.lstrip("#")
    return tuple(int(hex_color[i:i + 2], 16) for i in (0, 2, 4))


def _mix(start, end, t):
    return tuple(round(start[i] + (end[i] - start[i]) * t) for i in range(3))


class GradientBackground:
    """Vertical gradient painted once per size behind the UI."""

    def __init__(self, root, style):
        self.root = root
        self.style = style
        self._job = None
        self._photo = None

        self.canvas = tk.Canvas(
            root, highlightthickness=0, bd=0, bg=style.background_color
        )
        self.canvas.place(x=0, y=0, relwidth=1, relheight=1)
        self._image_id = self.canvas.create_image(0, 0, anchor="nw")
        self._lower_canvas()

        root.bind("<Configure>", self._on_configure, add="+")
        self.redraw()

    def _on_configure(self, event):
        if event.widget is not self.root:
            return
        if self._job is not None:
            try:
                self.root.after_cancel(self._job)
            except Exception:
                pass
        self._job = self.root.after(60, self.redraw)

    def apply_style(self, style):
        self.style = style
        try:
            self.canvas.configure(bg=style.background_color)
        except tk.TclError:
            pass
        self.redraw()

    def redraw(self):
        self._job = None
        width = max(self.canvas.winfo_width(), 1)
        height = max(self.canvas.winfo_height(), 1)

        style = self.style
        if not style.gradient:
            self.canvas.delete("gradient")
            self.canvas.configure(bg=style.background_color)
            return

        top = _rgb(style.background_top)
        bottom = _rgb(style.background_bottom)

        # Build a 1px-wide gradient, then stretch it to the window size.
        base = Image.new("RGB", (1, 512))
        base.putdata([_mix(top, bottom, y / 511) for y in range(512)])
        image = base.resize((width, height), Image.BILINEAR)

        self._photo = ImageTk.PhotoImage(image)
        self.canvas.itemconfigure(self._image_id, image=self._photo)
        self.canvas.coords(self._image_id, 0, 0)
        self._lower_canvas()

    def _lower_canvas(self):
        # Canvas.lower is tag_lower in Tk; this actually lowers the widget.
        self.canvas.tk.call("lower", self.canvas._w)


def _force_square_shapes():
    """Override every CustomTkinter theme default that rounds widget corners."""
    try:
        for section in ctk.ThemeManager.theme.values():
            if not isinstance(section, dict):
                continue
            for key in section:
                if key.startswith("corner_") or key.startswith("button_corner"):
                    section[key] = 0
    except Exception:
        pass


def apply_root_style(root, style):
    """Apply window-level and global look/feel for a style."""
    ctk.set_appearance_mode(style.appearance)
    ctk.set_default_color_theme(style.color_theme)
    _force_square_shapes()
    style_fonts.configure_tk_defaults(style.family())
    try:
        root.configure(fg_color=style.background_color)
    except tk.TclError:
        pass

    gradient = getattr(root, "_gradient_background", None)
    if gradient is not None:
        gradient.apply_style(style)


def setup_root(root):
    """Apply the active style once, before any UI is created."""
    style = ui_styles.current()
    root.title(APP_NAME)
    window.set_window_icon(root)
    apply_root_style(root, style)
    window.square_corners(root)

    # Keep a reference so the gradient object (and its bindings) stay alive.
    root._gradient_background = GradientBackground(root, style)


class WordGateUI:
    """Type-the-word gate, then a styled dictionary card."""

    QUIZ = "quiz"
    DONE = "done"

    def __init__(
        self,
        root,
        on_exit,
        on_correct=None,
        on_listen=None,
        is_playing=None,
        on_style_cycle=None,
        on_stop=None,
        on_ignore=None,
    ):
        self.root = root
        self.on_exit = on_exit
        self.on_correct = on_correct
        self.on_listen = on_listen
        self.is_playing = is_playing
        self.on_style_cycle = on_style_cycle
        self.on_stop = on_stop
        self.on_ignore = on_ignore
        self.style = ui_styles.current()
        self.state = self.QUIZ
        self.current = None
        self._focus_sequence = 0
        self._finished_at = 0.0
        self._finish_timer = None
        self.listen_button = None
        self.ignore_button = None
        self._speaking_button = None
        self._speaking_after = None
        self._speaking_dots = 1
        self._speaking_prefix = ""
        self._speaking_idle_text = "Listen"
        self._speaking_started_at = 0.0

        self._build_layout()
        self._build_quiz()
        root.bind("<Return>", self._on_enter)
        root.bind("<FocusIn>", self._on_root_focus, add="+")

    # ---------- layout / style switching ----------

    def _build_layout(self):
        style = self.style
        self.container = ctk.CTkFrame(
            self.root,
            fg_color=style.card,
            border_width=style.border_width,
            border_color=style.card_border,
        )
        self.container.place(relx=0.5, rely=0.5, anchor="center")

        self.quiz_frame = ctk.CTkFrame(self.container, fg_color="transparent")
        self.info_frame = ctk.CTkFrame(self.container, fg_color="transparent")

        self.answer_var = ctk.StringVar()
        self.answer_var.trace_add("write", self._on_answer_changed)

    def apply_style(self, style=None):
        """Swap to a new style and rebuild the current screen in place."""
        if style is not None:
            ui_styles.set_current(style)
        self.style = ui_styles.current()
        apply_root_style(self.root, self.style)

        state = self.state
        current = self.current
        answer = self.answer_var.get() if self.answer_var is not None else ""

        self._stop_speaking(reset=False, stop_audio=True)
        self._cancel_finish_timer()
        self.cancel_focus()
        self.listen_button = None
        self.ignore_button = None

        try:
            self.container.destroy()
        except tk.TclError:
            pass

        self._build_layout()
        self._build_quiz()
        self.state = state
        self.current = current

        if state == self.DONE and current is not None:
            self._build_info(current)
            self.info_frame.pack(padx=64, pady=44)
            return

        self.quiz_frame.pack(padx=60, pady=44)
        self.word_label.configure(text=(current or {}).get("word", ""))
        self.answer_var.set(answer)
        self._schedule_focus()

    # ---------- quiz screen ----------

    def _build_quiz(self):
        style = self.style

        ctk.CTkLabel(
            self.quiz_frame,
            text=style.heading("Word Lock"),
            text_color=style.accent_soft,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["brand"], "bold"),
        ).pack(pady=(0, 10))

        self.word_label = ctk.CTkLabel(
            self.quiz_frame,
            text="",
            text_color=style.text_primary,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["word"], "bold", display=True),
        )
        self.word_label.pack(pady=(0, 6))

        self.quiz_hint = ctk.CTkLabel(
            self.quiz_frame,
            text="Type the word — correct answers unlock automatically",
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["hint"]),
        )
        self.quiz_hint.pack(pady=(0, 16))

        self.answer_entry = ctk.CTkEntry(
            self.quiz_frame,
            textvariable=self.answer_var,
            width=440,
            height=58,
            font=style.font(ui_styles.SIZE["entry"]),
            justify="center",
            fg_color=style.entry_bg,
            border_color=style.accent,
            border_width=style.input_border_width,
            text_color=style.text_primary,
        )
        self.answer_entry.pack(pady=(0, 12))

        self.feedback_label = ctk.CTkLabel(
            self.quiz_frame,
            text="",
            text_color=style.error,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["feedback"]),
        )
        self.feedback_label.pack(pady=(0, 4))

        self.quiz_ignore_button = ctk.CTkButton(
            self.quiz_frame,
            text="Ignore this word",
            width=170,
            height=40,
            fg_color="transparent",
            border_width=style.border_width,
            border_color=style.card_border,
            hover_color=style.chip_hover,
            text_color=style.text_muted,
            font=style.font(ui_styles.SIZE["button"]),
            command=self._ignore_current,
        )
        self.quiz_ignore_button.pack(pady=(8, 0))

    def show_question(self, entry):
        """Reset the UI to the word-typing gate with a new word."""
        self.current = entry
        self.state = self.QUIZ
        self._stop_speaking(reset=False, stop_audio=True)

        self.info_frame.pack_forget()
        self.quiz_frame.pack(padx=60, pady=44)

        self.word_label.configure(text=entry.get("word", ""))
        self.answer_entry.configure(state="normal")
        self._cancel_finish_timer()
        self.answer_var.set("")
        self.feedback_label.configure(text="", text_color=self.style.error)
        self.answer_entry.configure(border_color=self.style.accent)
        self._schedule_focus()

    # ---------- keyboard focus ----------

    def _schedule_focus(self):
        """Retry focus while the window is being maximized and activated.

        A single focus_set() right after deiconify is unreliable on Windows:
        maximizing, force_foreground(), and freezing Chrome can all move focus
        after it runs, which is why the answer box needed a mouse click.
        """
        self._focus_sequence += 1
        sequence = self._focus_sequence
        for delay in (0, 120, 350, 700, 1200, 2000):
            self.root.after(delay, lambda s=sequence: self._focus_if_current(s))

    def cancel_focus(self):
        """Stop pending focus retries, e.g. when the window is hidden."""
        self._focus_sequence += 1

    def _focus_if_current(self, sequence):
        if sequence != self._focus_sequence or self.state != self.QUIZ:
            return
        try:
            if self.root.state() == "withdrawn":
                return
        except tk.TclError:
            return
        self.focus_answer()

    def focus_answer(self):
        """Put the text cursor in the answer box and force Tk/Windows focus."""
        if self.state != self.QUIZ:
            return
        try:
            self.answer_entry.focus_force()
            self.answer_entry.icursor("end")
        except tk.TclError:
            pass

    def _on_root_focus(self, event):
        """When the window itself gains focus, move it into the answer box."""
        if event.widget is self.root and self.state == self.QUIZ:
            self.root.after_idle(self.focus_answer)

    # ---------- info screen ----------

    def _clear_info(self):
        self._stop_speaking(reset=False)
        self.listen_button = None
        self.ignore_button = None
        for child in self.info_frame.winfo_children():
            child.destroy()

    def _section_title(self, parent, text):
        style = self.style
        ctk.CTkLabel(
            parent,
            text=style.heading(text),
            text_color=style.accent_soft,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["heading"], "bold"),
            anchor="w",
        ).pack(fill="x", pady=(18, 6))

    def _divider(self, parent):
        ctk.CTkFrame(parent, height=max(1, self.style.border_width), fg_color=self.style.card_border).pack(
            fill="x", pady=16
        )

    def _chips(self, parent, title, values, color=None):
        if not values:
            return
        style = self.style
        row = ctk.CTkFrame(parent, fg_color="transparent")
        row.pack(fill="x", pady=(6, 0))
        ctk.CTkLabel(
            row,
            text=style.heading(title) + ":",
            text_color=style.text_muted,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["chip_label"]),
        ).pack(side="left", padx=(0, 8))
        for value in values:
            ctk.CTkLabel(
                row,
                text=f"  {value}  ",
                fg_color=style.chip_bg,
                text_color=color or style.accent_soft,
                font=style.font(ui_styles.SIZE["chip"], "bold"),
            ).pack(side="left", padx=3)

    def _build_info(self, entry):
        self._clear_info()
        style = self.style

        content = ctk.CTkScrollableFrame(
            self.info_frame,
            width=INFO_WIDTH,
            height=INFO_HEIGHT,
            fg_color="transparent",
            scrollbar_button_color=style.card_border,
            scrollbar_button_hover_color=style.chip_hover,
        )
        content.pack(fill="both", expand=True)

        header = ctk.CTkFrame(content, fg_color="transparent")
        header.pack(fill="x")
        ctk.CTkLabel(
            header,
            text=entry.get("word", ""),
            text_color=style.text_primary,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["info_word"], "bold", display=True),
            anchor="w",
        ).pack(side="left")

        audio_path = dictionary.resolve_audio(entry)
        if audio_path is not None and self.on_listen is not None:
            self.listen_button = ctk.CTkButton(
                header,
                text="Listen",
                width=120,
                height=42,
                fg_color=style.secondary_button_bg,
                hover_color=style.secondary_button_hover,
                border_width=style.border_width,
                border_color=style.secondary_button_border,
                text_color=style.secondary_button_text,
                font=style.font(ui_styles.SIZE["button"], "bold"),
                command=lambda p=audio_path: self._speak(
                    p, self.listen_button, idle_text="Listen", prefix="Speaking"
                ),
            )
            self.listen_button.pack(side="right", pady=(12, 0), padx=(0, 20))
        else:
            self.listen_button = None

        self.ignore_button = ctk.CTkButton(
            header,
            text="Ignore",
            width=120,
            height=42,
            fg_color="transparent",
            hover_color=style.chip_hover,
            border_width=style.border_width,
            border_color=style.secondary_button_border,
            text_color=style.secondary_button_text,
            font=style.font(ui_styles.SIZE["button"], "bold"),
            command=self._ignore_current,
        )
        self.ignore_button.pack(side="right", pady=(12, 0), padx=(0, 10))

        pos = entry.get("part_of_speech", "")
        ipa = entry.get("pronunciation", {}).get("ipa", "")
        if pos or ipa:
            meta = ctk.CTkFrame(content, fg_color="transparent")
            meta.pack(anchor="w", pady=(4, 0))
            if pos:
                ctk.CTkLabel(
                    meta,
                    text=style.heading(pos),
                    text_color=style.accent,
                    fg_color="transparent",
                    font=style.font(ui_styles.SIZE["button"], "bold"),
                ).pack(side="left", padx=(0, 14))
            if ipa:
                ctk.CTkLabel(
                    meta,
                    text=ipa,
                    text_color=style.text_muted,
                    fg_color="transparent",
                    font=style.font(ui_styles.SIZE["translation"]),
                ).pack(side="left")

        self._divider(content)

        definitions = entry.get("definitions", [])
        if definitions:
            self._section_title(content, "Meaning")
            for definition in definitions:
                if definition.get("english"):
                    ctk.CTkLabel(
                        content,
                        text=definition["english"],
                        text_color=style.text_body,
                        fg_color="transparent",
                        font=style.font(ui_styles.SIZE["definition"]),
                        justify="left",
                        anchor="w",
                        wraplength=WRAP_LENGTH,
                    ).pack(fill="x", pady=(0, 4))
                if definition.get("vietnamese"):
                    ctk.CTkLabel(
                        content,
                        text=definition["vietnamese"],
                        text_color=style.text_primary,
                        fg_color="transparent",
                        font=style.font(ui_styles.SIZE["definition"], "bold"),
                        justify="left",
                        anchor="w",
                        wraplength=WRAP_LENGTH,
                    ).pack(fill="x", pady=(0, 4))

        examples = entry.get("examples", [])
        if examples:
            self._section_title(content, "Examples")
            for example in examples:
                if example.get("english"):
                    row = ctk.CTkFrame(content, fg_color="transparent")
                    row.pack(anchor="w", pady=(0, 2))
                    # Text packs first so the button always sits right after it;
                    # because the row hugs its content the button can never
                    # overlap the label, and wrapped text pushes it down.
                    ctk.CTkLabel(
                        row,
                        text="•  " + example["english"],
                        text_color=style.text_body,
                        fg_color="transparent",
                        font=style.font(ui_styles.SIZE["example"]),
                        justify="left",
                        anchor="w",
                        wraplength=WRAP_LENGTH - 140,
                    ).pack(side="left")
                    # Per-example speak button, packed right after the text.
                    example_audio = dictionary.resolve_example_audio(example)
                    speak_button = ctk.CTkButton(
                        row,
                        text="Speak",
                        width=78,
                        height=26,
                        fg_color=style.secondary_button_bg,
                        hover_color=style.secondary_button_hover,
                        border_width=style.border_width,
                        border_color=style.secondary_button_border,
                        text_color=style.secondary_button_text,
                        font=style.font(ui_styles.SIZE["small"], "bold"),
                        command=lambda: None,
                    )
                    speak_button.pack(side="left", padx=(8, 0), pady=(2, 0))
                    if example_audio is not None and self.on_listen is not None:
                        speak_button.configure(
                            command=lambda p=example_audio, b=speak_button: self._speak(
                                p, b, idle_text="Speak", prefix=""
                            )
                        )
                    else:
                        speak_button.configure(state="disabled")
                if example.get("vietnamese"):
                    ctk.CTkLabel(
                        content,
                        text="    " + example["vietnamese"],
                        text_color=style.text_muted,
                        fg_color="transparent",
                        font=style.font(ui_styles.SIZE["translation"]),
                        justify="left",
                        anchor="w",
                        wraplength=WRAP_LENGTH,
                    ).pack(fill="x", pady=(0, 8))

        self._chips(content, "Synonyms", entry.get("synonyms", []))
        self._chips(content, "Antonyms", entry.get("antonyms", []), color=style.error)
        self._chips(content, "Tags", entry.get("tags", []), color=style.text_muted)

        ctk.CTkFrame(
            self.info_frame,
            height=max(1, style.border_width),
            fg_color=style.card_border,
        ).pack(fill="x", pady=(14, 12))
        ctk.CTkLabel(
            self.info_frame,
            text=style.heading("Enter to exit"),
            text_color=style.accent_soft,
            fg_color="transparent",
            font=style.font(ui_styles.SIZE["enter"], "bold"),
        ).pack(pady=(0, 2))

    def _ignore_current(self):
        """Add the displayed word to IGNORE_WORDS in config.json.

        On the typing screen the app then closes like a normal exit; on the
        information card the app stays open and the button reports success.
        """
        word = str((self.current or {}).get("word", "")).strip()
        if not word:
            return

        try:
            saved = True if self.on_ignore is None else self.on_ignore(word)
        except Exception:
            saved = False

        if saved is False:
            if self.state == self.QUIZ:
                self.feedback_label.configure(
                    text="Could not save IGNORE_WORDS", text_color=self.style.error
                )
            elif self._button_alive(self.ignore_button):
                try:
                    self.ignore_button.configure(text="Ignore failed")
                except tk.TclError:
                    pass
            return

        if self.state == self.QUIZ:
            self.on_exit()
            return

        if self._button_alive(self.ignore_button):
            try:
                self.ignore_button.configure(text="Ignored", state="disabled")
            except tk.TclError:
                pass

    def _button_alive(self, button):
        """True while a Tk widget still exists (it may be destroyed by a rebuild)."""
        if button is None:
            return False
        try:
            return bool(button.winfo_exists())
        except (tk.TclError, AttributeError):
            return False

    def _speak(self, audio_path, button, idle_text="Listen", prefix="Speaking"):
        """Play ``audio_path`` and animate ``button`` with cycling dots.

        Only one button speaks at a time:
        * another button stops the current sound and resets its label,
        * clicking the speaking button again stops it (toggle),
        * rapid repeat clicks on the same button are debounced so the player
          is never restarted in a tight loop.
        """
        if self.on_listen is None or not self._button_alive(button):
            return

        now = time.monotonic()
        if (
            button is self._speaking_button
            and (now - self._speaking_started_at) * 1000.0 < SPEAK_DEBOUNCE_MS
        ):
            return

        if button is self._speaking_button:
            self._stop_speaking(stop_audio=True)
            return

        self._stop_speaking()
        try:
            started = self.on_listen(audio_path)
        except Exception:
            started = False

        if started is False or not self._button_alive(button):
            return

        self._speaking_button = button
        self._speaking_idle_text = idle_text
        self._speaking_prefix = prefix
        self._speaking_dots = 1
        self._speaking_started_at = now
        self._set_speaking_text()
        self._speaking_after = self.root.after(
            SPEAK_ANIMATION_MS, self._animate_speaking
        )

    def _set_speaking_text(self):
        button = self._speaking_button
        if not self._button_alive(button):
            self._forget_speaking()
            return
        text = "." * max(1, self._speaking_dots)
        if self._speaking_prefix:
            text = self._speaking_prefix + text
        try:
            button.configure(text=text)
        except tk.TclError:
            self._forget_speaking()

    def _animate_speaking(self):
        self._speaking_after = None
        if not self._button_alive(self._speaking_button):
            self._forget_speaking()
            return

        try:
            playing = self.is_playing() if self.is_playing is not None else False
        except Exception:
            playing = False

        if not playing:
            # Audio finished, failed, or was stopped elsewhere: restore label.
            self._stop_speaking()
            return

        self._speaking_dots = self._speaking_dots % 3 + 1
        self._set_speaking_text()
        if self._speaking_button is None:
            return
        self._speaking_after = self.root.after(
            SPEAK_ANIMATION_MS, self._animate_speaking
        )

    def _forget_speaking(self):
        """Drop animation state without touching widgets (they may be gone)."""
        self._speaking_button = None
        self._speaking_dots = 1
        self._speaking_prefix = ""
        self._speaking_idle_text = "Listen"
        self._speaking_started_at = 0.0

    def _stop_speaking(self, reset=True, stop_audio=False):
        """Cancel the dot animation and optionally silence the player."""
        if self._speaking_after is not None:
            try:
                self.root.after_cancel(self._speaking_after)
            except Exception:
                pass
            self._speaking_after = None

        button = self._speaking_button
        idle_text = self._speaking_idle_text
        self._forget_speaking()

        if reset and self._button_alive(button):
            try:
                button.configure(text=idle_text)
            except tk.TclError:
                pass

        if stop_audio and self.on_stop is not None:
            try:
                self.on_stop()
            except Exception:
                pass

    def _finish(self):
        """Correct answer: record progress, then show the word information card."""
        self._cancel_finish_timer()
        self.state = self.DONE
        self._finished_at = time.monotonic()
        self.cancel_focus()

        if self.on_correct is not None:
            try:
                self.on_correct((self.current or {}).get("word", ""))
            except Exception:
                # Progress tracking must never block the word gate.
                pass

        self.quiz_frame.pack_forget()
        self._build_info(self.current)
        self.info_frame.pack(padx=64, pady=44)

        # Play the pronunciation once automatically when the card appears.
        audio_path = dictionary.resolve_audio(self.current)
        if audio_path is not None and self.listen_button is not None:
            self._speak(
                audio_path, self.listen_button, idle_text="Listen", prefix="Speaking"
            )

    # ---------- key handling ----------

    def _cancel_finish_timer(self):
        if self._finish_timer is not None:
            try:
                self.root.after_cancel(self._finish_timer)
            except Exception:
                pass
            self._finish_timer = None

    def _schedule_finish(self):
        """Hold the completed word on screen briefly before switching cards."""
        if self._finish_timer is not None:
            return
        self.feedback_label.configure(text="Correct!", text_color=self.style.success)
        self.answer_entry.configure(border_color=self.style.success)
        self._finish_timer = self.root.after(AUTO_ACCEPT_DELAY_MS, self._finish)

    def _on_answer_changed(self, *_args):
        """Finish shortly after the typed word matches, so the last letter paints."""
        if self.state != self.QUIZ:
            return
        typed = self.answer_var.get().strip().lower()
        expected = str((self.current or {}).get("word", "")).strip().lower()
        if typed and expected and typed == expected:
            self._schedule_finish()
        else:
            # The answer was edited after matching; let the user keep typing.
            self._cancel_finish_timer()
            self.feedback_label.configure(text="", text_color=self.style.error)
            self.answer_entry.configure(border_color=self.style.accent)

    def _on_enter(self, _event=None):
        if self.state == self.QUIZ:
            typed = self.answer_var.get().strip().lower()
            expected = str((self.current or {}).get("word", "")).strip().lower()
            if typed and typed == expected:
                self._schedule_finish()
            else:
                self._cancel_finish_timer()
                self.feedback_label.configure(
                    text="Incorrect, try again", text_color=self.style.error
                )
                self.answer_var.set("")
                self.focus_answer()
        else:
            # Ignore a reflex Enter right after the word was auto-accepted, so
            # the information card is not dismissed before it can be read.
            if time.monotonic() - self._finished_at < 0.5:
                return
            self.on_exit()
            if self.on_style_cycle is not None:
                # Rebuild a moment after the window is withdrawn. CustomTkinter
                # queues focus work while hiding; destroying the widgets first
                # would leave that queued callback pointing at a dead entry.
                self.root.after(60, self.on_style_cycle)
