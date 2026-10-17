/*************************************************************************
*                                                                        *
*   WebApp.cpp                                                           *
*   HTTPS + secure WebSocket interface for the White Noise Machine.      *
*                                                                        *
*   Written by Matt Kaufman - December 5, 2025                           *
*   Rewritten for async + WebSocket - July 2026                          *
*   Moved to HTTPS - October 2026                                        *
*                                                                        *
**************************************************************************/

#include "WebApp.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <esp_https_server.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <ArduinoJson.h>
#include <Player.h>
#include <Net.h>
#include <Logbook.h>
#include <Ota.h>


/* -----------------------------------------------------------------------
 *  Files built into the firmware by board_build.embed_files and
 *  embed_txtfiles in platformio.ini. Each gets start/end symbols named
 *  after its path, e.g. web/index.html -> _binary_web_index_html_start.
 * ----------------------------------------------------------------------- */

#define EMBEDDED(name, path_symbol)                                             \
    extern const uint8_t name##_start[] asm("_binary_" path_symbol "_start");   \
    extern const uint8_t name##_end[]   asm("_binary_" path_symbol "_end");

EMBEDDED(index_html,    "web_index_html")
EMBEDDED(sw_js,         "web_sw_js")
EMBEDDED(manifest_json, "web_manifest_json")
EMBEDDED(icon_svg,      "web_icon_svg")
EMBEDDED(icon_180_png,  "web_icon_180_png")
EMBEDDED(icon_512_png,  "web_icon_512_png")
EMBEDDED(server_crt,    "certs_server_crt")   // made by tools/make_certs.py
EMBEDDED(server_key,    "certs_server_key")


namespace
{
    struct Asset {
        const char*    path;
        const char*    type;
        const char*    cache;
        const uint8_t* start;
        const uint8_t* end;
    };

    // index.html and sw.js are never cached by the browser itself -- the
    // service worker decides what to keep. Everything else only changes with
    // the firmware.
    const Asset ASSETS[] = {
        { "/",                     "text/html; charset=utf-8",  "no-cache",       index_html_start,    index_html_end    },
        { "/index.html",           "text/html; charset=utf-8",  "no-cache",       index_html_start,    index_html_end    },
        { "/sw.js",                "application/javascript",    "no-cache",       sw_js_start,         sw_js_end         },
        { "/manifest.json",        "application/manifest+json", "max-age=86400",  manifest_json_start, manifest_json_end },
        { "/icon.svg",             "image/svg+xml",             "max-age=604800", icon_svg_start,      icon_svg_end      },
        { "/icon-180.png",         "image/png",                 "max-age=604800", icon_180_png_start,  icon_180_png_end  },
        { "/icon-512.png",         "image/png",                 "max-age=604800", icon_512_png_start,  icon_512_png_end  },
        { "/apple-touch-icon.png", "image/png",                 "max-age=604800", icon_180_png_start,  icon_180_png_end  },  // iOS looks here by convention
    };

    /** A message on its way to the server task, which does the sending. */
    struct Outgoing {
        httpd_handle_t server;
        std::string    text;
        bool           log_only;   /**< Only to phones with the diagnostics panel open */
    };

    // Marks a phone that has the diagnostics panel open. It's stored as that
    // WebSocket's session context, so ESP-IDF forgets it by itself when the
    // phone disconnects. Only its address matters.
    char LOG_SUBSCRIBER;
    void keep_subscriber(void*) { }   // the session context's "free" function: nothing to free

    /** {"t": type, "m": lines} -- "l" appends to the panel, "L" replaces it. */
    std::string log_message(const char* type, const std::string& lines)
    {
        JsonDocument doc;
        doc["t"] = type;
        doc["m"] = lines;

        std::string out;
        serializeJson(doc, out);
        return out;
    }

    /** FNV-1a hash over every built-in web file: it changes whenever any of them does. */
    uint32_t web_fingerprint()
    {
        uint32_t h = 2166136261u;
        for (const Asset& a : ASSETS) {
            for (const uint8_t* p = a.start; p < a.end; p++) {
                h = (h ^ *p) * 16777619u;
            }
        }
        return h;
    }

