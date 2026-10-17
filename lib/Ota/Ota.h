/*************************************************************************
*                                                                        *
*   Ota.h                                                                *
*   Which firmware is running, and the probation after an update.        *
*                                                                        *
*   A firmware that just arrived over the air boots "on probation"       *
*   (ESP-IDF's PENDING_VERIFY state). It's kept for good only once it    *
*   has shown it can take the next update: Wi-Fi up, web server running, *
*   and SETTLE_MS without a crash. Until then:                           *
*     - a crash or any restart makes the bootloader boot the previous    *
*       firmware instead, by itself;                                     *
*     - if it still isn't healthy after DEADLINE_MS, tick() restarts,    *
*       which is the same thing.                                         *
*   A firmware flashed over USB is never on probation.                   *
*                                                                        *
*   Written October 2026                                                 *
*                                                                        *
**************************************************************************/

#ifndef OTA_H
#define OTA_H

#include <stdint.h>


namespace ota
{
    constexpr uint32_t SETTLE_MS   = 30 * 1000;       /**< Must run this long before it's confirmed */
    constexpr uint32_t DEADLINE_MS = 5 * 60 * 1000;   /**< Still not healthy by now: go back */

    /** Notes which firmware is running and whether it's on probation. Call once, early in setup(). */
    void begin();

    /** Confirms a firmware on probation once `healthy`, or rolls it back at the deadline. Call every loop(). */
    void tick(bool healthy);

    /** First 8 hex digits of the running firmware's ELF SHA-256. Changes with every build that changes the code. */
    const char* firmware_id();

    /** True if the most recent update failed its probation and the box went back to this firmware. */
    bool last_update_failed();
}


#endif  // OTA_H
