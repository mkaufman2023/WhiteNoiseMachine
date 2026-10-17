/*************************************************************************
*                                                                        *
*   WebApp.cpp                                                           *
*   Async HTTP + WebSocket interface for the White Noise Machine.        *
*                                                                        *
*   Written by Matt Kaufman - December 5, 2025                           *
*   Rewritten for async + WebSocket - July 2026                          *
*                                                                        *
**************************************************************************/

#include "WebApp.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Player.h>
#include <Net.h>


WebApp::WebApp(Player& player, Net& net)
    : _server(PORT), _ws("/ws"), _player(player), _net(net) { }


void WebApp::begin()
{
    _actions = xQueueCreate(ACTION_QUEUE_LEN, sizeof(Action));

    _ws.onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c,
                       AwsEventType t, void* arg, uint8_t* d, size_t len) {
        handle_ws_event(s, c, t, arg, d, len);
    });

    _server.addHandler(&_ws);

    setup_routes();
    _server.begin();

    Serial.println("WebApp: async server listening on port " + String(PORT));
}


void WebApp::setup_routes()
{
    // index.html is served no-cache so a firmware/UI update always lands. The
    // service worker handles making it feel instant anyway.
    _server.serveStatic("/", LittleFS, "/")
           .setDefaultFile("index.html")
           .setCacheControl("no-cache");

    // Icons and the manifest are immutable per release, so let the phone keep them.
    _server.serveStatic("/icon-180.png",  LittleFS, "/icon-180.png").setCacheControl("max-age=604800");
    _server.serveStatic("/icon-512.png",  LittleFS, "/icon-512.png").setCacheControl("max-age=604800");
    _server.serveStatic("/icon.svg",      LittleFS, "/icon.svg").setCacheControl("max-age=604800");
    _server.serveStatic("/manifest.json", LittleFS, "/manifest.json").setCacheControl("max-age=86400");

    // The service worker must never be cached, or updates can't propagate.
    _server.serveStatic("/sw.js", LittleFS, "/sw.js").setCacheControl("no-cache");

    // iOS looks for this by convention.
    _server.serveStatic("/apple-touch-icon.png", LittleFS, "/icon-180.png")
           .setCacheControl("max-age=604800");

    // Plain-HTTP state read, handy for curl / Home Assistant / debugging.
    _server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* req) {
        String json;
        build_state_json(json);
        req->send(200, "application/json", json);
    });

    _server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });
}


void WebApp::tick()
{
    _ws.cleanupClients();

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
 *  WebSocket
 * ======================================================================= */

void WebApp::handle_ws_event(AsyncWebSocket* /*server*/, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.println("WebApp: client #" + String(client->id()) + " connected");
            // Don't read Player from this task -- ask loop() to push state.
            _broadcast_due = true;
            break;

        case WS_EVT_DISCONNECT:
            Serial.println("WebApp: client #" + String(client->id()) + " disconnected");
            if (_log_subs > 0) _log_subs--;
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                handle_ws_message(client, data, len);
            }
            break;
        }

        case WS_EVT_ERROR:
        case WS_EVT_PONG:
        default:
            break;
    }
}


void WebApp::handle_ws_message(AsyncWebSocketClient* client, const uint8_t* data, size_t len)
{
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;   // malformed; ignore

    const char* cmd = doc["cmd"] | "";
    Action a;

    if      (!strcmp(cmd, "play"))     { a.kind = Action::Kind::SELECT_TRACK; a.arg = doc["track"] | 0; }
    else if (!strcmp(cmd, "volume"))   { a.kind = Action::Kind::SET_VOLUME;   a.arg = doc["value"] | 0; }
    else if (!strcmp(cmd, "pause"))    { a.kind = Action::Kind::PAUSE;    }
    else if (!strcmp(cmd, "resume"))   { a.kind = Action::Kind::RESUME;   }
    else if (!strcmp(cmd, "toggle"))   { a.kind = Action::Kind::TOGGLE;   }
    else if (!strcmp(cmd, "stop"))     { a.kind = Action::Kind::STOP;     }
    else if (!strcmp(cmd, "favorite")) { a.kind = Action::Kind::FAVORITE; a.arg = doc["track"] | 0; }
    else if (!strcmp(cmd, "state"))    { _broadcast_due = true; return; }
    else if (!strcmp(cmd, "log_on"))   { _log_subs++;                    return; }
    else if (!strcmp(cmd, "log_off"))  { if (_log_subs > 0) _log_subs--;  return; }
    else return;

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


void WebApp::broadcast_now()
{
    _broadcast_due = false;
    _t_last_bcast  = millis();

    if (_ws.count() == 0) return;

    String json;
    build_state_json(json);
    _ws.textAll(json);
}


void WebApp::log(const String& msg)
{
    const uint32_t ms  = millis();
    char           ts[16];
    snprintf(ts, sizeof(ts), "[%lu.%03lu]", ms / 1000, ms % 1000);

    const String line = String(ts) + " >> " + msg;
    Serial.println(line);

    if (_log_subs <= 0 || _ws.count() == 0) return;

    JsonDocument doc;
    doc["t"] = "l";
    doc["m"] = line;

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);
}
