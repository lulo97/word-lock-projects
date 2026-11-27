// Dumbbell workout: independent 10-rep counters, aspect-corrected overlay,
// camera + pose lifecycle, and a synthetic pose hook for tests.
(function () {
  'use strict';

  var RepCounter = window.DumbbellPose.RepCounter;
  var drawArms = window.DumbbellPose.drawArms;
  var createPoseTracker = window.DumbbellPose.createPoseTracker;

  var TARGET_REPS = 10;
  var SIDES = ['left', 'right'];
  var SEG_ON = 'ui-seg is-on';
  var SEG_OFF = 'ui-seg';

  function $(id) { return document.getElementById(id); }

  var videoEl = $('camera');
  var overlayEl = $('overlay');
  var statusEl = $('status');
  var doneEl = $('done');
  var overallEl = $('overall');
  var arms = { left: new RepCounter(TARGET_REPS), right: new RepCounter(TARGET_REPS) };

  var stream = null;
  var pose = null;
  var finished = false;
  var testPaused = false;

  // Tests can drive the rep path even when camera/model assets are unavailable.
  window.__dumbbellTest = {
    feedPose: function (leftAngle, rightAngle, points, now) {
      if (finished) return;
      testPaused = true;
      var arm = function (angle) {
        return angle == null ? null : { angle: angle, points: points || null };
      };
      processPose({ left: arm(leftAngle), right: arm(rightAngle) },
        now == null ? performance.now() : now);
    }
  };

  function processPose(result, now) {
    arms.left.update(result.left ? result.left.angle : null, now);
    arms.right.update(result.right ? result.right.angle : null, now);

    drawArms(overlayEl, videoEl, result);
    renderReps();

    if (arms.left.done && arms.right.done) complete();
  }

  var lastVideoTime = -1;
  function poseLoop() {
    if (finished) return;

    if (!testPaused && pose && videoEl.readyState >= 2 &&
        videoEl.currentTime !== lastVideoTime) {
      lastVideoTime = videoEl.currentTime;
      var now = performance.now();
      processPose(pose.detect(videoEl, now), now);
    }
    requestAnimationFrame(poseLoop);
  }

  function buildProgress() {
    SIDES.forEach(function (side) {
      var bar = $(side + '-bar');
      bar.replaceChildren();
      for (var i = 0; i < TARGET_REPS; i++) {
        var segment = document.createElement('span');
        segment.className = SEG_OFF;
        bar.appendChild(segment);
      }
    });
  }

  function renderReps() {
    SIDES.forEach(function (side) {
      var arm = arms[side];
      $(side + '-reps').textContent = arm.reps + ' / ' + TARGET_REPS;
      Array.prototype.forEach.call($(side + '-bar').children, function (segment, index) {
        segment.className = index < arm.reps ? SEG_ON : SEG_OFF;
      });

      var status;
      if (arm.done) status = 'DONE ✔';
      else if (!arm.visible) status = 'ARM HIDDEN';
      else status = String(arm.stage || '...').toUpperCase() + ' · ' + Math.round(arm.angle) + '°';
      $(side + '-stage').textContent = status;
      $('arm-' + side).className = arm.done ? 'ui-panel is-done' : 'ui-panel';
    });

    overallEl.textContent = 'Total ' + (arms.left.reps + arms.right.reps) + ' / ' + (TARGET_REPS * 2);
  }

  function complete() {
    finished = true;
    renderReps();
    doneEl.classList.remove('hidden');
    setTimeout(function () { window.app.send('app:exitInfo'); }, 300);
  }

  function stop() {
    finished = true;
    if (pose) pose.close();
    pose = null;
    if (stream) stream.getTracks().forEach(function (track) { track.stop(); });
    stream = null;
    videoEl.srcObject = null;
  }

  async function start() {
    buildProgress();
    renderReps();

    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      statusEl.textContent = 'Camera error: mediaDevices unavailable';
      return;
    }

    try {
      stream = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
      if (finished) {
        stream.getTracks().forEach(function (track) { track.stop(); });
        stream = null;
        return;
      }
      videoEl.srcObject = stream;
      await videoEl.play();
    } catch (error) {
      statusEl.textContent = 'Camera error: ' + error.message;
      return;
    }

    if (finished) return;

    try {
      statusEl.textContent = 'Loading pose model...';
      pose = await createPoseTracker();
      if (finished) {
        pose.close();
        pose = null;
        return;
      }
    } catch (error) {
      statusEl.textContent = 'Pose model error: ' + error.message;
      return;
    }

    statusEl.textContent = 'Pose tracker ready';
    requestAnimationFrame(poseLoop);
  }

  if (window.app && window.app.on) window.app.on('app:hide', stop);
  window.addEventListener('pagehide', stop);

  start();
})();
