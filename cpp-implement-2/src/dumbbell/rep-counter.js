// Elbow-angle rep counting: 5-frame median, 3-frame stage debounce, and the
// timing/lost-arm guards used by the original workout screen.
(function () {
  'use strict';

  var UP_ANGLE = 45;
  var DOWN_ANGLE = 155;
  var HISTORY = 5;
  var CONFIRM_FRAMES = 3;
  var MIN_UP_MS = 300;
  var MIN_REP_MS = 800;
  var LOST_FRAMES = 10;

  function calculateAngle(a, b, c) {
    var ba = [a.x - b.x, a.y - b.y];
    var bc = [c.x - b.x, c.y - b.y];
    var denom = Math.hypot(ba[0], ba[1]) * Math.hypot(bc[0], bc[1]);
    if (!denom) return 0;
    var cos = Math.max(-1, Math.min(1, (ba[0] * bc[0] + ba[1] * bc[1]) / denom));
    return (Math.acos(cos) * 180) / Math.PI;
  }

  function median(values) {
    var sorted = values.slice().sort(function (a, b) { return a - b; });
    return sorted[Math.floor(sorted.length / 2)];
  }

  function RepCounter(target) {
    this.target = target == null ? 10 : target;
    this.reps = 0;
    this.stage = null;
    this.angle = null;
    this.visible = false;
    this.history = [];
    this.pending = null;
    this.pendingCount = 0;
    this.lostFrames = 0;
    this.upSince = 0;
    this.lastRepAt = 0;
  }

  RepCounter.prototype.update = function (rawAngle, now) {
    if (rawAngle == null) {
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
    var angle = median(this.history);
    this.angle = angle;

    var candidate = null;
    if (angle > DOWN_ANGLE) candidate = 'down';
    else if (angle < UP_ANGLE) candidate = 'up';

    if (candidate === null || candidate === this.stage) {
      this.pending = null;
      this.pendingCount = 0;
      return;
    }

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
      var heldUp = now - this.upSince >= MIN_UP_MS;
      var spaced = now - this.lastRepAt >= MIN_REP_MS;
      if (this.stage === 'up' && heldUp && spaced && this.reps < this.target) {
        this.reps += 1;
        this.lastRepAt = now;
      }
      this.stage = 'down';
    }
  };

  Object.defineProperty(RepCounter.prototype, 'done', {
    get: function () { return this.reps >= this.target; }
  });

  window.DumbbellPose = {
    calculateAngle: calculateAngle,
    RepCounter: RepCounter
  };
})();
