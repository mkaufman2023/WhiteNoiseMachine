#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <WebServer.h>

#include <WebApp.h>
#include <config/wifi.h>
#include <DFPlayerMini.h>


// Function prototypes
void connect_to_wifi();
bool setup_DFPlayer();


// Global log buffer for messages to server
String log_buffer = "";


// Globals for debug info on web app
bool wifi_is_connected = false;
String wifi_IP_address = "";
bool DFPlayer_OK = false;


// WiFi config
const char* WIFI_SSID = "ORBI";
const char* WIFI_PASS = "rjtmkk2448";


// MCU RX/TX pins for DFPlayer
const uint8_t mcu_rx = D7;   // XIAO pin going TO MP3 TX
const uint8_t mcu_tx = D6;   // XIAO pin going TO MP3 RX


// `DFPlayerMini` instance
DFPlayerMini DFPlayer(mcu_rx, mcu_tx);

// `WebApp` instance
WebApp web_app(DFPlayer, log_buffer);





void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- White Noise Box Booting ---");

    // Initialize File System
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed!");
        return;
    }

    // 1) Connect to Wi-Fi
    connect_to_wifi();

    // 2) Setup Web Server & mDNS
    web_app.begin();

    // 3) Start DFPlayer
    DFPlayer_OK = setup_DFPlayer();

    if (!DFPlayer_OK) {
        Serial.println("WARNING: System running without Audio hardware.");
    }

    Serial.println("--- Setup complete. ---");

    web_app.log();
    web_app.log("Setup complete.");
    web_app.log("WiFi connected: " + String(wifi_is_connected));
    web_app.log("mDNS is setup: " + String(web_app.mDNS_is_setup));
    web_app.log("WiFi IP address: " + wifi_IP_address);
    web_app.log("DFPlayer initialized: " + String(DFPlayer_OK));
}





void loop()
{
    // Handle HTTP requests
    web_app.handle_client();

    delay(10);
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
        // { wifi::delaney::ssid, wifi::delaney::password },
        // { wifi::matthew::ssid, wifi::matthew::password }
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


bool setup_DFPlayer()
{
    Serial.println("Initializing DFPlayer...");

    Serial.println("Starting DFPlayer serial comms...");
    DFPlayer.begin();
    delay(700);

    Serial.println("Selecting SD card (2) as source...");
    DFPlayer.set_source(2);
    delay(700);

    Serial.println("Setting volume to 30...");
    DFPlayer.set_volume(30);
    delay(700);

    Serial.println("Setting power-on volume to 30...");
    DFPlayer.set_power_on_volume(30);
    delay(700);

    byte track_num = 1;
    Serial.println("Looping track # " + String(track_num));
    DFPlayer.loop_track(track_num);
    delay(0);

    return true;
}