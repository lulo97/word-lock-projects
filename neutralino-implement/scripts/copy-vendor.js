const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const vendorDir = path.join(root, 'resources', 'vendor');
const mediapipe = path.join(root, 'node_modules', '@mediapipe', 'tasks-vision');

fs.mkdirSync(vendorDir, { recursive: true });

function copyFile(from, to) {
  const source = path.join(root, from);
  if (!fs.existsSync(source)) {
    console.warn(`copy-vendor: missing ${from}`);
    return;
  }
  const destination = path.join(vendorDir, to);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  fs.copyFileSync(source, destination);
}

// MediaPipe is served over the normal http origin now, so app:// is gone.
// WebView2 always has WASM SIMD: copy only the SIMD build, never `nosimd`.
copyFile('node_modules/@mediapipe/tasks-vision/vision_bundle.mjs', 'mediapipe/vision_bundle.mjs');

const wasmDir = path.join(mediapipe, 'wasm');
if (fs.existsSync(wasmDir)) {
  const target = path.join(vendorDir, 'mediapipe', 'wasm');
  fs.mkdirSync(target, { recursive: true });
  for (const name of fs.readdirSync(wasmDir)) {
    if (name.includes('nosimd')) continue;
    if (!/\.(js|wasm)$/.test(name)) continue;
    fs.copyFileSync(path.join(wasmDir, name), path.join(target, name));
  }
} else {
  console.warn('copy-vendor: MediaPipe wasm folder is missing; run npm install first.');
}

console.log('Copied MediaPipe into resources/vendor/mediapipe/ (SIMD only).');
