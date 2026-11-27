const fs = require('node:fs');
const path = require('node:path');

// Reproducible, idempotent edits to the renderer HTML. The renderer logic is
// otherwise untouched: these pages still talk to window.electronAPI.
const root = path.join(__dirname, '..');
const srcDir = path.join(root, 'resources', 'src');
const BRIDGE = '<script src="../bridge/electronAPI.js"></script>';
const BRIDGE_LINE = `  ${BRIDGE}`;
const BRIDGE_RE = /^[ \t]*<script src="\.\.\/bridge\/electronAPI\.js"><\/script>[ \t]*\r?\n?/im;
const CSP_RE = /<meta\s+http-equiv="Content-Security-Policy"[\s\S]*?>/i;
const CSP_CONTENT_RE = /(<meta\s+http-equiv="Content-Security-Policy"\s+content=")([^"]*)(")/i;
const WS = 'ws://127.0.0.1:47831';
const WS_ANY_RE = /ws:\/\/127\.0\.0\.1:\*/g;

function walk(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      if (entry.name === 'bridge') continue;
      out.push(...walk(full));
    } else if (entry.name.endsWith('.html')) {
      out.push(full);
    }
  }
  return out;
}

function placeBridge(html) {
  let next = html.replace(BRIDGE_RE, '');

  // The CSP meta must govern the bridge script, so insert immediately after
  // the policy instead of as the first child of <head>.
  if (CSP_RE.test(next)) {
    next = next.replace(CSP_RE, (meta) => `${meta}\n${BRIDGE_LINE}`);
  } else {
    next = next.replace(/<head(?:\s[^>]*)?>/i, (head) => `${head}\n${BRIDGE_LINE}`);
  }

  return next;
}

function patchCsp(html) {
  return html.replace(CSP_CONTENT_RE, (_match, open, policy, close) => {
    let updated = policy.replace(WS_ANY_RE, WS);

    if (!/connect-src/.test(updated)) {
      updated = updated.replace(/;\s*$/, '') + `; connect-src 'self' ${WS}`;
    } else if (!updated.includes(WS)) {
      updated = updated.replace(/connect-src\s+([^;]*)/, (_m, sources) =>
        `connect-src ${sources.trim()} ${WS}`,
      );
    }

    return `${open}${updated}${close}`;
  });
}

function patch(html) {
  return patchCsp(placeBridge(html));
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
