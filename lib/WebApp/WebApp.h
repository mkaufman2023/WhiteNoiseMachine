/******************************************************************
*                                                                 *
*    lib / WebApp / WebApp.h                                      *
*    Created by Matt Kaufman, December 5, 2025.                   *
*                                                                 *
*    Declaration of the WebApp class designed to                  *
*      handle routes, play/status handlers, and mDNS setup.       *
*                                                                 *
*******************************************************************/

#ifndef WEB_APP_H
#define WEB_APP_H

#include <WebServer.h>

class DFPlayerMini;


class WebApp {
public:
    /** 
     * Constructor for WebApp class.
     * @param player Reference to the DFPlayerMini instance being used.
     */
    explicit WebApp(DFPlayerMini& player);

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

private:
    WebServer _server;          /**< Web server instance for handling HTTP requests */
    DFPlayerMini& _player;      /**< Reference to the DFPlayerMini instance used in main */

    /** 
     * Private method that sets up the mDNS responder (skips if WiFi is not connected).
     */
    void setup_mdns();

    /** 
     * Private method that configures the HTTP routes for the web application.
     * - Serves static `index.html` at root (`/`)
     * - Serves static files for PWA support (icons, manifest)
     * - Sets up dynamic endpoints for `/play` and `/status`
     */
    void setup_routes();
    
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
};


#endif  // WEB_APP_H