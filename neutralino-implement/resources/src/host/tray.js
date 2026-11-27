import { showMainWindow, createSettingsWindow, quitAll } from './windows.js';

const MENU_ITEMS = [
  { id: 'SHOW', text: 'Show App' },
  { text: '-' },
  { id: 'SETTINGS', text: 'Settings' },
  { text: '-' },
  { id: 'QUIT', text: 'Quit' },
];

export async function createTray({ visibleFallback = true } = {}) {
  const candidates = ['/resources/assets/icon.png', '/assets/icon.png'];
  let lastError = null;

  for (const icon of candidates) {
    try {
      await Neutralino.os.setTray({ icon, menuItems: MENU_ITEMS });
      Neutralino.events.on('trayMenuItemClicked', ({ detail }) => {
        if (detail.id === 'SHOW') void showMainWindow();
        if (detail.id === 'SETTINGS') void createSettingsWindow();
        if (detail.id === 'QUIT') quitAll();
      });
      return true;
    } catch (error) {
      // Different Neutralino releases resolve tray icons from different roots.
      lastError = error;
    }
  }

  // Without a tray the borderless app has no quit affordance. Log the real
  // failure and offer a visible fallback instead of silently hiding it.
  console.warn('PomodoroLock could not install the tray icon:', lastError);
  if (visibleFallback) {
    void (async () => {
      try {
        const choice = await Neutralino.os.showMessageBox(
          'PomodoroLock',
          'The system tray is unavailable. There is no tray menu to quit the app. Quit now?',
          'OK_CANCEL',
          'WARNING',
        );
        if (choice === 'OK') await quitAll();
      } catch (error) {
        console.warn('PomodoroLock tray fallback failed:', error);
      }
    })();
  }
  return false;
}
