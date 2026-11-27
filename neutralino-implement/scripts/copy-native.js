const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const from = path.join(root, 'native', 'target', 'release', 'wordlock-native.exe');
const to = path.join(root, 'extensions', 'wordlock-native.exe');

if (!fs.existsSync(from)) {
  console.error(`copy-native: ${from} not found. Did cargo build --release succeed?`);
  process.exit(1);
}

fs.mkdirSync(path.dirname(to), { recursive: true });
fs.copyFileSync(from, to);
console.log('Copied wordlock-native.exe into extensions/');
