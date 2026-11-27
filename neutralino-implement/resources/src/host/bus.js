// All cross-window commands are broadcast events with a target id.
export function emit(name, detail) {
  return Neutralino.app.broadcast(name, detail);
}

export function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
