import { createPoseTracker, RepCounter } from './pose-tracker.js';
import { drawArms } from './overlay.js';

const TARGET_REPS = 10;
const SIDES = ['left', 'right'];
const SEG_ON = 'ui-seg is-on';
const SEG_OFF = 'ui-seg';

const $ = (id) => document.getElementById(id);
const videoEl = $('camera');
const overlayEl = $('overlay');
const statusEl = $('status');
const doneEl = $('done');
const overallEl = $('overall');

const arms = { left: new RepCounter(TARGET_REPS), right: new RepCounter(TARGET_REPS) };

let stream = null;
let pose = null;
let finished = false;
let testPaused = false;

function processPose(result, now = performance.now()) {
  arms.left.update(result.left?.angle ?? null, now);
  arms.right.update(result.right?.angle ?? null, now);

  drawArms(overlayEl, videoEl, result);
  renderReps();

  if (arms.left.done && arms.right.done) complete();
}

let lastVideoTime = -1;
function poseLoop() {
  if (finished) return;

  if (!testPaused && pose && videoEl.readyState >= 2 && videoEl.currentTime !== lastVideoTime) {
    lastVideoTime = videoEl.currentTime;
    const now = performance.now();
    processPose(pose.detect(videoEl, now), now);
  }

  requestAnimationFrame(poseLoop);
}

// ---------- UI ----------
function buildProgress() {
  for (const side of SIDES) {
    $(`${side}-bar`).replaceChildren(
      ...Array.from({ length: TARGET_REPS }, () => {
        const segment = document.createElement('span');
        segment.className = SEG_OFF;
        return segment;
      }),
    );
  }
}

function renderReps() {
  for (const side of SIDES) {
    const arm = arms[side];
    $(`${side}-reps`).textContent = `${arm.reps} / ${TARGET_REPS}`;

    [...$(`${side}-bar`).children].forEach((segment, index) => {
      segment.className = index < arm.reps ? SEG_ON : SEG_OFF;
    });

    let status;
    if (arm.done) status = 'DONE ✔';
    else if (!arm.visible) status = 'ARM HIDDEN';
    else status = `${(arm.stage ?? '...').toUpperCase()} · ${Math.round(arm.angle)}°`;
    $(`${side}-stage`).textContent = status;

    $(`arm-${side}`).className = arm.done ? 'ui-panel is-done' : 'ui-panel';
  }

  overallEl.textContent = `Total ${arms.left.reps + arms.right.reps} / ${TARGET_REPS * 2}`;
}

// ---------- lifecycle ----------
function complete() {
  finished = true;
  renderReps();
  doneEl.classList.remove('hidden');
  setTimeout(() => window.electronAPI.exitInfo(), 300);
}

function stop() {
  finished = true;
  pose?.close();
  pose = null;
  stream?.getTracks().forEach((track) => track.stop());
  stream = null;
  videoEl.srcObject = null;
}

async function start() {
  buildProgress();
  renderReps();

  try {
    stream = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
    videoEl.srcObject = stream;
    await videoEl.play();
  } catch (error) {
    statusEl.textContent = `Camera error: ${error.message}`;
    return;
  }

  try {
    pose = await createPoseTracker();
  } catch (error) {
    statusEl.textContent = `Pose model error: ${error.message}`;
    return;
  }

  statusEl.textContent = 'Pose tracker ready';

  // Test hook: drives the same rep path with synthetic angles, so tests do not
  // need a real person in front of the camera.
  window.__dumbbellTest = {
    feedPose(leftAngle, rightAngle, points = null, now = performance.now()) {
      if (finished) return;
      testPaused = true;
      const arm = (angle) => (angle === null ? null : { angle, points });
      processPose({ left: arm(leftAngle), right: arm(rightAngle) }, now);
    },
  };

  requestAnimationFrame(poseLoop);
}

window.electronAPI.onHide(stop);
window.addEventListener('pagehide', stop);

start();
