function centerPhysicalFromScaled(scaledWidth, scaledHeight, scale, appWidth, appHeight) {
  const physicalWidth = scaledWidth * scale;
  const physicalHeight = scaledHeight * scale;
  const x = Math.round((physicalWidth - appWidth) / 2);
  const y = Math.round((physicalHeight - appHeight) / 2);
  return { x, y };
}

// Example: scaled 1536x864 at 1.25x zoom, app 1280x720 -> { x: 320, y: 180 }
// const scaledResult = centerPhysicalFromScaled(1536, 864, 1.25, 1280, 720);
// console.log(`(${scaledResult.x}, ${scaledResult.y})`);