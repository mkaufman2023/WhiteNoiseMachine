/*************************************************************************
*                                                                        *
*   WebApp.cpp                                                           *
*   Library for handling static and dynamic routes and mDNS setup.       *
*                                                                        *
*   Written by Matt Kaufman - December 5, 2025                           *
*                                                                        *
**************************************************************************/

#include "WebApp.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <DFPlayerMini.h>


namespace webserver
{
    constexpr int port = 80;
    const char* hostname = "whitenoise";
    const char* hostname_full = "http://whitenoise.local";
}



WebApp::WebApp(
    DFPlayerMini& player,
    String& log_buffer
) : _server(webserver::port), _player(player), _log_buffer(log_buffer) { }


void WebApp::begin()
{
    setup_routes();

    _server.begin();
    Serial.println("HTTP server started on port " + String(webserver::port));

    mDNS_is_setup = setup_mdns();
}


void WebApp::handle_client()
{
    _server.handleClient();
}


void WebApp::log()
{
    _log_buffer += "\n";

    // Prevent memory overflow
    if (_log_buffer.length() > 2000) { 
        _log_buffer = _log_buffer.substring(_log_buffer.length() - 1000); 
    }
}


void WebApp::log(String msg)
{
    unsigned long ms = millis();
    unsigned long sec = ms / 1000;
    unsigned long rem = ms % 1000;

    // Create formatted timestamp string
    // %lu   = unsigned long
    // %03lu = unsigned long, at least 3 digits, zero-padded (e.g. 5 -> 005)
    char time_str[20];
    snprintf(time_str, sizeof(time_str), "[%lu.%03lu]", sec, rem);

    // Combine with the message
    String timestamped_msg = String(time_str) + " >> " + msg;

    // Append to web buffer
    _log_buffer += timestamped_msg + "\n";

    // Prevent memory overflow
    if (_log_buffer.length() > 2000) { 
        _log_buffer = _log_buffer.substring(_log_buffer.length() - 1000); 
    }
}


bool WebApp::setup_mdns()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Skipping mDNS setup (WiFi not connected).");
        return false;
    }

    if (!MDNS.begin(webserver::hostname)) {
        Serial.println("Error setting up MDNS responder!");
        return false;
    }

    MDNS.addService("http", "tcp", webserver::port);
    Serial.println("mDNS responder started: " + String(webserver::hostname_full));
    return true;
}


void WebApp::setup_routes()
{
    // Serve static files
    _server.serveStatic("/", SPIFFS, "/index.html");

    // PWA handlers
    _server.serveStatic("/icon.svg", SPIFFS, "/icon.svg");
    _server.serveStatic("/manifest.json", SPIFFS, "/manifest.json");
    _server.serveStatic("/apple-touch-icon.png", SPIFFS, "/icon.svg");

    // Dynamic endpoints
    _server.on("/log", HTTP_GET, [this]() { handle_log(); });
    _server.on("/play", HTTP_GET, [this]() { handle_play(); });
    _server.on("/status", HTTP_GET, [this]() { handle_status(); });
    
    // Playback controls
    _server.on("/previous", HTTP_GET, [this]() { handle_previous(); });
    _server.on("/pause", HTTP_GET, [this]() { handle_pause(); });
    _server.on("/resume", HTTP_GET, [this]() { handle_resume(); });
    _server.on("/next", HTTP_GET, [this]() { handle_next(); });
    _server.on("/volume_down", HTTP_GET, [this]() { handle_volume_down(); });
    _server.on("/stop", HTTP_GET, [this]() { handle_stop(); });
    _server.on("/volume_up", HTTP_GET, [this]() { handle_volume_up(); });
    _server.on("/start_repeat", HTTP_GET, [this]() { handle_start_repeat(); });
    _server.on("/stop_repeat", HTTP_GET, [this]() { handle_stop_repeat(); });
    _server.on("/set_eq_normal", HTTP_GET, [this]() { handle_set_EQ_normal(); });
    _server.on("/set_eq_rock", HTTP_GET, [this]() { handle_set_EQ_rock(); });
    _server.on("/set_eq_pop", HTTP_GET, [this]() { handle_set_EQ_pop(); });

    // 404 handler
    _server.onNotFound([this]() { handle_not_found(); });
}


void WebApp::handle_log()
{
    _server.send(200, "text/plain", _log_buffer);
    _log_buffer = "";
}


void WebApp::handle_not_found()
{
   _server.send(404, "text/plain", "Not found");
}


void WebApp::handle_play()
{
    if (!_server.hasArg("track")) {
        _server.send(400, "text/plain", "Missing 'track'");
        return;
    }

    int track = _server.arg("track").toInt();

    log();
    log("Received call to /play endpoint with track=" + String(track));

    if (track <= 0) {
        log("Invalid track number received: " + String(track));
        _server.send(400, "text/plain", "Invalid track number.");
        return;
    }

    _player.loop_track(track);
    log("Called DFPlayer.loop_track(track)");

    _server.send(200, "text/plain", "Looping track " + String(track));
}


void WebApp::handle_status()
{
    log();
    log("Received call to /status endpoint");
    _server.send(200, "text/plain", "1");
}


void WebApp::handle_previous()
{
    log();
    log("Received call to /previous endpoint");
    _player.play_previous();
    log("Called DFPlayer.play_previous()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_pause()
{
    log();
    log("Received call to /pause endpoint");
    _player.pause();
    log("Called DFPlayer.pause()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_resume()
{
    log();
    log("Received call to /resume endpoint");
    _player.play();
    log("Called DFPlayer.play()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_next()
{
    log();
    log("Received call to /next endpoint");
    _player.play_next();
    log("Called DFPlayer.play_next()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_volume_down()
{
    log();
    log("Received call to /volume_down endpoint");
    _player.decrement_volume();
    log("Called DFPlayer.decrement_volume()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_stop()
{
    log();
    log("Received call to /stop endpoint");
    _player.stop_all_playback();
    log("Called DFPlayer.stop_all_playback()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_volume_up()
{
    log();
    log("Received call to /volume_up endpoint");
    _player.increment_volume();
    log("Called DFPlayer.increment_volume()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_start_repeat()
{
    log();
    log("Received call to /start_repeat endpoint");
    _player.start_looping_current_track();
    log("Called DFPlayer.start_looping_current_track()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_stop_repeat()
{
    log();
    log("Received call to /stop_repeat endpoint");
    _player.stop_looping_current_track();
    log("Called DFPlayer.stop_looping_current_track()");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_set_EQ_normal()
{
    log();
    log("Received call to /set_eq_normal endpoint");
    _player.set_EQ(0);
    log("Called DFPlayer.set_EQ(0)");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_set_EQ_rock()
{
    log();
    log("Received call to /set_eq_rock endpoint");
    _player.set_EQ(1);
    log("Called DFPlayer.set_EQ(1)");
    _server.send(200, "text/plain", "OK");
}


void WebApp::handle_set_EQ_pop()
{
    log();
    log("Received call to /set_eq_pop endpoint");
    _player.set_EQ(3);
    log("Called DFPlayer.set_EQ(3)");
    _server.send(200, "text/plain", "OK");
}