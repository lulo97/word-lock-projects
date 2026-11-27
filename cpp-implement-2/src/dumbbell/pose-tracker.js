// Pose landmark extraction for the dumbbell screen. The packaged MediaPipe ES
// module is loaded at runtime; test mode swaps in a synthetic landmarker so the
// rep path can run without a camera or a 30 MB model download.
(function () {
  'use strict';

  var MODEL_PATH = 'pose_landmarker_heavy.task';
  var ARMS = {
    left: { shoulder: 11, elbow: 13, wrist: 15 },
    right: { shoulder: 12, elbow: 14, wrist: 16 }
  };
  var MIN_VISIBILITY = 0.6;

  var mockEnabled = window.__WORDLOCK_DUMBBELL_MOCK__ === true ||
      window.location.search.indexOf('mock=1') !== -1;
  window.__dumbbellMock = {
    enabled: mockEnabled,
    modelAssetPath: null,
    delegate: null,
    detectCount: 0,
    lastAngle: null
  };

  function setMockArm(points, ids, angleDeg, xOffset) {
    var radians = angleDeg * Math.PI / 180;
    points[ids.shoulder] = { x: 0.5 + xOffset, y: 0.32, z: 0, visibility: 1 };
    points[ids.elbow] = { x: 0.5 + xOffset, y: 0.52, z: 0, visibility: 1 };
    points[ids.wrist] = {
      x: 0.5 + xOffset + 0.2 * Math.sin(radians),
      y: 0.52 - 0.2 * Math.cos(radians),
      z: 0,
      visibility: 1
    };
  }

  function mockLandmarks(timestampMs) {
    var points = [];
    for (var i = 0; i < 33; i++) {
      points.push({ x: 0.5, y: 0.5, z: 0, visibility: 1 });
    }
    var angle = Math.floor(timestampMs / 450) % 2 === 0 ? 170 : 30;
    setMockArm(points, ARMS.left, angle, -0.08);
    setMockArm(points, ARMS.right, angle, 0.08);
    window.__dumbbellMock.lastAngle = angle;
    return points;
  }

  function createMockLandmarker() {
    return {
      detectForVideo: function (videoEl, timestampMs) {
        window.__dumbbellMock.detectCount += 1;
        return { landmarks: [mockLandmarks(timestampMs)] };
      },
      close: function () {}
    };
  }

  function installMockRuntime() {
    if (!window.FilesetResolver) {
      window.FilesetResolver = {
        forVisionTasks: function () { return Promise.resolve({ mock: true }); }
      };
    }
    if (!window.PoseLandmarker) {
      window.PoseLandmarker = {
        createFromOptions: function (fileset, options) {
          window.__dumbbellMock.modelAssetPath = options.baseOptions.modelAssetPath;
          window.__dumbbellMock.delegate = options.baseOptions.delegate;
          return Promise.resolve(createMockLandmarker());
        }
      };
    }
  }

  async function ensureRuntime() {
    if (mockEnabled) {
      installMockRuntime();
      return true;
    }
    if (window.FilesetResolver && window.PoseLandmarker) return true;
    try {
      var mod = await import('./mediapipe/vision_bundle.mjs');
      window.FilesetResolver = mod.FilesetResolver;
      window.PoseLandmarker = mod.PoseLandmarker;
    } catch (error) {
      console.error(error);
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

    var modelResponse = await fetch(MODEL_PATH, { method: 'HEAD' });
    if (!modelResponse.ok) {
      throw new Error('MediaPipe model is not installed');
    }

    var fileset = await resolver.forVisionTasks('mediapipe/wasm');
    return landmarkerClass.createFromOptions(fileset, {
      baseOptions: { modelAssetPath: MODEL_PATH, delegate: delegate },
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

      var width = videoEl.videoWidth || 640;
      var height = videoEl.videoHeight || 640;

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
