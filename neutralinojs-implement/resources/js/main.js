Neutralino.init();

async function setAppFromScaledCenter() {
  try {
    const scaledWidth = window.screen.width;
    const scaledHeight = window.screen.height;
    const scale = window.devicePixelRatio || 1;
    const appWidth = 1280;
    const appHeight = 720;
    const posCalc = centerPhysicalFromScaled(scaledWidth, scaledHeight, scale, appWidth, appHeight);
    try { Neutralino.debug.log(`scaledWidth=${scaledWidth}, scaledHeight=${scaledHeight}, scale=${scale}, appWidth=${appWidth}, appHeight=${appHeight}`); } catch (e) {}
    try { Neutralino.debug.log(`centerPhysicalFromScaled -> x=${posCalc.x}, y=${posCalc.y}`); } catch (e) {}
    await Neutralino.window.setSize({ width: appWidth, height: appHeight, resizable: true });
    await Neutralino.window.move(posCalc.x, posCalc.y);
    const pos = await Neutralino.window.getPosition();
    const size = await Neutralino.window.getSize();
    try { Neutralino.debug.log(`After move: x=${pos.x}, y=${pos.y}, size=${size.width}x${size.height}`); } catch (e) {}
  } catch (e) {
    try { Neutralino.debug.log('setAppFromScaledCenter error: ' + String(e)); } catch (e2) {}
  }
}

setAppFromScaledCenter();

function onWindowClose() {
  Neutralino.app.exit();
}

Neutralino.events.on('windowClose', onWindowClose);
