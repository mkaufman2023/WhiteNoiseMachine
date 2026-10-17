/* Service worker for the White Noise Machine PWA.
 *
 * Caches only the app shell. All live data arrives over the WebSocket, so
 * there is no API response to cache and no staleness to reason about.
 *
 * Why bother: without this, opening the home-screen icon while the box is
 * booting (or off) gives a browser connection error. With it, the UI opens
 * instantly from cache and shows "Not linked" in its own voice, then connects
 * itself as soon as the box answers.
 *
 * Bump CACHE when anything in data/ changes, or phones will keep the old shell.
 */

const CACHE = "wnm-v2";
const SHELL = [
  "/",
  "/index.html",
  "/manifest.json",
  "/icon-180.png",
  "/icon-512.png",
  "/icon.svg"
];

self.addEventListener("install", event => {
  event.waitUntil(
    caches.open(CACHE)
      .then(c => c.addAll(SHELL))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener("activate", event => {
  event.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", event => {
  const req = event.request;

  // Never touch WebSocket upgrades or the state endpoint.
  if (req.method !== "GET" || req.url.includes("/ws") || req.url.includes("/api/")) return;

  // Cache first, then network. The shell is tiny and changes only on release,
  // and this is what makes a cold launch feel instant on a sleepy phone.
  event.respondWith(
    caches.match(req).then(hit => {
      if (hit) {
        // Refresh in the background so the next launch has the newest shell.
        event.waitUntil(
          fetch(req).then(res => caches.open(CACHE).then(c => c.put(req, res))).catch(() => {})
        );
        return hit;
      }
      return fetch(req).catch(() => caches.match("/index.html"));
    })
  );
});