    /** Ends a firmware upload with a plain-text reason, which the app and curl show as is. */
    esp_err_t refuse_update(httpd_req_t* req, const char* status, const char* why)
    {
        logbook::add("OTA: refused: %s", why);
        httpd_resp_set_status(req, status);
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, why);
    }
}


WebApp::WebApp(Player& player, Net& net, const char* update_key)
    : _player(player), _net(net), _update_key(update_key) { }


void WebApp::begin()
{
    _actions = xQueueCreate(ACTION_QUEUE_LEN, sizeof(Action));

    // sw.js goes out with the UI's fingerprint appended. When an update
    // changes any web file, the phones see a "new" service worker and pick
    // up the new UI by themselves (see web/sw.js).
    snprintf(_ui_build, sizeof(_ui_build), "%08lx", (unsigned long)web_fingerprint());
    _sw_js.assign(reinterpret_cast<const char*>(sw_js_start), sw_js_end - sw_js_start);
    _sw_js += "\n// build ";
    _sw_js += _ui_build;
    _sw_js += "\n";
    logbook::add("Web: UI build %s", _ui_build);

    build_state_json(_state_json);   // so /api/state answers before the first broadcast

    if (start_https()) {
        logbook::add("Web: HTTPS server listening on port 443");
    }
    if (start_redirect()) {
        logbook::add("Web: port 80 redirects to HTTPS");
    }
}


void WebApp::tick()
{
    if (_broadcast_due && (millis() - _t_last_bcast) >= MIN_BROADCAST_GAP_MS) {
        broadcast_now();
    }

    push_new_log_lines();

    if (_restart_due && (millis() - _t_restart) >= RESTART_DELAY_MS) {
        esp_restart();   // into the firmware on_update() just installed
    }
}


void WebApp::request_broadcast()
{
    _broadcast_due = true;
}


bool WebApp::next_action(Action& out)
{
    if (_actions == nullptr) return false;
    return xQueueReceive(_actions, &out, 0) == pdTRUE;
}


/* =======================================================================
 *  Servers
 * ======================================================================= */

bool WebApp::start_https()
{
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    // The PEM text includes its trailing NUL, which is what mbedTLS expects.
    conf.servercert     = server_crt_start;
    conf.servercert_len = static_cast<size_t>(server_crt_end - server_crt_start);
    conf.prvtkey_pem    = server_key_start;
    conf.prvtkey_len    = static_cast<size_t>(server_key_end - server_key_start);

    conf.httpd.max_open_sockets = MAX_SOCKETS;
    conf.httpd.max_uri_handlers = 4;      // ws, state, update, files
    conf.httpd.max_req_hdr_len  = 2048;   // the core's 1024 is close to what a phone sends; it only grows as needed
    conf.httpd.uri_match_fn     = httpd_uri_match_wildcard;

    if (httpd_ssl_start(&_https, &conf) != ESP_OK) {
        logbook::add("Web: HTTPS server failed to start -- check the certificate (tools/make_certs.py)");
        _https = nullptr;
        return false;
    }

    // Handlers are matched in the order they're registered, so the catch-all
    // for the web files goes last.
    httpd_uri_t ws = {};
    ws.uri          = "/ws";
    ws.method       = HTTP_GET;
    ws.handler      = on_ws;
    ws.user_ctx     = this;
    ws.is_websocket = true;
    httpd_register_uri_handler(_https, &ws);

    httpd_uri_t state = {};
    state.uri      = "/api/state";   // plain JSON read, handy for curl / Home Assistant
    state.method   = HTTP_GET;
    state.handler  = on_state;
    state.user_ctx = this;
    httpd_register_uri_handler(_https, &state);

    httpd_uri_t update = {};
    update.uri      = "/api/update";
    update.method   = HTTP_POST;
    update.handler  = on_update;
    update.user_ctx = this;
    httpd_register_uri_handler(_https, &update);

    httpd_uri_t asset = {};
    asset.uri      = "/*";
    asset.method   = HTTP_GET;
    asset.handler  = on_asset;
    asset.user_ctx = this;
    httpd_register_uri_handler(_https, &asset);

    return true;
}


