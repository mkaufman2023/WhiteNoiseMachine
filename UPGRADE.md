# Upgrade notes

Drop-in replacement for the original firmware and UI. Same hardware, same
wiring, same SD card layout — no soldering.

---

## 0. Do this first: rotate the Wi-Fi passwords

`lib/HomeWiFi/wificonfig.h` was committed to a public repo with live SSIDs and
passwords for both networks, and `src/main.cpp` had a third hardcoded copy.
Because it's in public git history, deleting the file does not undo the
disclosure — anyone who cloned or forked the repo still has it, as do GitHub's
own caches.

1. **Change the password on both networks.** This is the only step that
   actually fixes it. Everything else is hygiene.
2. Delete the dead directories: `lib/HomeWiFi/` and `lib/TD5580A/`. Nothing
   includes either one; `TD5580A` was being compiled into the binary for
   nothing (it's the library the DFPlayerMini driver was adapted *from*).
3. Put the new credentials only in `include/config/wifi.h`, which is
   gitignored. The updated `.gitignore` now also covers `**/wificonfig.h`.
4. Optional, if you want the history clean:
   `git filter-repo --path lib/HomeWiFi/wificonfig.h --invert-paths`
   then force-push. Treat this as cosmetic — the passwords are already burned.

---

## 1. Files

| Path | Change |
|---|---|
| `platformio.ini` | pioarduino platform, LittleFS, async + JSON deps, dropped unused DFRobot dep |
| `src/main.cpp` | Rewritten; no blocking, no inline credentials |
| `lib/Player/` | **New.** Playback engine: state, command queue, fades, NVS |
| `lib/Net/` | **New.** Non-blocking Wi-Fi with reconnect + mDNS re-advertisement |
| `lib/WebApp/` | Rewritten for `ESPAsyncWebServer` + WebSocket |
| `lib/DFPlayerMini/` | Three surgical edits (see below); the other 850 lines untouched |
| `data/` | New UI, service worker, manifest, PNG icons |
| `test/host/` | **New.** Host-side regression test, no hardware needed |

Three edits to your driver, all marked with `NOTE:` comments:

- `_send_command()` — removed `delay(20)` and `delay(500)`
- `begin()` — removed `delay(2500)`

Those three delays were the whole latency problem. Every command cost ~520 ms
inside the HTTP handler; the module only needs a short gap between UART frames,
which `Player::tick()` now enforces at 35 ms without ever blocking.

---

## 2. Flash it

```bash
pio run -t upload      # firmware
pio run -t uploadfs    # the data/ folder -- easy to forget, and the UI is blank without it
```

`uploadfs` is now **LittleFS**, not SPIFFS. The first boot after switching
reformats the partition, which is fine — nothing of yours lived there.

On her phone: delete the old home-screen icon and re-add it. The old one has a
stale service-worker-less shell and the wrong icon.

---

## 3. What changed operationally

**Boot is silent.** The old `setup_DFPlayer()` set volume to 30 and immediately
looped track 1, so any power blip meant full-volume airplane cabin at 2 a.m.
Now the box restores the last track and volume from NVS, comes up at 0, and
fades in. It also caps the module's own power-on volume at 8 as a backstop.

**Wi-Fi recovers by itself.** The old code connected once in `setup()` and never
again — a router reboot bricked it until you unplugged it. Now it rotates
through both networks, retries with backoff forever, and re-advertises mDNS on
every reconnect (mDNS has to be torn down and restarted, or the record goes
stale and `whitenoise.local` silently stops resolving).

**Set a DHCP reservation on your router** for the XIAO's MAC. mDNS on Android is
unreliable and some routers break it entirely; a fixed IP means there's always
a second way in. The address is shown in the diagnostics panel.

---

## 4. Using it

- **Tap a sound** to play it; it fades out, switches, fades back in.
- **Hold a sound for half a second** to pin it. Pinned sounds appear in a
  larger grid at the top. Held again to unpin. Pins are stored on the box, so
  both phones see the same set.
- **Night mode** (bottom left) drops the whole UI to dim amber and removes all
  blue. It's stored per phone, not on the box.
- **Hold the title** ("Acoustic Wellness Unit") for a second to open the
  diagnostics panel — IP, signal, mDNS status, uptime, and the live log that
  used to be bolted to the main screen. Logs only stream while it's open.

The volume readout is the actual module level and the slider is the setpoint.
When they differ you'll see `14 → 20`, which is the fade walking. That's
intentional — it's the one place the machine shows you its own internals.

---

## 5. Tuning

In `lib/Player/Player.h`:

| Constant | Default | Effect |
|---|---|---|
| `MIN_TX_GAP_MS` | 35 | UART frame spacing. Lower = snappier. If commands start getting dropped, raise it. |
| `FADE_STEP_MS` | 50 | One volume unit per this. 50 × 20 units ≈ 1 s fade. Raise for slower, gentler transitions. |
| `WARMUP_MS` | 2500 | Module settling time after power-on. |
| `SAVE_DEBOUNCE_MS` | 5000 | How long after the last change before writing NVS. |

`MIN_TX_GAP_MS` is the one worth experimenting with. 35 ms is conservative;
the frame itself is only 8.3 ms at 9600 baud. If your clone tolerates 20 ms,
sliders get noticeably tighter.

Run the tests after changing any of these:

```bash
cd test/host && ./run.sh
```

It mocks the DFPlayer and simulates `millis()`, so it verifies fade ordering,
command coalescing, frame spacing, and NVS round-tripping in about a second —
without reflashing.

---

## 6. Worth doing next

- **Wire the DFPlayer `BUSY` pin** to any free GPIO (`D0`–`D5`, `D8`–`D10` are
  all unused). It's currently unconnected, which is why playback state has to
  be inferred. One wire gets you true "is it actually playing" feedback instead
  of a shadow copy — useful because the clone's query commands are unreliable
  (your own note on `Commands.h:63`).
- **Rotary encoder** on two more free GPIOs. With state broadcasting already in
  place, turning the knob would move the slider on her phone live, and vice
  versa.
- **MQTT publish** of the same state struct would drop it straight into Home
  Assistant — voice control and "fade off at 7 a.m." automations for maybe 40
  lines. The state model is already the right shape for it.
