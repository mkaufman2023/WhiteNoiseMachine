/*************************************************************************
*                                                                        *
*   WebApp.h                                                             *
*   Async HTTP + WebSocket interface for the White Noise Machine.        *
*                                                                        *
*   The phone opens one WebSocket and does two things: it sends commands  *
*   and it renders whatever state the box pushes back. There is no        *
*   polling, and the UI never has to guess what the hardware is doing.    *
*                                                                        *
*   THREADING: ESPAsyncWebServer callbacks run on the async TCP task,     *
*   not on the Arduino loop() task. Touching Player from inside a         *
*   callback would race against Player::tick(). So callbacks only push    *
*   Actions onto a FreeRTOS queue, and loop() drains it. Everything that  *
*   mutates state therefore happens on exactly one task.                  *
*                                                                        *
*   Written by Matt Kaufman - December 5, 2025                           *
*   Rewritten for async + WebSocket - July 2026                          *
*                                                                        *
**************************************************************************/

#ifndef WEB_APP_H
#define WEB_APP_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class Player;
class Net;


class WebApp {
public:
    /** A command received from a phone, waiting to be applied on the loop task. */
    struct Action {
        enum class Kind : uint8_t {
            NONE, SELECT_TRACK, SET_VOLUME, PAUSE, RESUME, TOGGLE, STOP, FAVORITE
        };
        Kind    kind = Kind::NONE;
        uint8_t arg  = 0;
    };

    WebApp(Player& player, Net& net);

    /** Mounts the filesystem routes, starts the server and the WebSocket. */
    void begin();

    /** Housekeeping (stale socket cleanup) plus deferred broadcasts. */
    void tick();

    /** Pops one queued Action. Returns false when the queue is empty. */
    bool next_action(Action& out);

    /** Requests a state push to every connected phone (coalesced by tick()). */
    void request_broadcast();

    /** Appends a line to the diagnostics log and pushes it to subscribers. */
    void log(const String& msg);

    /** True if at least one phone currently has the diagnostics panel open. */
    bool log_subscribed() const { return _log_subs > 0; }


private:
    static constexpr uint16_t PORT                  = 80;
    static constexpr uint32_t MIN_BROADCAST_GAP_MS  = 40;  /**< Coalesce fade-step bursts */
    static constexpr uint8_t  ACTION_QUEUE_LEN      = 16;

    AsyncWebServer _server;
    AsyncWebSocket _ws;
    Player&        _player;
    Net&           _net;

    QueueHandle_t  _actions = nullptr;

    volatile bool  _broadcast_due = false;
    volatile int   _log_subs      = 0;
    uint32_t       _t_last_bcast  = 0;

    void setup_routes();
    void handle_ws_event(AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len);
    void handle_ws_message(AsyncWebSocketClient* client, const uint8_t* data, size_t len);
    void build_state_json(String& out);
    void broadcast_now();
};


#endif  // WEB_APP_H
