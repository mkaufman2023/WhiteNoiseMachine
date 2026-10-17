/*************************************************************************
*                                                                        *
*   Logbook.h                                                            *
*   The box's diagnostics log.                                           *
*                                                                        *
*   Every line goes to the USB serial monitor, and the last CAPACITY     *
*   lines are also kept in memory, numbered, so the app's diagnostics    *
*   panel can show recent history (boot included) and then stream new    *
*   lines as they arrive. Safe to call from any task.                    *
*                                                                        *
*   Written by Matt Kaufman - October 2026                               *
*                                                                        *
**************************************************************************/

#ifndef LOGBOOK_H
#define LOGBOOK_H

#include <stdint.h>
#include <string>


namespace logbook
{
    constexpr uint8_t CAPACITY    = 40;   /**< Lines kept in memory */
    constexpr size_t  LINE_LENGTH = 96;   /**< Longer lines are cut short */

    /** Adds a line, printf-style. A timestamp ("[12.345] ") is prepended. */
    void add(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

    /** Number of the newest line (lines are numbered from 1; 0 = none yet). */
    uint32_t newest();

    /**
     * Appends the kept lines numbered after `after` and up to `upto` to `out`,
     * one per line, oldest first. Lines that have already scrolled out of
     * memory are skipped.
     */
    void copy(uint32_t after, uint32_t upto, std::string& out);
}


#endif  // LOGBOOK_H
