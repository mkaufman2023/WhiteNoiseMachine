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
 * Updates take care of themselves: the box appends a fingerprint of all its
 * built-in web files to this script when it serves it ("// build ..." at the
 * end). When a firmware update changes the UI, the fingerprint changes, the
 * browser sees a new service worker, installs it (re-fetching the shell), and
 * the page reloads once onto the new version. Nothing to bump by hand.
 */

const CACHE = "wnm-shell";
const SHELL = [
  "/",
  "/index.html",
  "/manifest.json",
  "/icon-180.png",
  "/icon-512.png",
  "/icon.svg"
];

self.addEventListener("install", event => {
  // "reload" skips the browser's HTTP cache, so a new version never picks up
  // last week's icons or page.
  event.waitUntil(
    caches.open(CACHE)
      .then(c => c.addAll(SHELL.map(url => new Request(url, { cache: "reload" }))))
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
