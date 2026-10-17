/*************************************************************************
*                                                                        *
*   Net.cpp                                                              *
*   Non-blocking Wi-Fi connection manager with mDNS re-advertisement.    *
*                                                                        *
*   Written by Matt Kaufman - July 2026                                  *
*                                                                        *
**************************************************************************/

#include "Net.h"

#include <WiFi.h>
#include <ESPmDNS.h>


Net::Net(const char* ssid, const char* password, const char* hostname)
    : _ssid(ssid), _password(password), _hostname(hostname) { }


void Net::begin()
{
    WiFi.persistent(false);          // don't wear out flash storing credentials

    // The core only stores the hostname here; WiFi.mode() is what applies it
    // to the station interface, so this has to come first.
    WiFi.setHostname(_hostname);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // Modem sleep adds 100-200 ms of latency to the first packet after an idle
    // period, which on a WebSocket control UI reads as "the first tap lags".
    WiFi.setSleep(false);

    connect();
}


void Net::tick()
{
    const bool up = (WiFi.status() == WL_CONNECTED);

    if (up != _up) {
        _up           = up;
        _link_changed = true;
        up ? on_up() : on_down();
    }

    // Auto-reconnect covers most drops, but not all of them (e.g. a handshake
    // timeout while the router reboots). If we've been down this long, it has
    // given up -- start over.
    if (!_up && (millis() - _t_down) >= RESTART_AFTER_MS) {
        Serial.println("Net: still offline, restarting the connection");
        WiFi.disconnect();
        connect();
    }
}


bool Net::connected() const
{
    return WiFi.status() == WL_CONNECTED;
}


int8_t Net::rssi() const
{
    return connected() ? (int8_t)WiFi.RSSI() : (int8_t)0;
}


String Net::ip() const
{
    return connected() ? WiFi.localIP().toString() : String("0.0.0.0");
}


bool Net::consume_link_change()
{
    if (!_link_changed) return false;
    _link_changed = false;
    return true;
}


void Net::connect()
{
    Serial.println("Net: connecting to " + String(_ssid));

    WiFi.begin(_ssid, _password);
    _t_down = millis();
}


void Net::on_up()
{
    Serial.println("Net: online as " + WiFi.localIP().toString() +
                   " (" + String(WiFi.RSSI()) + " dBm)");

    start_mdns();
}


void Net::on_down()
{
    Serial.println("Net: link lost");

    _mdns_up = false;
    _t_down  = millis();

    MDNS.end();          // must be torn down or the re-advertisement won't stick
}


void Net::start_mdns()
{
    MDNS.end();          // safe to call when not running; clears stale records

    if (!MDNS.begin(_hostname)) {
        Serial.println("Net: mDNS failed to start");
        _mdns_up = false;
        return;
    }

    MDNS.addService("https", "tcp", 443);
    _mdns_up = true;

    Serial.println("Net: mDNS up at https://" + String(_hostname) + ".local");
}
