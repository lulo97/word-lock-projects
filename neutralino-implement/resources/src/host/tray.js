import { showMainWindow, createSettingsWindow, quitAll } from './windows.js';

const MENU_ITEMS = [
  { id: 'SHOW', text: 'Show App' },
  { text: '-' },
  { id: 'SETTINGS', text: 'Settings' },
  { text: '-' },
  { id: 'QUIT', text: 'Quit' },
];

export async function createTray() {
  const candidates = ['/resources/assets/icon.png', '/assets/icon.png'];
  let installed = false;
  for (const icon of candidates) {
    try {
      await Neutralino.os.setTray({ icon, menuItems: MENU_ITEMS });
      installed = true;
      break;
    } catch {
      // Different Neutralino releases resolve tray icons from different roots.
    }
  }
  if (!installed) return;

  Neutralino.events.on('trayMenuItemClicked', ({ detail }) => {
    if (detail.id === 'SHOW') void showMainWindow();
    if (detail.id === 'SETTINGS') void createSettingsWindow();
    if (detail.id === 'QUIT') quitAll();
  });
}
