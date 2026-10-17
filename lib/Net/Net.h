/*************************************************************************
*                                                                        *
*   Net.h                                                                *
*   Non-blocking Wi-Fi connection manager with mDNS re-advertisement.    *
*                                                                        *
*   Connects to the one home network and keeps it that way: the core's  *
*   auto-reconnect handles routine drops, and tick() restarts the        *
*   connection if a drop lasts long enough that auto-reconnect has       *
*   evidently given up (it doesn't retry after every disconnect reason). *
*                                                                        *
*   Written by Matt Kaufman - July 2026                                  *
*                                                                        *
**************************************************************************/

#ifndef NET_H
#define NET_H

#include <Arduino.h>


class Net {
public:
    /**
     * @param ssid      Network name.
     * @param password  Network password.
     * @param hostname  mDNS/DHCP hostname (e.g. "whitenoise").
     */
    Net(const char* ssid, const char* password, const char* hostname);

    /** Starts connecting. Does not block. */
    void begin();

    /** Watches the link and restarts it if it stays down. Call every loop() iteration. */
    void tick();

    bool    connected() const;
    bool    mdns_up()   const { return _mdns_up; }
    int8_t  rssi()      const;
    String  ip()        const;

    /** True once after the link comes up or goes down, then clears. */
    bool consume_link_change();


private:
    static constexpr uint32_t RESTART_AFTER_MS = 30000;  /**< Down this long -> start the connection over */

    const char* _ssid;
    const char* _password;
    const char* _hostname;

    bool     _up           = false;
    bool     _mdns_up      = false;
    bool     _link_changed = false;
    uint32_t _t_down       = 0;      /**< When the link went down (or the last restart) */

    void connect();
    void on_up();
    void on_down();
    void start_mdns();
};


#endif  // NET_H
