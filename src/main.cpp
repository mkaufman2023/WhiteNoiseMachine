/*************************************************************************
*                                                                        *
*   main.cpp - White Noise Machine                                       *
*                                                                        *
*   Nothing in setup() or loop() blocks. The box boots straight into its  *
*   main loop and serves the UI while the audio module settles and Wi-Fi  *
*   negotiates, so the app is never left hanging on a request.            *
*                                                                        *
*   Data flow:                                                            *
*       phone --WS--> WebApp queues Action --> loop() applies to Player   *
*       Player state changes --> loop() requests broadcast --WS--> phone  *
*                                                                        *
*   Written by Matt Kaufman - December 2025                              *
*   Rewritten for async + WebSocket - July 2026                          *
*                                                                        *
**************************************************************************/

#include <Arduino.h>
#include <LittleFS.h>

#include <config/secrets.h>
#include <DFPlayerMini.h>
#include <Player.h>
#include <Net.h>
#include <WebApp.h>


// MCU pins wired to the DFPlayer clone (see docs/images/schematic.png)
static constexpr uint8_t MCU_RX = D7;   // XIAO pin going TO module TX
static constexpr uint8_t MCU_TX = D6;   // XIAO pin going TO module RX

static constexpr char HOSTNAME[] = "whitenoise";


// Credentials live in include/config/secrets.h, which is gitignored --
// never inline them here.
DFPlayerMini df_player(MCU_RX, MCU_TX);
Player       player(df_player);
Net          wifi_net(secrets::wifi_ssid, secrets::wifi_password, HOSTNAME);
WebApp       web_app(player, wifi_net);


static void apply(const WebApp::Action& a)
{
    using K = WebApp::Action::Kind;

    switch (a.kind) {
        case K::SELECT_TRACK: player.select_track(a.arg);    break;
        case K::SET_VOLUME:   player.set_volume(a.arg);      break;
        case K::PAUSE:        player.pause();                break;
        case K::RESUME:       player.resume();               break;
        case K::TOGGLE:       player.toggle();               break;
        case K::STOP:         player.stop();                 break;
        case K::FAVORITE:     player.toggle_favorite(a.arg); break;
        case K::NONE:
        default:                                             break;
    }
}


void setup()
{
    Serial.begin(115200);
    Serial.println("\n--- White Noise Machine booting ---");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed -- the UI will not be served.");
    }

    player.begin();     // starts UART, restores saved track/volume/favorites
    wifi_net.begin();   // starts connecting; does not wait for it
    web_app.begin();    // server is up immediately, even before Wi-Fi lands

    Serial.println("--- Boot complete (audio module warming up) ---");
}


void loop()
{
    wifi_net.tick();
    player.tick();
    web_app.tick();

    // Apply everything the phones asked for since the last pass. Doing this
    // here (rather than in the WebSocket callback) keeps all state mutation on
    // a single task -- see the threading note in WebApp.h.
    WebApp::Action action;
    while (web_app.next_action(action)) {
        apply(action);
    }

    // Push state whenever the box's own view of reality changed.
    if (player.consume_dirty() || wifi_net.consume_link_change()) {
        web_app.request_broadcast();
    }

    delay(1);   // yield to the async TCP task and the idle/watchdog task
}
