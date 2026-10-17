/*************************************************************************
*                                                                        *
*   Net.h                                                                *
*   Non-blocking Wi-Fi connection manager with mDNS re-advertisement.    *
*                                                                        *
*   Replaces the old one-shot connect_to_wifi(), which ran once in       *
*   setup() and never again -- meaning a router reboot left the box       *
*   unreachable until someone physically unplugged it.                    *
*                                                                        *
*   Written by Matt Kaufman - July 2026                                  *
*                                                                        *
**************************************************************************/

#ifndef NET_H
#define NET_H

#include <Arduino.h>


class Net {
public:
    struct Credentials {
        const char* ssid;
        const char* password;
    };

    /**
     * @param networks  Array of networks to try, in order.
     * @param count     Number of entries in `networks`.
     * @param hostname  mDNS/DHCP hostname (e.g. "whitenoise").
     */
    Net(const Credentials* networks, uint8_t count, const char* hostname);

    /** Starts the first connection attempt. Does not block. */
    void begin();

    /** Drives the connection state machine. Call every loop() iteration. */
    void tick();

    bool    connected() const;
    bool    mdns_up()   const { return _mdns_up; }
    int8_t  rssi()      const;
    String  ip()        const;
    const char* ssid()  const;

    /** True once after the link comes up or goes down, then clears. */
    bool consume_link_change();


private:
    static constexpr uint32_t ATTEMPT_TIMEOUT_MS = 8000;   /**< Per-network attempt */
    static constexpr uint32_t RETRY_BACKOFF_MS   = 5000;   /**< Pause after a full sweep */

    enum class St : uint8_t { IDLE, CONNECTING, ONLINE, BACKOFF };

    const Credentials* _nets;
    uint8_t            _count;
    const char*        _hostname;

    St       _st            = St::IDLE;
    uint8_t  _index         = 0;
    uint32_t _t_state       = 0;
    bool     _mdns_up       = false;
    bool     _link_changed  = false;

    void start_attempt(uint8_t index);
    void on_connected();
    void on_lost();
    void start_mdns();
};


#endif  // NET_H
