import { FilesetResolver, PoseLandmarker } from '../../vendor/mediapipe/vision_bundle.mjs';

const ARMS = {
  left: { shoulder: 11, elbow: 13, wrist: 15 },
  right: { shoulder: 12, elbow: 14, wrist: 16 },
};

const UP_ANGLE = 45; // elbow bent -> top of the curl
const DOWN_ANGLE = 155; // arm straight -> bottom of the curl
const MIN_VISIBILITY = 0.6;

// Anti-glitch guards. A single frame can jump 100°+ when landmarks flicker.
const HISTORY = 5; // median window (frames)
const CONFIRM_FRAMES = 3; // frames a threshold must hold before the stage changes
const MIN_UP_MS = 300; // minimum time spent at the top of the curl
const MIN_REP_MS = 800; // minimum time between two counted reps
const LOST_FRAMES = 10; // frames without the arm before the state resets

// Resolved against this module so they work from the app:// page as well.
const WASM_DIR = new URL('../../vendor/mediapipe/wasm', import.meta.url).href;
const MODEL_PATH = new URL('../../assets/pose_landmarker_heavy.task', import.meta.url).href;

export function calculateAngle(a, b, c) {
  const ba = [a.x - b.x, a.y - b.y];
  const bc = [c.x - b.x, c.y - b.y];
  const denom = Math.hypot(...ba) * Math.hypot(...bc);
  if (!denom) return 0;

  const cos = Math.max(-1, Math.min(1, (ba[0] * bc[0] + ba[1] * bc[1]) / denom));
  return (Math.acos(cos) * 180) / Math.PI;
}

function median(values) {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.floor(sorted.length / 2)];
}

export class RepCounter {
  constructor(target = 10) {
    this.target = target;
    this.reps = 0;
    this.stage = null; // null | 'down' | 'up'
    this.angle = null; // smoothed angle
    this.visible = false;

    this.history = [];
    this.pending = null; // stage we are waiting to confirm
    this.pendingCount = 0;
    this.lostFrames = 0;
    this.upSince = 0;
    this.lastRepAt = 0;
  }

  update(rawAngle, now = performance.now()) {
    if (rawAngle === null) {
      this.visible = false;
      this.angle = null;
      if (++this.lostFrames >= LOST_FRAMES) {
        this.stage = null;
        this.history = [];
        this.pending = null;
        this.pendingCount = 0;
      }
      return;
    }

    this.lostFrames = 0;
    this.visible = true;

    this.history.push(rawAngle);
    if (this.history.length > HISTORY) this.history.shift();
    const angle = median(this.history);
    this.angle = angle;

    // Which stage does this frame suggest?
    let candidate = null;
    if (angle > DOWN_ANGLE) candidate = 'down';
    else if (angle < UP_ANGLE) candidate = 'up';

    if (candidate === null || candidate === this.stage) {
      this.pending = null;
      this.pendingCount = 0;
      return;
    }

    // Debounce: the candidate must repeat for CONFIRM_FRAMES frames.
    if (candidate === this.pending) this.pendingCount += 1;
    else {
      this.pending = candidate;
      this.pendingCount = 1;
    }
    if (this.pendingCount < CONFIRM_FRAMES) return;

    this.pending = null;
    this.pendingCount = 0;

    if (candidate === 'up' && this.stage === 'down') {
      this.stage = 'up';
      this.upSince = now;
    } else if (candidate === 'down') {
      const heldUp = now - this.upSince >= MIN_UP_MS;
      const spaced = now - this.lastRepAt >= MIN_REP_MS;
      if (this.stage === 'up' && heldUp && spaced && this.reps < this.target) {
        this.reps += 1;
        this.lastRepAt = now;
      }
      this.stage = 'down';
    }
  }

  get done() {
    return this.reps >= this.target;
  }
}

async function createLandmarker(delegate) {
  const fileset = await FilesetResolver.forVisionTasks(WASM_DIR);
  return PoseLandmarker.createFromOptions(fileset, {
    baseOptions: { modelAssetPath: MODEL_PATH, delegate },
    runningMode: 'VIDEO',
    numPoses: 1,
    minPoseDetectionConfidence: 0.5,
    minTrackingConfidence: 0.5,
  });
}

export async function createPoseTracker() {
  let landmarker;
  try {
    landmarker = await createLandmarker('GPU');
  } catch {
    // Some machines have no usable WebGL context; CPU is slower but works.
    landmarker = await createLandmarker('CPU');
  }

  // Returns { left, right } where each arm is null or { angle, points }.
  function detect(videoEl, timestampMs) {
    const res = landmarker.detectForVideo(videoEl, timestampMs);
    const lms = res.landmarks?.[0];
    const out = { left: null, right: null };
    if (!lms) return out;

    const w = videoEl.videoWidth;
    const h = videoEl.videoHeight;

    for (const [side, ids] of Object.entries(ARMS)) {
      const pts = [ids.shoulder, ids.elbow, ids.wrist].map((i) => lms[i]);
      if (!pts.every((p) => p && (p.visibility ?? 0) > MIN_VISIBILITY)) continue;

      // Scale to pixels so the angle is not distorted by the video aspect ratio.
      const px = pts.map((p) => ({ x: p.x * w, y: p.y * h }));
      out[side] = {
        angle: calculateAngle(px[0], px[1], px[2]),
        points: pts, // normalized 0..1: shoulder, elbow, wrist
      };
    }

    return out;
  }

  return { detect, close: () => landmarker.close() };
}
