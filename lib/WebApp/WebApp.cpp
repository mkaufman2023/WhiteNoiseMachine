/******************************************************************
*                                                                 *
*    lib / WebApp / WebApp.cpp                                    *
*    Created by Matt Kaufman, December 5, 2025.                   *
*                                                                 *
*    Implementation of the WebApp class designed to               *
*      handle routes, play/status handlers, and mDNS setup.       *
*                                                                 *
*******************************************************************/

#include "WebApp.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <DFPlayerMini.h>
#include <config/webserver.h>



WebApp::WebApp(
    DFPlayerMini& player
) : _server(webserver::port), _player(player) { }


void WebApp::begin() {
    setup_routes();

    _server.begin();
    Serial.println("HTTP server started on port " + String(webserver::port));

    setup_mdns();
}


void WebApp::handle_client() {
    _server.handleClient();
}


void WebApp::setup_routes() {
    // Serve static files
    _server.serveStatic("/", SPIFFS, "/index.html");

    // PWA handlers
    _server.serveStatic("/icon.svg", SPIFFS, "/icon.svg");
    _server.serveStatic("/manifest.json", SPIFFS, "/manifest.json");
    _server.serveStatic("/apple-touch-icon.png", SPIFFS, "/icon.svg");  // Reuse SVG for iOS fallback

    // Dynamic endpoints
    _server.on("/play", HTTP_GET, [this]() { handle_play(); });
    _server.on("/status", HTTP_GET, [this]() { handle_status(); });
}


void WebApp::setup_mdns() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Skipping mDNS setup (WiFi not connected).");
        return;
    }

    if (!MDNS.begin(webserver::hostname)) {
        Serial.println("Error setting up MDNS responder!");
        return;
    }

    MDNS.addService("http", "tcp", webserver::port);
    Serial.println("mDNS responder started: " + String(webserver::hostname_full));
}


void WebApp::handle_play() {
    if (!_player.is_online()) {
        _server.send(500, "text/plain", "DFPlayer not initialized.");
        return;
    }

    if (!_server.hasArg("track")) {
        _server.send(400, "text/plain", "Missing 'track'");
        return;
    }

    int track = _server.arg("track").toInt();
    _player.loop_track(track);
    _server.send(200, "text/plain", "Looping track " + String(track));
}


void WebApp::handle_status() {
    _server.send(200, "text/plain", _player.is_online() ? "1" : "0");
}
