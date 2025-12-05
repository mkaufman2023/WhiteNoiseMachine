#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DFPlayerMini.h>
#include <config/wifi.h>
#include <config/webserver.h>

DFPlayerMini DFPlayer;
WebServer server(webserver::port);


void setup_mDNS();
void handle_play();
void handle_status();
void connect_to_wifi();
void setup_HTTP_server();



void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- White Noise Machine Booting ---");

    // Initialize File System
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed!");
        return;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    connect_to_wifi();
    setup_HTTP_server();
    setup_mDNS();
}


void loop()
{
    server.handleClient();
    delay(2);
}



void connect_to_wifi()
{
    struct Network {
        const char* ssid;
        const char* password;
    };

    const Network networks[] = {
        { wifi::matthew::ssid, wifi::matthew::password },
        { wifi::delaney::ssid, wifi::delaney::password }
    };

    constexpr unsigned long timeout_ms = 20000;

    WiFi.mode(WIFI_STA);

    for (const auto& network : networks) {
        Serial.print("Connecting to WiFi: ");
        Serial.println(network.ssid);

        WiFi.begin(network.ssid, network.password);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            return;  // Exit on successful connection
        }

        Serial.println("\nFailed to connect to " + String(network.ssid) + ". Trying next network...");
    }

    Serial.println("All WiFi connection attempts FAILED (continuing without WiFi).");
}


void handle_play()
{
    if (!DFPlayer.is_online()) {
        server.send(500, "text/plain", "DFPlayer not initialized.");
        return;
    }

    if (!server.hasArg("track")) {
        server.send(400, "text/plain", "Missing 'track'");
        return;
    }

    int track = server.arg("track").toInt();
    DFPlayer.loop_track(track);
    server.send(200, "text/plain", "Looping track " + String(track));
}


void handle_status()
{
    server.send(200, "text/plain", DFPlayer.is_online() ? "1" : "0");
}


void setup_HTTP_server()
{
    // Serve static files
    server.serveStatic("/", SPIFFS, "/index.html");

    // PWA handlers
    server.serveStatic("/icon.svg", SPIFFS, "/icon.svg");
    server.serveStatic("/manifest.json", SPIFFS, "/manifest.json");
    server.serveStatic("/apple-touch-icon.png", SPIFFS, "/icon.svg"); // Reuse SVG for iOS fallback

    // Dynamic endpoints
    server.on("/play", HTTP_GET, handle_play);
    server.on("/status", HTTP_GET, handle_status);

    server.begin();
    Serial.println("HTTP server started on port " + String(webserver::port));
}


void setup_mDNS()
{
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