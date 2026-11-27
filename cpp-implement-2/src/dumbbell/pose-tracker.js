// Pose landmark extraction for the dumbbell screen. When MediaPipe assets are
// installed next to the app we use them; otherwise the tracker gracefully
// reports no landmarks, and the test hook can still drive the rep path.
(function () {
  'use strict';

  var ARMS = {
    left: { shoulder: 11, elbow: 13, wrist: 15 },
    right: { shoulder: 12, elbow: 14, wrist: 16 }
  };
  var MIN_VISIBILITY = 0.6;

  async function ensureRuntime() {
    if (window.FilesetResolver && window.PoseLandmarker) return true;
    try {
      var head = await fetch('mediapipe/tasks-vision.js', { method: 'HEAD' });
      if (!head.ok) return false;
      await new Promise(function (resolve, reject) {
        var script = document.createElement('script');
        script.src = 'mediapipe/tasks-vision.js';
        script.onload = resolve;
        script.onerror = reject;
        document.head.appendChild(script);
      });
    } catch (error) {
      return false;
    }
    return Boolean(window.FilesetResolver && window.PoseLandmarker);
  }

  async function createLandmarker(delegate) {
    if (!await ensureRuntime()) {
      throw new Error('MediaPipe runtime is not installed');
    }
    var resolver = window.FilesetResolver;
    var landmarkerClass = window.PoseLandmarker;
    if (!resolver || !landmarkerClass) {
      throw new Error('MediaPipe runtime is not installed');
    }

    var modelResponse = await fetch('pose_landmarker_lite.task', { method: 'HEAD' });
    if (!modelResponse.ok) {
      throw new Error('MediaPipe model is not installed');
    }

    var fileset = await resolver.forVisionTasks('mediapipe/wasm');
    return landmarkerClass.createFromOptions(fileset, {
      baseOptions: { modelAssetPath: 'pose_landmarker_lite.task', delegate: delegate },
      runningMode: 'VIDEO',
      numPoses: 1,
      minPoseDetectionConfidence: 0.5,
      minTrackingConfidence: 0.5
    });
  }

  async function createPoseTracker() {
    var landmarker = null;
    try {
      landmarker = await createLandmarker('GPU');
    } catch (error) {
      try {
        landmarker = await createLandmarker('CPU');
      } catch (cpuError) {
        landmarker = null;
      }
    }

    function detect(videoEl, timestampMs) {
      var out = { left: null, right: null };
      if (!landmarker) return out;

      var result = landmarker.detectForVideo(videoEl, timestampMs);
      var lms = result && result.landmarks && result.landmarks[0];
      if (!lms) return out;

      var width = videoEl.videoWidth;
      var height = videoEl.videoHeight;

      Object.keys(ARMS).forEach(function (side) {
        var ids = ARMS[side];
        var points = [ids.shoulder, ids.elbow, ids.wrist].map(function (index) {
          return lms[index];
        });
        if (!points.every(function (p) {
          return p && p.visibility != null && p.visibility > MIN_VISIBILITY;
        })) return;

        var pixels = points.map(function (p) {
          return { x: p.x * width, y: p.y * height };
        });
        out[side] = {
          angle: window.DumbbellPose.calculateAngle(pixels[0], pixels[1], pixels[2]),
          points: points
        };
      });

      return out;
    }

    return {
      detect: detect,
      close: function () {
        if (landmarker && landmarker.close) landmarker.close();
        landmarker = null;
      }
    };
  }

  window.DumbbellPose.createPoseTracker = createPoseTracker;
})();
