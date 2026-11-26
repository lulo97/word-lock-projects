const path = require('node:path');
const { nativeImage } = require('electron');

// One icon for the tray, every window, and the Windows taskbar grouping.
const iconPath = path.join(__dirname, '..', 'assets', 'icon.ico');
const icon = nativeImage.createFromPath(iconPath);

module.exports = { iconPath, icon };
