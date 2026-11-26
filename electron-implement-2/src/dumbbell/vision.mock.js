const { LABELS } = require('./vision');

const queue = [];

function setOutputs(outputs) {
  queue.length = 0;
  queue.push(...outputs);
}

function getRemaining() {
  return [...queue];
}

async function analyzeFrame(_dataUrl) {
  const raw = queue.length ? queue.shift() : LABELS.none;
  const text = String(raw).toLowerCase().trim();

  let label = '';
  if (text.includes('no hold') || text.includes('not hold')) label = LABELS.none;
  else if (text.includes('down')) label = LABELS.down;
  else if (text.includes('up')) label = LABELS.up;
  else if (text.includes('hold')) label = LABELS.hold;

  return { raw, label };
}

async function getStatus() {
  return true;
}

module.exports = { analyzeFrame, getStatus, setOutputs, getRemaining, LABELS };
