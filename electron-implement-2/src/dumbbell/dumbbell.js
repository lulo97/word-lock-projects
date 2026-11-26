const STAGES = ['hold dumbbell', 'up dumbbell', 'down dumbbell'];
const SET_COUNT = 5;
const TOTAL_STEPS = SET_COUNT * STAGES.length;
const REQUIRED_MATCHES = 3;

const videoEl = document.getElementById('camera');
const canvasEl = document.getElementById('frame');
const setsEl = document.getElementById('sets');
const overallEl = document.getElementById('overall');
const seenEl = document.getElementById('seen');
const requiredEl = document.getElementById('required');
const streakEl = document.getElementById('streak');
const doneEl = document.getElementById('done');
const connectionEl = document.getElementById('connection');
const requestsEl = document.getElementById('requests');
const avgEl = document.getElementById('avg');
const recentEl = document.getElementById('recent');

let stream = null;
let timer = null;
let statusTimer = null;
let frameIntervalMs = 1000;
let stepIndex = 0;
let streak = 0;
let finished = false;
let connected = false;
let requestCount = 0;
let totalDurationMs = 0;
let recent = [];

function renderProgress() {
  overallEl.textContent = `Step ${Math.min(stepIndex + 1, TOTAL_STEPS)} / ${TOTAL_STEPS}`;
  setsEl.replaceChildren();

  for (let set = 0; set < SET_COUNT; set += 1) {
    const setEl = document.createElement('div');
    setEl.className = 'flex flex-col gap-1';

    const title = document.createElement('p');
    title.className = 'whitespace-nowrap text-sm font-bold';
    title.textContent = `Set ${set + 1} / ${SET_COUNT}`;
    setEl.appendChild(title);

    const row = document.createElement('div');
    row.className = 'flex w-full flex-nowrap gap-1';

    for (let rep = 0; rep < STAGES.length; rep += 1) {
      const index = set * STAGES.length + rep;
      const chip = document.createElement('span');

      if (index < stepIndex) {
        chip.className = 'min-w-0 flex-1 truncate rounded bg-green-600 px-1 py-0.5 text-xs font-semibold text-white';
      } else if (index === stepIndex && !finished) {
        chip.className = 'min-w-0 flex-1 truncate rounded bg-yellow-400 px-1 py-0.5 text-xs font-bold';
      } else {
        chip.className = 'min-w-0 flex-1 truncate rounded bg-gray-200 px-1 py-0.5 text-xs';
      }

      chip.textContent = `${rep + 1}. ${STAGES[rep].replace(' dumbbell', '')}`;
      row.appendChild(chip);
    }

    setEl.appendChild(row);
    setsEl.appendChild(setEl);
  }

  requiredEl.textContent = finished
    ? ''
    : `Required: ${STAGES[stepIndex % STAGES.length]} (${streak} / ${REQUIRED_MATCHES} continuous matches)`;
  streakEl.textContent = `Matches: ${streak} / ${REQUIRED_MATCHES}`;
}

function updateStats() {
  connectionEl.textContent = `llama.cpp 8080 status: ${connected ? 'connected' : 'disconnected'}`;
  requestsEl.textContent = `Requests sent: ${requestCount}`;
  avgEl.textContent = requestCount
    ? `Avg request: ${(totalDurationMs / requestCount / 1000).toFixed(2)}s`
    : 'Avg request: —';

  recentEl.replaceChildren();
  for (const entry of recent) {
    const item = document.createElement('li');
    item.className = 'truncate';
    item.textContent = entry;
    item.title = entry;
    recentEl.appendChild(item);
  }
}

async function refreshConnection() {
  try {
    connected = await window.electronAPI.getVisionStatus();
  } catch {
    connected = false;
  }
  updateStats();
}

function recordRecent(entry) {
  recent.unshift(entry);
  recent = recent.slice(0, 5);
}

function captureFrame() {
  if (!videoEl.videoWidth || !videoEl.videoHeight) return null;

  canvasEl.width = 320;
  canvasEl.height = 240;
  const context = canvasEl.getContext('2d');
  context.drawImage(videoEl, 0, 0, canvasEl.width, canvasEl.height);
  return canvasEl.toDataURL('image/jpeg', 0.7);
}

function scheduleNextFrame() {
  if (!finished) {
    timer = setTimeout(analyzeFrame, frameIntervalMs);
  }
}

async function analyzeFrame() {
  if (finished) return;

  const dataUrl = captureFrame();
  if (!dataUrl) {
    scheduleNextFrame();
    return;
  }

  requestCount += 1;
  const startedAt = Date.now();

  try {
    const result = await window.electronAPI.analyzeFrame(dataUrl);
    totalDurationMs += Date.now() - startedAt;

    const seen = result.raw || '(no text)';
    seenEl.textContent = `Last seen: ${seen}`;
    recordRecent(seen);

    if (result.label === STAGES[stepIndex % STAGES.length]) {
      streak += 1;
    } else {
      streak = 0;
    }
  } catch (error) {
    totalDurationMs += Date.now() - startedAt;
    seenEl.textContent = `Last seen: error - ${error.message}`;
    recordRecent(`error: ${error.message}`);
    streak = 0;
  }

  updateStats();

  if (streak >= REQUIRED_MATCHES) {
    stepIndex += 1;
    streak = 0;

    if (stepIndex >= TOTAL_STEPS) {
      complete();
      return;
    }
  }

  renderProgress();
  scheduleNextFrame();
}

function complete() {
  finished = true;
  clearTimeout(timer);
  renderProgress();
  doneEl.classList.remove('hidden');
  setTimeout(() => window.electronAPI.exitInfo(), 300);
}

function stop() {
  finished = true;
  clearTimeout(timer);
  clearInterval(statusTimer);

  if (stream) {
    for (const track of stream.getTracks()) track.stop();
    stream = null;
  }

  videoEl.srcObject = null;
}

async function start() {
  const config = await window.electronAPI.getVisionConfig();
  frameIntervalMs = config.frameIntervalMs;

  await refreshConnection();
  statusTimer = setInterval(refreshConnection, 2000);

  try {
    stream = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
    videoEl.srcObject = stream;
    await videoEl.play();
  } catch (error) {
    seenEl.textContent = `Camera error: ${error.message}`;
  }

  renderProgress();
  updateStats();
  scheduleNextFrame();
}

window.electronAPI.onHide(stop);
window.addEventListener('pagehide', stop);

start();
