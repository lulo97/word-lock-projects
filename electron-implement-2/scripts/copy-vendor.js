const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const vendorDir = path.join(root, 'vendor');

const files = [
  ['node_modules/jquery/dist/jquery.min.js', 'jquery.min.js'],
  ['node_modules/lodash/lodash.min.js', 'lodash.min.js'],
];

fs.mkdirSync(vendorDir, { recursive: true });

for (const [from, to] of files) {
  fs.copyFileSync(path.join(root, from), path.join(vendorDir, to));
}

console.log('Copied jQuery and lodash into vendor/');
