import { FilesetResolver, PoseLandmarker } from '../../vendor/mediapipe/vision_bundle.mjs';
import { calculateAngle } from './rep-counter.js';

// Keep the old import path working for callers that only need the counter.
export { calculateAngle, RepCounter } from './rep-counter.js';

const ARMS = {
  left: { shoulder: 11, elbow: 13, wrist: 15 },
  right: { shoulder: 12, elbow: 14, wrist: 16 },
};
const MIN_VISIBILITY = 0.6;

// Resolved against this module so they work from the app:// page as well.
const WASM_DIR = new URL('../../vendor/mediapipe/wasm', import.meta.url).href;
const MODEL_PATH = new URL('../../assets/pose_landmarker_lite.task', import.meta.url).href;

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
