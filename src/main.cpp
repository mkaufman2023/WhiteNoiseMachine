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

// Global log buffer for messages to server
String log_buffer = "";

// Globals for debug info on web app
bool wifi_is_connected = false;
bool mDNS_is_setup = false;
String wifi_IP_address = "";


void setup_mDNS();
void connect_to_wifi();
void setup_HTTP_server();

void handle_play();
void handle_status();
void handle_root();
void handle_icon();
void handle_manifest();
void handle_not_found();
void handle_log();
void handle_previous();
void handle_pause();
void handle_resume();
void handle_next();
void handle_volume_up();
void handle_volume_down();
void handle_stop();
void handle_start_repeat();
void handle_stop_repeat();
void handle_set_EQ_normal();
void handle_set_EQ_pop();
void handle_set_EQ_rock();

void log_to_web_blank();
void log_to_web(String msg);



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

    DFPlayer.begin();  // Enable debug messages
    Serial.println("--- Setup complete. ---");

    log_to_web_blank();
    log_to_web("Setup complete.");
    log_to_web("WiFi connected: " + String(wifi_is_connected));
    log_to_web("mDNS is setup: " + String(mDNS_is_setup));
    log_to_web("WiFi IP address: " + wifi_IP_address);
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
    if (!server.hasArg("track")) {
        server.send(400, "text/plain", "Missing 'track'");
        return;
    }

    int track = server.arg("track").toInt();

    log_to_web_blank();
    log_to_web("Received call to /play endpoint with track=" + String(track));

    DFPlayer.loop_track(track);
    log_to_web("Called DFPlayer.loop_track(track)");

    server.send(200, "text/plain", "Looping track " + String(track));
}


void handle_status()
{
    server.send(200, "text/plain", "Status endpoint not yet implemented.");
}


void handle_log()
{
    server.send(200, "text/plain", log_buffer);
    log_buffer = "";
}


void handle_previous()
{
    log_to_web_blank();
    log_to_web("Received call to /previous endpoint");
    DFPlayer.play_previous();
    log_to_web("Called DFPlayer.play_previous()");
    server.send(200, "text/plain", "OK");
}


void handle_pause()
{
    log_to_web_blank();
    log_to_web("Received call to /pause endpoint");
    DFPlayer.pause();
    log_to_web("Called DFPlayer.pause()");
    server.send(200, "text/plain", "OK");
}


void handle_resume()
{
    log_to_web_blank();
    log_to_web("Received call to /resume endpoint");
    DFPlayer.play();
    log_to_web("Called DFPlayer.play()");
    server.send(200, "text/plain", "OK");
}


void handle_next()
{
    log_to_web_blank();
    log_to_web("Received call to /next endpoint");
    DFPlayer.play_next();
    log_to_web("Called DFPlayer.play_next()");
    server.send(200, "text/plain", "OK");
}


void handle_volume_down()
{
    log_to_web_blank();
    log_to_web("Received call to /volume_down endpoint");
    DFPlayer.decrement_volume();
    log_to_web("Called DFPlayer.decrement_volume()");
    server.send(200, "text/plain", "OK");
}


void handle_stop()
{
    log_to_web_blank();
    log_to_web("Received call to /stop endpoint");
    DFPlayer.stop_all_playback();
    log_to_web("Called DFPlayer.stop_all_playback()");
    server.send(200, "text/plain", "OK");
}


void handle_volume_up()
{
    log_to_web_blank();
    log_to_web("Received call to /volume_up endpoint");
    DFPlayer.increment_volume();
    log_to_web("Called DFPlayer.increment_volume()");
    server.send(200, "text/plain", "OK");
}


void handle_start_repeat()
{
    log_to_web_blank();
    log_to_web("Received call to /start_repeat endpoint");
    DFPlayer.start_looping_current_track();
    log_to_web("Called DFPlayer.start_looping_current_track()");
    server.send(200, "text/plain", "OK");
}


void handle_stop_repeat()
{
    log_to_web_blank();
    log_to_web("Received call to /stop_repeat endpoint");
    DFPlayer.stop_looping_current_track();
    log_to_web("Called DFPlayer.stop_looping_current_track()");
    server.send(200, "text/plain", "OK");
}


void handle_set_EQ_normal()
{
    log_to_web_blank();
    log_to_web("Received call to /set_eq_normal endpoint");
    DFPlayer.set_EQ(0);
    log_to_web("Called DFPlayer.set_EQ(0)");
    server.send(200, "text/plain", "OK");
}


void handle_set_EQ_rock()
{
    log_to_web_blank();
    log_to_web("Received call to /set_eq_rock endpoint");
    DFPlayer.set_EQ(1);
    log_to_web("Called DFPlayer.set_EQ(1)");
    server.send(200, "text/plain", "OK");
}


void handle_set_EQ_pop()
{
    log_to_web_blank();
    log_to_web("Received call to /set_eq_pop endpoint");
    DFPlayer.set_EQ(3);
    log_to_web("Called DFPlayer.set_EQ(3)");
    server.send(200, "text/plain", "OK");
}


void handle_not_found()
{
   server.send(404, "text/plain", "Not found");
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
    
    server.on("/log", HTTP_GET, handle_log);
    
    server.on("/previous", HTTP_GET, handle_previous);
    server.on("/pause", HTTP_GET, handle_pause);
    server.on("/resume", HTTP_GET, handle_resume);
    server.on("/next", HTTP_GET, handle_next);
    server.on("/volume_down", HTTP_GET, handle_volume_down);
    server.on("/stop", HTTP_GET, handle_stop);
    server.on("/volume_up", HTTP_GET, handle_volume_up);
    server.on("/start_repeat", HTTP_GET, handle_start_repeat);
    server.on("/stop_repeat", HTTP_GET, handle_stop_repeat);
    server.on("/set_eq_normal", HTTP_GET, handle_set_EQ_normal);
    server.on("/set_eq_rock", HTTP_GET, handle_set_EQ_rock);
    server.on("/set_eq_pop", HTTP_GET, handle_set_EQ_pop);

    server.onNotFound(handle_not_found);
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


void log_to_web(String msg)
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
    String timestamped_msg = "[" + String(time_str) + "] >> " + msg;

    // Append to web buffer
    log_buffer += timestamped_msg + "\n";

    // Prevent memory overflow
    if (log_buffer.length() > 2000) { 
        log_buffer = log_buffer.substring(log_buffer.length() - 1000); 
    }
}


void log_to_web_blank()
{
    log_buffer += "\n";

    // Prevent memory overflow
    if (log_buffer.length() > 2000) { 
        log_buffer = log_buffer.substring(log_buffer.length() - 1000); 
    }
}