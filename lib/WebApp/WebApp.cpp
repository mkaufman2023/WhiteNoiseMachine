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

#include <cstring>
#include <memory>
#include <string>
#include <esp_https_server.h>
#include <ArduinoJson.h>
#include <Player.h>
#include <Net.h>


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

    /** A state message on its way to the server task, which does the sending. */
    struct Outgoing {
        httpd_handle_t server;
        std::string    text;
    };
}


WebApp::WebApp(Player& player, Net& net)
    : _player(player), _net(net) { }


void WebApp::begin()
{
    _actions = xQueueCreate(ACTION_QUEUE_LEN, sizeof(Action));

    build_state_json(_state_json);   // so /api/state answers before the first broadcast

    if (start_https()) {
        Serial.println("WebApp: HTTPS server listening on port 443");
    }
    if (start_redirect()) {
        Serial.println("WebApp: port 80 redirects to HTTPS");
    }
}


void WebApp::tick()
{
    if (_broadcast_due && (millis() - _t_last_bcast) >= MIN_BROADCAST_GAP_MS) {
        broadcast_now();
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
    conf.httpd.max_uri_handlers = 4;
    conf.httpd.uri_match_fn     = httpd_uri_match_wildcard;

    if (httpd_ssl_start(&_https, &conf) != ESP_OK) {
        Serial.println("WebApp: HTTPS server failed to start -- check the certificate (tools/make_certs.py)");
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
        Serial.println("WebApp: port 80 redirect failed to start");
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
    const size_t len = strcspn(req->uri, "?#");   // ignore any query string

    for (const Asset& a : ASSETS) {
        if (strlen(a.path) == len && strncmp(a.path, req->uri, len) == 0) {
            httpd_resp_set_type(req, a.type);
            httpd_resp_set_hdr(req, "Cache-Control", a.cache);
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


esp_err_t WebApp::on_ws(httpd_req_t* req)
{
    WebApp* self = static_cast<WebApp*>(req->user_ctx);

    // The handshake arrives as a GET. Push state to the new phone -- from
    // loop(), the only task allowed to read Player.
    if (req->method == HTTP_GET) {
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
        self->handle_ws_message(buf, frame.len);
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


/** Sends one state message to every connected phone. Runs on the server's task. */
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
        if (httpd_ws_get_fd_info(out->server, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(out->server, fds[i], &frame);
        }
    }
}


/* =======================================================================
 *  State and commands
 * ======================================================================= */

void WebApp::handle_ws_message(const uint8_t* data, size_t len)
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
    else return;   // unknown, or out of range

    if (_actions) xQueueSend(_actions, &a, 0);
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

    if (_https == nullptr) return;

    auto* out = new Outgoing{ _https, std::string(json.c_str(), json.length()) };
    if (httpd_queue_work(_https, send_to_all, out) != ESP_OK) {
        delete out;   // queue full -- the next broadcast carries the same state anyway
    }
}