bool WebApp::start_redirect()
{
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();   // port 80; its control port differs from the HTTPS server's

    conf.max_open_sockets = 2;
    conf.max_uri_handlers = 1;
    conf.lru_purge_enable = true;
    conf.uri_match_fn     = httpd_uri_match_wildcard;

    if (httpd_start(&_redirect, &conf) != ESP_OK) {
        logbook::add("Web: port 80 redirect failed to start");
        _redirect = nullptr;
        return false;
    }

    httpd_uri_t all = {};
    all.uri      = "/*";
    all.method   = HTTP_GET;
    all.handler  = on_redirect;
    all.user_ctx = this;
    httpd_register_uri_handler(_redirect, &all);

    return true;
}


/* =======================================================================
 *  Handlers (run on the server's task)
 * ======================================================================= */

esp_err_t WebApp::on_asset(httpd_req_t* req)
{
    WebApp*      self = static_cast<WebApp*>(req->user_ctx);
    const size_t len  = strcspn(req->uri, "?#");   // ignore any query string

    for (const Asset& a : ASSETS) {
        if (strlen(a.path) == len && strncmp(a.path, req->uri, len) == 0) {
            httpd_resp_set_type(req, a.type);
            httpd_resp_set_hdr(req, "Cache-Control", a.cache);

            if (a.start == sw_js_start) {   // with the UI fingerprint appended
                return httpd_resp_send(req, self->_sw_js.data(), self->_sw_js.size());
            }
            return httpd_resp_send(req, reinterpret_cast<const char*>(a.start), a.end - a.start);
        }
    }

    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
}


esp_err_t WebApp::on_state(httpd_req_t* req)
{
    WebApp* self = static_cast<WebApp*>(req->user_ctx);

    String json;
    {
        std::lock_guard<std::mutex> lock(self->_state_mutex);
        json = self->_state_json;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json.c_str(), json.length());
}


/**
 * POST /api/update: a firmware.bin as the raw request body, with the update
 * key in an X-OTA-Key header. It's written into the spare app slot as it
 * arrives, checked as a whole, and made the boot image; tick() restarts into
 * it a moment later, on probation (see Ota.h). Anything that goes wrong
 * before that leaves the running firmware untouched.
 *
 * This holds the server's task for the whole upload (10-20 s), so other
 * requests and WebSocket pushes wait. The sound doesn't: Player runs on the
 * loop task.
 */
esp_err_t WebApp::on_update(httpd_req_t* req)
{
    WebApp* self = static_cast<WebApp*>(req->user_ctx);

    if (self->_update_key[0] == '\0') {
        return refuse_update(req, "403 Forbidden", "Updates are off: set ota_key in include/config/secrets.h.");
    }
    char key[MAX_KEY_LEN + 1] = "";
    const esp_err_t got = httpd_req_get_hdr_value_str(req, "X-OTA-Key", key, sizeof(key));
    if (got == ESP_ERR_NOT_FOUND) {
        return refuse_update(req, "403 Forbidden", "No update key sent. (From pio: is WNM_OTA_KEY set?)");
    }
    if (got != ESP_OK || strcmp(key, self->_update_key) != 0) {
        return refuse_update(req, "403 Forbidden", "Wrong update key.");
    }
    if (self->_restart_due) {
        return refuse_update(req, "503 Service Unavailable", "The box is already restarting into a new firmware.");
    }

    const esp_partition_t* slot = esp_ota_get_next_update_partition(nullptr);
    if (slot == nullptr) {
        return refuse_update(req, "500 Internal Server Error",
                             "No spare firmware slot. Flash over USB once to install the partition table.");
    }
    if (req->content_len == 0 || req->content_len > slot->size) {
        return refuse_update(req, "400 Bad Request", "That file is the wrong size for a firmware image.");
    }

    esp_ota_handle_t ota = 0;
    esp_err_t err = esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &ota);   // erases as it goes, no long pause up front
    if (err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
        return refuse_update(req, "503 Service Unavailable",
                             "The box is still checking its current firmware. Try again in a minute.");
    }
    if (err != ESP_OK) {
        return refuse_update(req, "500 Internal Server Error", esp_err_to_name(err));
    }

    logbook::add("OTA: receiving %u bytes", (unsigned)req->content_len);

    std::unique_ptr<char[]> buf(new char[UPDATE_CHUNK]);
    size_t left    = req->content_len;
    int    waiting = 0;

    while (left > 0) {
        const int n = httpd_req_recv(req, buf.get(), std::min(left, UPDATE_CHUNK));
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++waiting < 3) continue;   // a slow moment on Wi-Fi
        if (n <= 0) {
            esp_ota_abort(ota);
            logbook::add("OTA: upload cut off; nothing changed");
            return ESP_FAIL;   // the connection is gone, so there's no one to answer
        }
        waiting = 0;

        err = esp_ota_write(ota, buf.get(), n);
        if (err != ESP_OK) {
            esp_ota_abort(ota);
            return refuse_update(req, "400 Bad Request", err == ESP_ERR_OTA_VALIDATE_FAILED
                                     ? "That isn't a firmware image. Pick firmware.bin."
                                     : esp_err_to_name(err));
        }
        left -= n;
    }

    err = esp_ota_end(ota);   // checks the whole image: format, chip, checksum, SHA-256
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(slot);
    }
    if (err != ESP_OK) {
        return refuse_update(req, "400 Bad Request", err == ESP_ERR_OTA_VALIDATE_FAILED
                                 ? "That image failed its checks: damaged, or built for another chip."
                                 : esp_err_to_name(err));
    }

    logbook::add("OTA: installed; restarting into it");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Installed. Restarting.");

    self->_t_restart   = millis();
    self->_restart_due = true;
    return ESP_OK;
}


