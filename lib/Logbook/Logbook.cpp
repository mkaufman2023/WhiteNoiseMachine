/*************************************************************************
*                                                                        *
*   Logbook.cpp                                                          *
*   The box's diagnostics log.                                           *
*                                                                        *
*   Written by Matt Kaufman - October 2026                               *
*                                                                        *
**************************************************************************/

#include "Logbook.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>


namespace logbook
{
    namespace
    {
        struct Line {
            uint32_t number;
            char     text[LINE_LENGTH];
        };

        Line       lines[CAPACITY] = {};   // a ring: line N lives in slot N % CAPACITY
        uint32_t   next_number     = 1;
        std::mutex mutex;
    }


    void add(const char* fmt, ...)
    {
        char text[LINE_LENGTH];

        const uint32_t ms = millis();
        int n = snprintf(text, sizeof(text), "[%lu.%03lu] ",
                         (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));

        va_list args;
        va_start(args, fmt);
        vsnprintf(text + n, sizeof(text) - n, fmt, args);
        va_end(args);

        Serial.println(text);

        std::lock_guard<std::mutex> lock(mutex);
        Line& line  = lines[next_number % CAPACITY];
        line.number = next_number++;
        memcpy(line.text, text, sizeof(text));
    }


    uint32_t newest()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return next_number - 1;
    }


    void copy(uint32_t after, uint32_t upto, std::string& out)
    {
        std::lock_guard<std::mutex> lock(mutex);

        const uint32_t newest_number = next_number - 1;
        if (upto > newest_number) upto = newest_number;

        // Only the newest CAPACITY lines are still in memory.
        const uint32_t oldest_kept = (newest_number > CAPACITY) ? newest_number - CAPACITY + 1 : 1;
        uint32_t number = (after + 1 > oldest_kept) ? after + 1 : oldest_kept;

        for (; number <= upto; number++) {
            const Line& line = lines[number % CAPACITY];
            if (line.number != number) continue;   // overwritten since; skip

            if (!out.empty()) out += '\n';
            out += line.text;
        }
    }
}
