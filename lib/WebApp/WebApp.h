/*************************************************************************
*                                                                        *
*   WebApp.h                                                             *
*   HTTPS + secure WebSocket interface for the White Noise Machine.      *
*                                                                        *
*   The phone opens one WebSocket and does two things: it sends commands  *
*   and it renders whatever state the box pushes back. There is no        *
*   polling, and the UI never has to guess what the hardware is doing.    *
*                                                                        *
*   Everything is served over HTTPS (port 443) using ESP-IDF's built-in   *
*   server, because phones only run the app's service worker on a secure  *
*   page. Port 80 just redirects there. The web files themselves are      *
*   built into the firmware (see board_build.embed_files), so the UI and  *
*   the firmware can never be out of step.                                *
*                                                                        *
*   THREADING: the server's handlers run on its own task, not on the      *
*   Arduino loop() task. Touching Player from a handler would race        *
*   against Player::tick(). So handlers only push Actions onto a FreeRTOS *
*   queue, which loop() drains; and /api/state reads a JSON snapshot      *
*   that loop() keeps up to date. Everything that touches Player happens  *
*   on exactly one task.                                                  *
*                                                                        *
*   Written by Matt Kaufman - December 5, 2025                           *
*   Rewritten for async + WebSocket - July 2026                          *
*   Moved to HTTPS - October 2026                                        *
*                                                                        *
**************************************************************************/

#ifndef WEB_APP_H
#define WEB_APP_H

#include <Arduino.h>
#include <atomic>
#include <mutex>
#include <string>
#include <esp_http_server.h>
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

    /** Starts the HTTPS server (UI, WebSocket, /api/state) and the port-80 redirect. */
    void begin();

    /** Sends deferred state broadcasts and new log lines. Call every loop() iteration. */
    void tick();

    /** Pops one queued Action. Returns false when the queue is empty. */
    bool next_action(Action& out);

    /** Requests a state push to every connected phone (coalesced by tick()). */
    void request_broadcast();


private:
    static constexpr uint32_t MIN_BROADCAST_GAP_MS = 40;   /**< Coalesce fade-step bursts */
    static constexpr uint8_t  ACTION_QUEUE_LEN     = 16;
    static constexpr uint8_t  MAX_SOCKETS          = 6;    /**< Open HTTPS connections (each costs a TLS session) */
    static constexpr size_t   MAX_WS_MESSAGE       = 128;  /**< Commands are tiny; anything bigger is refused */

    Player& _player;
    Net&    _net;

    httpd_handle_t _https    = nullptr;
    httpd_handle_t _redirect = nullptr;
    QueueHandle_t  _actions  = nullptr;

    std::atomic<bool>     _broadcast_due { false };
    uint32_t              _t_last_bcast = 0;
    std::atomic<uint32_t> _log_sent { 0 };   /**< Newest log line already pushed to subscribers */

    std::mutex _state_mutex;
    String     _state_json;   /**< Latest state, built on the loop task; read by /api/state */

    bool start_https();
    bool start_redirect();
    void broadcast_now();
    void push_new_log_lines();
    void queue_send(std::string text, bool log_only);
    void build_state_json(String& out);
    void handle_ws_message(httpd_req_t* req, const uint8_t* data, size_t len);
    void subscribe_to_log(httpd_req_t* req);

    /* Server handlers. Static, because ESP-IDF calls plain functions; `this`
     * arrives through each handler's user_ctx. */
    static esp_err_t on_asset(httpd_req_t* req);
    static esp_err_t on_ws(httpd_req_t* req);
    static esp_err_t on_state(httpd_req_t* req);
    static esp_err_t on_redirect(httpd_req_t* req);
    static void      send_to_all(void* arg);
};


#endif  // WEB_APP_H
