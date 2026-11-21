import atexit
import queue
import sys
import time

import customtkinter as ctk

import audio_player
import dictionary
import overlay
import progress
import settings
import settings_ui
import startup
import tray_icon
import ui
import ui_styles
import window
from chrome import ChromeBlocker


def _configure_utf8_streams():
    """Never let a Vietnamese log line die on a legacy console code page."""
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError, OSError):
            pass


class WordLockApp:
    def __init__(self):
        self.root = ctk.CTk()
        ui.setup_root(self.root)

        self.chrome = ChromeBlocker()
        atexit.register(self.chrome.shutdown)

        self.player = audio_player.AudioPlayer()

        self.ui = ui.WordGateUI(
            self.root,
            on_exit=self.hide,
            on_correct=progress.record_success,
            on_listen=self.player.play,
            is_playing=self.player.is_playing,
            on_style_cycle=self.cycle_style,
            on_stop=self.player.stop,
            on_ignore=settings.add_ignored_word,
        )
        self.overlay = overlay.CountdownOverlay(self.root, ui_styles.current())
        self.settings_dialog = None
        self.running = True
        self.countdown_enabled = True
        self.next_show_at = None
        self.overlay_timer = None
        self.events = queue.Queue()
        self.tray = None
        self.show_timer = None
        self.visible = False
        self.show_sequence = 0

        self.root.protocol("WM_DELETE_WINDOW", self.hide)

        # Create %USERPROFILE%/wordlock/config.json on first run.
        settings.load()

        # Honour the saved preference. This also repairs the Run entry when the
        # exe was moved or renamed since the last launch (Windows never updates
        # the stored path itself).
        if settings.start_with_windows():
            startup.enable()

        # Tray callbacks only enqueue work; Tk runs on the main thread.
        self.tray = tray_icon.start_tray(
            self._tray_open,
            self._tray_next_style,
            self._tray_settings,
            self._tray_exit,
            lambda: self.running,
            self._tray_toggle_running,
            lambda: self.countdown_enabled,
            self._tray_toggle_countdown,
        )

        # Start hidden in the tray, with the countdown running.
        self.root.withdraw()
        self._schedule_auto_show()
        self.root.after(100, self._poll_events)

    # ---------- tray -> main thread ----------

    def _tray_open(self):
        self.events.put("open")

    def _tray_next_style(self):
        self.events.put("next_style")

    def _tray_settings(self):
        self.events.put("settings")

    def _tray_exit(self):
        self.events.put("exit")

    def _tray_toggle_countdown(self):
        self.events.put("toggle_countdown")

    def _tray_toggle_running(self):
        self.events.put("toggle_running")

    def _poll_events(self):
        while True:
            try:
                action = self.events.get_nowait()
            except queue.Empty:
                break
            if action == "open":
                self.show()
            elif action == "next_style":
                self.cycle_style()
            elif action == "settings":
                self._open_settings()
            elif action == "toggle_running":
                self._toggle_running()
            elif action == "toggle_countdown":
                self._toggle_countdown()
            elif action == "exit":
                self.quit()
                return
        self.root.after(100, self._poll_events)

    # ---------- countdown ----------

    def _cancel_auto_show(self):
        if self.show_timer is not None:
            try:
                self.root.after_cancel(self.show_timer)
            except Exception:
                pass
            self.show_timer = None
        self.next_show_at = None
        self._stop_overlay_clock()

    def _stop_overlay_clock(self):
        if self.overlay_timer is not None:
            try:
                self.root.after_cancel(self.overlay_timer)
            except Exception:
                pass
            self.overlay_timer = None
        self.overlay.hide()

    def _schedule_auto_show(self):
        self._cancel_auto_show()
        if not self.running or self.settings_dialog is not None:
            return
        delay_ms = settings.auto_show_delay_ms()
        self.next_show_at = time.monotonic() + delay_ms / 1000.0
        self.show_timer = self.root.after(delay_ms, self._auto_show)
        self._update_overlay()

    def _auto_show(self):
        self.show_timer = None
        if self.running:
            self.show()

    def _toggle_running(self):
        """Tray toggle: stop or resume the word gate and its countdown."""
        self.running = not self.running
        if self.running:
            self._schedule_auto_show()
        else:
            self._cancel_auto_show()
            if self.visible:
                self.hide()

        if self.tray is not None:
            try:
                self.tray.update_menu()
            except Exception:
                pass

    def _toggle_countdown(self):
        """Tray toggle: show or hide the bottom-right countdown card."""
        self.countdown_enabled = not self.countdown_enabled
        if self.countdown_enabled:
            self._update_overlay()
        else:
            self._stop_overlay_clock()

        if self.tray is not None:
            try:
                self.tray.update_menu()
            except Exception:
                pass

    def _update_overlay(self):
        """Refresh the countdown text and schedule the next tick."""
        self.overlay_timer = None
        if (
            not self.running
            or self.settings_dialog is not None
            or not self.countdown_enabled
            or self.visible
            or self.next_show_at is None
        ):
            self.overlay.hide()
            return

        self.overlay.set_remaining(self.next_show_at - time.monotonic())
        self.overlay.show()
        self.overlay_timer = self.root.after(250, self._update_overlay)

    # ---------- UI style ----------

    def cycle_style(self):
        """Swap to a random style (called after a word card is dismissed)."""
        style = ui_styles.random_style(exclude=ui_styles.current())
        self.ui.apply_style(style)
        self.overlay.apply_style(style)
        if self.tray is not None:
            try:
                self.tray.update_menu()
            except Exception:
                pass

    # ---------- settings ----------

    def _open_settings(self):
        """Open the Settings modal, pausing the countdown while it is open."""
        if self.settings_dialog is not None:
            self.settings_dialog.focus()
            return

        self._cancel_auto_show()
        self.settings_dialog = settings_ui.SettingsDialog(
            self.root, on_close=self._settings_closed
        )

    def _settings_closed(self):
        self.settings_dialog = None
        if self.running and not self.visible:
            self._schedule_auto_show()

    # ---------- window state ----------

    def show(self):
        """Open in front, start a word round, and freeze Chrome.

        The window is topmost only for a moment so it is seen immediately;
        it is then released so Windows users can switch to other apps normally.
        """
        if self.settings_dialog is not None:
            self.settings_dialog.focus()
            return

        self._cancel_auto_show()
        if not self.running:
            return
        if not self.visible:
            self.player.stop()
            # Least-practiced word first so the whole collection is learned evenly.
            self.ui.show_question(
                dictionary.load_next_entry(
                    progress.load_attempts(), settings.ignored_words()
                )
            )

        self.root.deiconify()
        window.maximize(self.root)
        try:
            self.root.attributes("-topmost", True)
        except ctk.TclError:
            pass
        self.root.lift()
        self.root.focus_force()
        window.force_foreground(self.root)

        self.show_sequence += 1
        sequence = self.show_sequence
        self.root.after(600, lambda: self._release_topmost(sequence))

        self.visible = True
        self.chrome.freeze()

        # Windows can reject the first foreground request while the window is
        # still being mapped/maximized. Retry activation together with entry
        # focus so typing works without an initial mouse click.
        for delay in (0, 120, 400, 900):
            self.root.after(delay, lambda s=sequence: self._activate(s))

    def _activate(self, sequence):
        """Re-assert OS foreground ownership and answer-box focus."""
        if sequence != self.show_sequence or not self.visible:
            return
        if not window.is_foreground(self.root):
            window.force_foreground(self.root)
        self.ui.focus_answer()

    def _release_topmost(self, sequence):
        """Let other windows come above the app after the initial reveal."""
        if sequence != self.show_sequence:
            return
        try:
            self.root.attributes("-topmost", False)
        except ctk.TclError:
            pass

    def hide(self):
        """Minimize to tray, resume Chrome, and restart the 1-minute countdown."""
        self.show_sequence += 1
        try:
            self.root.attributes("-topmost", False)
        except ctk.TclError:
            pass
        self.ui.cancel_focus()
        self.player.stop()
        self.root.withdraw()
        self.visible = False
        self.chrome.unfreeze()
        if self.running:
            self._schedule_auto_show()

    def quit(self):
        self._cancel_auto_show()
        self.player.stop()
        self.overlay.destroy()
        if self.tray is not None:
            try:
                self.tray.stop()
            except Exception:
                pass
            self.tray = None
        self.chrome.shutdown()
        self.root.after(0, self.root.destroy)

    def run(self):
        self.root.mainloop()


def main():
    _configure_utf8_streams()
    WordLockApp().run()


if __name__ == "__main__":
    main()
