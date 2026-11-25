const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');

function read(rel) {
  return fs.readFileSync(path.join(root, rel), 'utf8');
}

function assert(cond, msg) {
  if (!cond) {
    console.error('FAIL:', msg);
    process.exitCode = 1;
  } else {
    console.log('ok -', msg);
  }
}

try {
  const styles = read('resources/styles.css');
  assert(styles.trim().length === 0, 'resources/styles.css is empty (all CSS removed)');

  const html = read('resources/index.html');
  assert(html.includes('cdn.tailwindcss.com'), 'index.html includes Tailwind CDN');
  assert(html.includes('jquery-3.7.1.min.js'), 'index.html includes jQuery CDN');
  assert(html.includes('lodash@4.17.21'), 'index.html includes Lodash CDN');
  assert(html.includes('Bottom left'), 'index.html contains bottom-right label "Bottom left"');

  const cfg = JSON.parse(read('neutralino.config.json'));
  assert(cfg.modes && cfg.modes.window && cfg.modes.window.hidden === false, 'neutralino.config.json window.hidden is false');

  const mainJs = read('resources/js/main.js');
  assert(mainJs.includes('openHelloWindow'), 'main.js defines openHelloWindow');
  assert(mainJs.includes('1280') && mainJs.includes('720'), 'main.js references 1280x720 window size');
  assert(mainJs.includes('Hello world'), 'main.js contains "Hello world" for the popup window');

  if (process.exitCode && process.exitCode !== 0) {
    console.error('\nSome checks failed.');
    process.exit(1);
  } else {
    console.log('\nAll smoke checks passed.');
  }
} catch (e) {
  console.error('Test error:', e.message);
  process.exit(1);
}
