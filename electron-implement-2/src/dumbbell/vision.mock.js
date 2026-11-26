const { LABELS, parseVisionResponse } = require('./vision');

const queue = [];
const stages = [];

function setOutputs(outputs) {
  queue.length = 0;
  queue.push(...outputs);
  stages.length = 0;
}

function getRemaining() {
  return [...queue];
}

function getStages() {
  return [...stages];
}

async function analyzeFrame(_dataUrl, stage = '') {
  stages.push(stage);

  const raw = queue.length ? queue.shift() : LABELS.none;
  const { message, label, status } = parseVisionResponse(raw);

  return { raw: message, label, status };
}

async function getStatus() {
  return true;
}

module.exports = { analyzeFrame, getStatus, setOutputs, getRemaining, getStages, LABELS };