esp_err_t WebApp::on_ws(httpd_req_t* req)
{
    WebApp* self = static_cast<WebApp*>(req->user_ctx);

    // The handshake arrives as a GET. Push state to the new phone -- from
    // loop(), the only task allowed to read Player.
    if (req->method == HTTP_GET) {
        logbook::add("Web: a phone connected");
        self->_broadcast_due = true;
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {};
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);   // header only: type and length
    if (err != ESP_OK) return err;

    if (frame.len > MAX_WS_MESSAGE) return ESP_FAIL;      // not from our UI -- drop the connection

    uint8_t buf[MAX_WS_MESSAGE];
    frame.payload = buf;
    if (frame.len > 0) {
        err = httpd_ws_recv_frame(req, &frame, frame.len);
        if (err != ESP_OK) return err;
    }

    if (frame.type == HTTPD_WS_TYPE_TEXT) {
        self->handle_ws_message(req, buf, frame.len);
    }
    return ESP_OK;
}


esp_err_t WebApp::on_redirect(httpd_req_t* req)
{
    char host[64] = "whitenoise.local";
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));   // keeps the default if missing

    const std::string location = std::string("https://") + host + req->uri;

    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", location.c_str());
    return httpd_resp_send(req, nullptr, 0);
}


/** Sends one message to every connected phone (or every log subscriber). Runs on the server's task. */
void WebApp::send_to_all(void* arg)
{
    std::unique_ptr<Outgoing> out(static_cast<Outgoing*>(arg));

    int    fds[MAX_SOCKETS];
    size_t count = MAX_SOCKETS;
    if (httpd_get_client_list(out->server, &count, fds) != ESP_OK) return;

    httpd_ws_frame_t frame = {};
    frame.type    = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<uint8_t*>(out->text.data());
    frame.len     = out->text.size();

    for (size_t i = 0; i < count; i++) {
        if (httpd_ws_get_fd_info(out->server, fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
        if (out->log_only && httpd_sess_get_ctx(out->server, fds[i]) != &LOG_SUBSCRIBER) continue;

        httpd_ws_send_frame_async(out->server, fds[i], &frame);
    }
}


/* =======================================================================
 *  State and commands
 * ======================================================================= */

void WebApp::handle_ws_message(httpd_req_t* req, const uint8_t* data, size_t len)
{
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;   // malformed; ignore

    const char* cmd   = doc["cmd"]   | "";
    const int   track = doc["track"] | 0;
    const int   value = doc["value"] | -1;

    // Checked before narrowing to a byte, so e.g. a volume of -1 can't wrap
    // around to 255 (which used to clamp to full volume).
    const bool track_ok  = (track >= 1 && track <= Player::TRACK_COUNT);
    const bool volume_ok = (value >= 0 && value <= Player::VOL_MAX);

    using K = Action::Kind;
    Action a;

    if      (!strcmp(cmd, "play")     && track_ok)  { a.kind = K::SELECT_TRACK; a.arg = (uint8_t)track; }
    else if (!strcmp(cmd, "volume")   && volume_ok) { a.kind = K::SET_VOLUME;   a.arg = (uint8_t)value; }
    else if (!strcmp(cmd, "pause"))                 { a.kind = K::PAUSE;    }
    else if (!strcmp(cmd, "resume"))                { a.kind = K::RESUME;   }
    else if (!strcmp(cmd, "toggle"))                { a.kind = K::TOGGLE;   }
    else if (!strcmp(cmd, "stop"))                  { a.kind = K::STOP;     }
    else if (!strcmp(cmd, "favorite") && track_ok)  { a.kind = K::FAVORITE;     a.arg = (uint8_t)track; }
    else if (!strcmp(cmd, "state"))                 { _broadcast_due = true; return; }
    else if (!strcmp(cmd, "log_on"))                { subscribe_to_log(req); return; }
    else if (!strcmp(cmd, "log_off"))               { req->sess_ctx = nullptr; return; }
    else return;   // unknown, or out of range

    if (_actions) xQueueSend(_actions, &a, 0);
}


/**
 * Marks this phone as having the diagnostics panel open, and sends it the
 * history so far. History goes up to the last line already pushed live, so
 * nothing arrives twice; anything newer comes with the next push.
 */
void WebApp::subscribe_to_log(httpd_req_t* req)
{
    req->sess_ctx = &LOG_SUBSCRIBER;
    req->free_ctx = keep_subscriber;

    std::string lines;
    logbook::copy(0, _log_sent, lines);
    std::string msg = log_message("L", lines);

    httpd_ws_frame_t frame = {};
    frame.type    = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<uint8_t*>(msg.data());
    frame.len     = msg.size();
    httpd_ws_send_frame(req, &frame);
}


/** Runs on the loop task: sends log lines added since the last push to every subscriber. */
void WebApp::push_new_log_lines()
{
    const uint32_t newest = logbook::newest();
    const uint32_t sent   = _log_sent;
    if (newest == sent) return;

    std::string lines;
    logbook::copy(sent, newest, lines);
    _log_sent = newest;

    queue_send(log_message("l", lines), true);
}


void WebApp::build_state_json(String& out)
{
    const Player::State& s = _player.state();

    JsonDocument doc;
    doc["t"]     = "s";
    doc["play"]  = s.playing;
    doc["trk"]   = s.track;
    doc["vol"]   = s.volume;       // setpoint: where the slider sits
    doc["vnow"]  = s.volume_now;   // actual: the number you watch walk
    doc["fade"]  = s.fading;
    doc["hw"]    = (uint8_t)s.hw;  // Player::Hw: 0 warming, 1 ok, 2 no replies, 3 fault
    doc["favs"]  = s.favorites;
    doc["rssi"]  = _net.rssi();
    doc["ip"]    = _net.ip();
    doc["mdns"]  = _net.mdns_up();
    doc["up"]    = millis() / 1000;
    doc["ui"]    = _ui_build;      // fingerprint of the built-in web files
    doc["fw"]    = ota::firmware_id();
    doc["rb"]    = ota::last_update_failed();   // the last update was rolled back

    serializeJson(doc, out);
}


/** Runs on the loop task: refreshes the snapshot and hands a copy to the server task to send. */
void WebApp::broadcast_now()
{
    _broadcast_due = false;
    _t_last_bcast  = millis();

    String json;
    build_state_json(json);

    {
        std::lock_guard<std::mutex> lock(_state_mutex);
        _state_json = json;
    }

    queue_send(std::string(json.c_str(), json.length()), false);
}


/** Hands a message to the server task, which is the only one allowed to send it. */
void WebApp::queue_send(std::string text, bool log_only)
{
    if (_https == nullptr) return;

    auto* out = new Outgoing{ _https, std::move(text), log_only };
    if (httpd_queue_work(_https, send_to_all, out) != ESP_OK) {
        delete out;   // queue full -- dropped; the next state push or log push catches up
    }
}
