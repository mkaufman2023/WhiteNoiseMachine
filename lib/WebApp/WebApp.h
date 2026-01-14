/*************************************************************************
*                                                                        *
*   WebApp.h                                                             *
*   Library for handling static and dynamic routes and mDNS setup.       *
*                                                                        *
*   Written by Matt Kaufman - December 5, 2025                           *
*                                                                        *
**************************************************************************/


#ifndef WEB_APP_H
#define WEB_APP_H

#include <WebServer.h>

class DFPlayerMini;


class WebApp {
public:
    /** 
     * Constructor for WebApp class.
     * @param player Reference to the DFPlayerMini instance being used.
     * @param log_buffer Reference to the global log buffer string (terminal messages).
     */
    explicit WebApp(DFPlayerMini& player, String& log_buffer);

    /**
     * Initializes the web application.
     * - Sets up routes
     * - Starts the server
     * - Configures mDNS
     */
    void begin();

    /**
     * Handles incoming client requests.
     * Should be called repeatedly in the main loop.
     */
    void handle_client();

    /** 
     * Logs a blank message to the web log buffer (UI terminal).
     */
    void log();

    /** 
     * Logs a message to the web log buffer (UI terminal).
     * @param msg The message to log.
     */
    void log(String msg);

    bool mDNS_is_setup = false; /**< Flag indicating if mDNS setup was successful */


private:
    WebServer _server;          /**< Web server instance for handling HTTP requests */
    String& _log_buffer;        /**< Reference to the global log buffer string (terminal messages) */
    DFPlayerMini& _player;      /**< Reference to the DFPlayerMini instance used in main */

    /** 
     * Sets up the mDNS responder (skips if WiFi is not connected).
     */
    bool setup_mdns();

    /** 
     * Configures the HTTP routes for the web application.
     * - Serves static `index.html` at root (`/`)
     * - Serves static files for PWA support (icons, manifest)
     * - Sets up dynamic endpoints for `/play`, `/status`, etc.
     */
    void setup_routes();
    

    /* ↓↓↓↓↓ DYNAMIC ENDPOINTS ↓↓↓↓↓ */


    /** 
     * Private handler for the `/log` endpoint.
     * Returns the current log buffer contents.
     */
    void handle_log();

    /** 
     * Private handler for unknown routes (404 Not Found).
     */
    void handle_not_found();

    /** 
     * Private handler for the `/play` endpoint.
     * Expects a `track` query parameter to specify which track to loop.
     */
    void handle_play();
    
    /** 
     * Private handler for the `/status` endpoint.
     * Returns `1` if the DFPlayer is online, `0` otherwise.
     */
    void handle_status();

    /** 
     * Private handler for the `/previous` endpoint.
     * Commands the DFPlayer to play the previous track.
     */
    void handle_previous();

    /** 
     * Private handler for the `/pause` endpoint.
     * Commands the DFPlayer to pause playback.
     */
    void handle_pause();

    /** 
     * Private handler for the `/resume` endpoint.
     * Commands the DFPlayer to resume playback.
     */
    void handle_resume();

    /** 
     * Private handler for the `/next` endpoint.
     * Commands the DFPlayer to play the next track.
     */
    void handle_next();

    /** 
     * Private handler for the `/volume_down` endpoint.
     * Commands the DFPlayer to decrement the volume.
     */
    void handle_volume_down();

    /** 
     * Private handler for the `/stop` endpoint.
     * Commands the DFPlayer to stop all playback.
     */
    void handle_stop();

    /** 
     * Private handler for the `/volume_up` endpoint.
     * Commands the DFPlayer to increment the volume.
     */
    void handle_volume_up();

    /** 
     * Private handler for the `/start_repeat` endpoint.
     * Commands the DFPlayer to start looping the current track.
     */
    void handle_start_repeat();

    /** 
     * Private handler for the `/stop_repeat` endpoint.
     * Commands the DFPlayer to stop looping the current track.
     */
    void handle_stop_repeat();

    /** 
     * Private handler for the `/set_eq_normal` endpoint.
     * Sets the DFPlayer EQ to `Normal` mode.
     */
    void handle_set_EQ_normal();

    /** 
     * Private handler for the `/set_eq_rock` endpoint.
     * Sets the DFPlayer EQ to `Rock` mode.
     */
    void handle_set_EQ_rock();

    /** 
     * Private handler for the `/set_eq_pop` endpoint.
     * Sets the DFPlayer EQ to `Pop` mode.
     */
    void handle_set_EQ_pop();
};


#endif  // WEB_APP_H