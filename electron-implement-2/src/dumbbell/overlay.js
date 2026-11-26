const LINE_COLOR = '#22c55e';
const DOT_COLOR = '#ffffff';

export function drawArms(canvas, video, result) {
  const cw = video.clientWidth;
  const ch = video.clientHeight;
  if (canvas.width !== cw || canvas.height !== ch) {
    canvas.width = cw;
    canvas.height = ch;
  }

  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, cw, ch);

  const vw = video.videoWidth;
  const vh = video.videoHeight;
  if (!vw || !vh) return;

  // Same math as CSS object-fit: cover.
  const scale = Math.max(cw / vw, ch / vh);
  const offX = (cw - vw * scale) / 2;
  const offY = (ch - vh * scale) / 2;
  const map = (p) => ({ x: p.x * vw * scale + offX, y: p.y * vh * scale + offY });

  for (const side of ['left', 'right']) {
    const arm = result[side];
    if (!arm?.points) continue;
    const [shoulder, elbow, wrist] = arm.points.map(map);

    ctx.lineWidth = 6;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.strokeStyle = LINE_COLOR;
    ctx.beginPath();
    ctx.moveTo(shoulder.x, shoulder.y);
    ctx.lineTo(elbow.x, elbow.y);
    ctx.lineTo(wrist.x, wrist.y);
    ctx.stroke();

    for (const p of [shoulder, elbow, wrist]) {
      ctx.beginPath();
      ctx.arc(p.x, p.y, 8, 0, Math.PI * 2);
      ctx.fillStyle = DOT_COLOR;
      ctx.fill();
      ctx.lineWidth = 3;
      ctx.strokeStyle = LINE_COLOR;
      ctx.stroke();
    }
  }
}
