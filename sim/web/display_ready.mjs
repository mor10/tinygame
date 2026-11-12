export function waitForDisplayFrames(host = globalThis) {
  return new Promise((resolve, reject) => {
    let remaining = 4;
    let frame;
    let timer;
    const cleanup = () => {
      host.cancelAnimationFrame(frame);
      host.clearTimeout(timer);
      host.document.removeEventListener("visibilitychange", visibility);
    };
    const visibility = () => {
      host.clearTimeout(timer);
      if (!host.document.hidden) {
        timer = host.setTimeout(() => {
          cleanup();
          reject(new Error("Simulator display did not become ready."));
        }, 3000);
      }
    };
    const next = () => {
      if (--remaining === 0) { cleanup(); resolve(); }
      else frame = host.requestAnimationFrame(next);
    };
    host.document.addEventListener("visibilitychange", visibility);
    visibility();
    frame = host.requestAnimationFrame(next);
  });
}
