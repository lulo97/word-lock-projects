Neutralino.init();

function setTray() {
  if (NL_MODE !== 'window') return;
  try {
    Neutralino.os.setTray({
      icon: '/resources/icons/trayIcon.png',
      menuItems: [
        { id: 'SHOW', text: 'Show window' },
        { id: 'QUIT', text: 'Quit' }
      ]
    });
  } catch (e) {
    try { Neutralino.debug.log('setTray error: ' + String(e)); } catch (e2) {}
  }
}

function onTrayMenuItemClicked(event) {
  if (!event || !event.detail) return;
  if (event.detail.id === 'SHOW') {
    Neutralino.window.show();
    Neutralino.window.focus();
  } else if (event.detail.id === 'QUIT') {
    Neutralino.app.exit();
  }
}

function centerApp() {
  try {
    const scaledWidth = window.screen.width;
    const scaledHeight = window.screen.height;
    const scale = window.devicePixelRatio || 1;
    const appWidth = 1280;
    const appHeight = 720;
    const posCalc = centerPhysicalFromScaled(scaledWidth, scaledHeight, scale, appWidth, appHeight);
    Neutralino.window.setSize({ width: appWidth, height: appHeight, resizable: true }).then(() => {
      return Neutralino.window.move(posCalc.x, posCalc.y);
    }).catch(() => {});
  } catch (e) {}
}

centerApp();
setTray();

Neutralino.events.on('trayMenuItemClicked', onTrayMenuItemClicked);
Neutralino.events.on('windowClose', () => {
  Neutralino.window.hide();
});
