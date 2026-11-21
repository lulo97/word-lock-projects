import platform

from config import ASSETS_DIR


def maximize(root):
    system = platform.system()
    if system == "Windows":
        root.state("zoomed")
    elif system == "Linux":
        try:
            root.attributes("-zoomed", True)
        except Exception:
            root.geometry(f"{root.winfo_screenwidth()}x{root.winfo_screenheight()}+0+0")
    else:
        # macOS doesn't use "zoomed" the same way; set to full screen work area
        root.geometry(f"{root.winfo_screenwidth()}x{root.winfo_screenheight()}+0+0")


_WINDOWS_API = None


def _windows_api():
    """Build one set of correctly-typed Win32 functions (64-bit safe)."""
    global _WINDOWS_API
    if _WINDOWS_API is not None:
        return _WINDOWS_API

    import ctypes
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    kernel32 = ctypes.windll.kernel32

    user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
    user32.GetAncestor.restype = wintypes.HWND
    user32.GetForegroundWindow.restype = wintypes.HWND
    user32.GetWindowThreadProcessId.argtypes = [
        wintypes.HWND, ctypes.POINTER(wintypes.DWORD)
    ]
    user32.GetWindowThreadProcessId.restype = wintypes.DWORD
    user32.AttachThreadInput.argtypes = [
        wintypes.DWORD, wintypes.DWORD, wintypes.BOOL
    ]
    user32.AttachThreadInput.restype = wintypes.BOOL
    user32.SetForegroundWindow.argtypes = [wintypes.HWND]
    user32.SetForegroundWindow.restype = wintypes.BOOL
    user32.BringWindowToTop.argtypes = [wintypes.HWND]
    user32.BringWindowToTop.restype = wintypes.BOOL
    user32.SetActiveWindow.argtypes = [wintypes.HWND]
    user32.SetActiveWindow.restype = wintypes.HWND
    user32.SetFocus.argtypes = [wintypes.HWND]
    user32.SetFocus.restype = wintypes.HWND
    user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.ShowWindow.restype = wintypes.BOOL
    user32.keybd_event.argtypes = [
        wintypes.BYTE, wintypes.BYTE, wintypes.DWORD, ctypes.c_size_t
    ]
    user32.keybd_event.restype = None
    kernel32.GetCurrentThreadId.restype = wintypes.DWORD

    switch = getattr(user32, "SwitchToThisWindow", None)
    if switch is not None:
        switch.argtypes = [wintypes.HWND, wintypes.BOOL]
        switch.restype = None

    _WINDOWS_API = {
        "user32": user32,
        "kernel32": kernel32,
        "switch": switch,
    }
    return _WINDOWS_API


def is_foreground(root):
    """Return True when this app owns the Windows foreground/keyboard focus."""
    if platform.system() != "Windows":
        return True
    try:
        api = _windows_api()
        user32 = api["user32"]
        hwnd = user32.GetAncestor(root.winfo_id(), 2)  # GA_ROOT
        return bool(hwnd) and user32.GetForegroundWindow() == hwnd
    except Exception:
        return True


def force_foreground(root):
    """Windows: reliably make the app own keyboard focus, not just Tk focus.

    Tk's focus_force() only changes Tk's internal focus. Windows has a
    foreground lock that ignores SetForegroundWindow() from a background
    process, which is why a mouse click was needed. Attaching to the current
    foreground thread's input queue lifts that restriction; the Alt-key
    nudge is a fallback for the remaining cases.
    """
    if platform.system() != "Windows":
        return True

    try:
        api = _windows_api()
        user32 = api["user32"]
        kernel32 = api["kernel32"]

        hwnd = user32.GetAncestor(root.winfo_id(), 2)  # GA_ROOT
        if not hwnd:
            return False

        # Show/restore without resetting the maximized state.
        user32.ShowWindow(hwnd, 5)  # SW_SHOW
        user32.BringWindowToTop(hwnd)

        foreground = user32.GetForegroundWindow()
        current_thread = kernel32.GetCurrentThreadId()
        target_thread = user32.GetWindowThreadProcessId(hwnd, None)
        foreground_thread = (
            user32.GetWindowThreadProcessId(foreground, None) if foreground else 0
        )

        attached = []
        for thread in (foreground_thread, target_thread):
            if (
                thread
                and thread != current_thread
                and thread not in attached
                and user32.AttachThreadInput(current_thread, thread, True)
            ):
                attached.append(thread)

        try:
            user32.SetForegroundWindow(hwnd)
            user32.SetActiveWindow(hwnd)
            user32.SetFocus(hwnd)
        finally:
            for thread in attached:
                user32.AttachThreadInput(current_thread, thread, False)

        if user32.GetForegroundWindow() == hwnd:
            return True

        # Fallback: simulate an Alt press so Windows treats this process as
        # having received input, then switch to the window directly.
        VK_MENU = 0x12
        KEYEVENTF_KEYUP = 0x0002
        try:
            user32.keybd_event(VK_MENU, 0, 0, 0)
            switch = api["switch"]
            if switch is not None:
                switch(hwnd, True)
            user32.SetForegroundWindow(hwnd)
            user32.SetActiveWindow(hwnd)
            user32.SetFocus(hwnd)
        finally:
            user32.keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0)

        return user32.GetForegroundWindow() == hwnd
    except Exception:
        return False


def square_corners(window):
    """Ask Windows 11 not to round the native window corners."""
    if platform.system() != "Windows":
        return False
    try:
        import ctypes
        from ctypes import wintypes

        user32 = ctypes.windll.user32
        user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
        user32.GetAncestor.restype = wintypes.HWND

        hwnd = user32.GetAncestor(window.winfo_id(), 2)  # GA_ROOT
        if not hwnd:
            return False

        # DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_DONOTROUND = 1
        preference = ctypes.c_int(1)
        result = ctypes.windll.dwmapi.DwmSetWindowAttribute(
            hwnd, 33, ctypes.byref(preference), ctypes.sizeof(preference)
        )
        return result == 0
    except Exception:
        return False


def set_window_icon(root):
    """Use the generated icon for the taskbar and window title bar."""
    try:
        if platform.system() == "Windows":
            ico = ASSETS_DIR / "icon.ico"
            if ico.exists():
                root.iconbitmap(default=str(ico))
                return
    except Exception:
        pass

    try:
        from PIL import Image, ImageTk

        png = ASSETS_DIR / "icon.png"
        if png.exists():
            image = ImageTk.PhotoImage(Image.open(png))
            root.iconphoto(True, image)
            root._icon_image = image  # keep a reference
    except Exception:
        pass
