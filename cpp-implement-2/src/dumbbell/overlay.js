// Draws shoulder/elbow/wrist lines and joints over the (mirrored) camera video.
(function () {
  'use strict';

  var LINE_COLOR_FALLBACK = '#22c55e';
  var DOT_COLOR = '#ffffff';

  function lineColor() {
    var value = getComputedStyle(document.documentElement)
      .getPropertyValue('--c-accent').trim();
    return value || LINE_COLOR_FALLBACK;
  }

  function drawArms(canvas, video, result) {
    var cw = video.clientWidth;
    var ch = video.clientHeight;
    if (canvas.width !== cw || canvas.height !== ch) {
      canvas.width = cw;
      canvas.height = ch;
    }

    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, cw, ch);

    var vw = video.videoWidth;
    var vh = video.videoHeight;
    if (!vw || !vh) return;

    var scale = Math.max(cw / vw, ch / vh);
    var offX = (cw - vw * scale) / 2;
    var offY = (ch - vh * scale) / 2;
    var map = function (p) {
      return { x: p.x * vw * scale + offX, y: p.y * vh * scale + offY };
    };

    ['left', 'right'].forEach(function (side) {
      var arm = result && result[side];
      if (!arm || !arm.points) return;
      var points = arm.points.map(map);
      if (points.length < 3) return;

      ctx.lineWidth = 6;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      ctx.strokeStyle = lineColor();
      ctx.beginPath();
      ctx.moveTo(points[0].x, points[0].y);
      ctx.lineTo(points[1].x, points[1].y);
      ctx.lineTo(points[2].x, points[2].y);
      ctx.stroke();

      points.forEach(function (p) {
        ctx.beginPath();
        ctx.arc(p.x, p.y, 8, 0, Math.PI * 2);
        ctx.fillStyle = DOT_COLOR;
        ctx.fill();
        ctx.lineWidth = 3;
        ctx.strokeStyle = lineColor();
        ctx.stroke();
      });
    });
  }

  window.DumbbellPose.drawArms = drawArms;
})();
