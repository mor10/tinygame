// Kept separate so ordinary browser use and the Copilot Canvas share cart loading.
globalThis.TinyGamePreview = {
  cartUrl(search, base) {
    const cart = new URLSearchParams(search).get("cart");
    if (!cart) return null;
    const url = new URL(cart, base);
    if (url.origin !== new URL(base).origin || !/^\/games\/[a-z0-9_-]{1,31}\.lua$/.test(url.pathname)
        || url.search || url.hash) {
      throw new Error("Preview cart must be a local /games/<name>.lua file.");
    }
    return url.href;
  },

  watch(url, { read, load, report, schedule = setTimeout }) {
    let active = true, previous, failed = false;
    async function poll() {
      if (!active) return;
      try {
        const text = await read(url);
        if (!active) return;
        if (text !== previous) {
          load(text, url.split("/").pop());
          previous = text;
        }
        failed = false;
      } catch (error) {
        if (active && !failed) report(`Live preview: ${error.message}`);
        failed = true;
      }
      if (active) schedule(poll, 1000);
    }
    void poll();
    return () => { active = false; };
  },
};
