const { BrowserWindow, screen } = require('electron');
const path = require('node:path');

const WIDTH = 100;
const HEIGHT = 50;

let countdownWindow = null;

function createCountdownWindow() {
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
    webPreferences: {
      preload: path.join(__dirname, '..', 'preload.js'),
    },
  });

  countdownWindow.loadFile('countdown.html');
  countdownWindow.setAlwaysOnTop(true, 'screen-saver');

  return countdownWindow;
}

module.exports = { createCountdownWindow };
