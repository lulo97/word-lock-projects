const fs = require('node:fs');
const path = require('node:path');

// Remove only generated/ignored build and test artifacts.
const root = path.join(__dirname, '..');
const targets = [
  'dist',
  'resources/resources.neu',
  'resources/assets/tailwind.css',
  'resources/assets/themes.css',
  'test-results',
  'playwright-report',
  'blob-report',
  'playwright/.cache',
  '.tmp',
];

for (const target of targets) {
  fs.rmSync(path.join(root, target), { recursive: true, force: true });
}

console.log('clean: removed generated build and test artifacts.');
