const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const vendorDir = path.join(root, 'vendor');

const files = [
  ['node_modules/jquery/dist/jquery.min.js', 'jquery.min.js'],
  ['node_modules/lodash/lodash.min.js', 'lodash.min.js'],
  [
    'node_modules/@mediapipe/tasks-vision/vision_bundle.mjs',
    'mediapipe/vision_bundle.mjs',
  ],
];

const directories = [
  ['node_modules/@mediapipe/tasks-vision/wasm', 'mediapipe/wasm'],
];

fs.mkdirSync(vendorDir, { recursive: true });

for (const [from, to] of files) {
  const destination = path.join(vendorDir, to);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  fs.copyFileSync(path.join(root, from), destination);
}

for (const [from, to] of directories) {
  fs.cpSync(path.join(root, from), path.join(vendorDir, to), { recursive: true });
}

console.log('Copied jQuery, lodash and MediaPipe into vendor/');
