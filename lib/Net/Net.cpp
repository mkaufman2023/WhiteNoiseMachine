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


Net::Net(const Credentials* networks, uint8_t count, const char* hostname)
    : _nets(networks), _count(count), _hostname(hostname) { }


void Net::begin()
{
    WiFi.persistent(false);          // don't wear out flash storing credentials
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hostname);     // must be set before connecting
    WiFi.setAutoReconnect(true);

    // Modem sleep adds 100-200 ms of latency to the first packet after an idle
    // period, which on a WebSocket control UI reads as "the first tap lags".
    WiFi.setSleep(false);

    start_attempt(0);
}


void Net::tick()
{
    const uint32_t now = millis();
    const bool     up  = (WiFi.status() == WL_CONNECTED);

    switch (_st) {
        case St::CONNECTING:
            if (up) {
                on_connected();
            }
            else if (now - _t_state >= ATTEMPT_TIMEOUT_MS) {
                Serial.println("Net: timeout on " + String(_nets[_index].ssid));

                const uint8_t next = (uint8_t)(_index + 1);
                if (next < _count) {
                    start_attempt(next);
                }
                else {
                    // Tried every network. Pause before sweeping again so we
                    // aren't thrashing the radio forever.
                    WiFi.disconnect();
                    _st      = St::BACKOFF;
                    _t_state = now;
                }
            }
            break;

        case St::ONLINE:
            if (!up) on_lost();
            break;

        case St::BACKOFF:
            if (now - _t_state >= RETRY_BACKOFF_MS) start_attempt(0);
            break;

        case St::IDLE:
        default:
            break;
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


const char* Net::ssid() const
{
    return (_index < _count) ? _nets[_index].ssid : "";
}


bool Net::consume_link_change()
{
    if (!_link_changed) return false;
    _link_changed = false;
    return true;
}


void Net::start_attempt(uint8_t index)
{
    _index   = index;
    _st      = St::CONNECTING;
    _t_state = millis();

    Serial.println("Net: connecting to " + String(_nets[index].ssid));

    WiFi.disconnect();
    WiFi.begin(_nets[index].ssid, _nets[index].password);
}


void Net::on_connected()
{
    _st           = St::ONLINE;
    _t_state      = millis();
    _link_changed = true;

    Serial.println("Net: online as " + WiFi.localIP().toString());

    start_mdns();
}


void Net::on_lost()
{
    Serial.println("Net: link lost");

    _mdns_up      = false;
    _link_changed = true;

    MDNS.end();          // must be torn down or the re-advertisement won't stick
    start_attempt(0);
}


void Net::start_mdns()
{
    MDNS.end();          // safe to call when not running; clears stale records

    if (!MDNS.begin(_hostname)) {
        Serial.println("Net: mDNS failed to start");
        _mdns_up = false;
        return;
    }

    MDNS.addService("http", "tcp", 80);
    _mdns_up = true;

    Serial.println("Net: mDNS up at http://" + String(_hostname) + ".local");
}
