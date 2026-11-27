const fs = require('node:fs');
const path = require('node:path');

// Reproducible, idempotent edits to the renderer HTML. The renderer logic is
// otherwise untouched: these pages still talk to window.electronAPI.
const root = path.join(__dirname, '..');
const srcDir = path.join(root, 'resources', 'src');
const BRIDGE = '<script src="../bridge/electronAPI.js"></script>';
const WS = "ws://127.0.0.1:*";

function walk(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      if (entry.name === 'bridge' || entry.name === 'host') continue;
      out.push(...walk(full));
    } else if (entry.name.endsWith('.html')) {
      out.push(full);
    }
  }
  return out;
}

function patch(html) {
  let next = html;

  // 1. The bridge must run before theme-boot.js so the first paint is themed.
  if (!next.includes('bridge/electronAPI.js')) {
    next = next.replace(/<head(?:\s[^>]*)?>/i, (head) => `${head}\n    ${BRIDGE}`);
  }

  // 2. Neutralino's IPC is a WebSocket on the same host. Some WebView2 builds
  //    do not treat ws:// as 'self', so spell it out in every CSP.
  next = next.replace(
    /(<meta\s+http-equiv="Content-Security-Policy"\s+content=")([^"]*)(")/i,
    (_match, open, policy, close) => {
      let updated = policy;
      if (!/connect-src/.test(updated)) {
        updated = updated.replace(/;$/, '') + `; connect-src 'self' ${WS}`;
      } else if (!updated.includes(WS)) {
        updated = updated.replace(/connect-src\s+([^;]*)/, (m, sources) =>
          `connect-src ${sources.trim()} ${WS}`,
        );
      }
      return `${open}${updated}${close}`;
    },
  );

  return next;
}

let changed = 0;
for (const file of walk(srcDir)) {
  const before = fs.readFileSync(file, 'utf8');
  const after = patch(before);
  if (after !== before) {
    fs.writeFileSync(file, after);
    changed += 1;
  }
}

console.log(`codemod-html: patched ${changed} HTML file(s).`);
