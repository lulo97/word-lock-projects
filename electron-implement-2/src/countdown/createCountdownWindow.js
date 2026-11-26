const { BrowserWindow, screen } = require('electron');
const path = require('node:path');
const { iconPath } = require('../appIcon');

const WIDTH = 100;
const HEIGHT = 50;

let countdownWindow = null;

function createCountdownWindow() {
  if (countdownWindow) return countdownWindow;

  const { x, y, width, height } = screen.getPrimaryDisplay().workArea;

  countdownWindow = new BrowserWindow({
    width: WIDTH,
    height: HEIGHT,
    x: x + width - WIDTH - 10,
    y: y + height - HEIGHT - 10,
    frame: false,
    roundedCorners: false,
    alwaysOnTop: true,
    opacity: 0.5,
    icon: iconPath,
    webPreferences: {
      preload: path.join(__dirname, '..', 'preload.js'),
    },
  });

  countdownWindow.loadFile(path.join(__dirname, 'countdown.html'));
  countdownWindow.setAlwaysOnTop(true, 'screen-saver');

  countdownWindow.on('closed', () => {
    countdownWindow = null;
  });

  return countdownWindow;
}

function closeCountdownWindow() {
  if (!countdownWindow) return;

  countdownWindow.destroy();
  countdownWindow = null;
}

module.exports = { createCountdownWindow, closeCountdownWindow };
