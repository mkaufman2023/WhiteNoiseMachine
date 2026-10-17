/*
 * Host-side stand-in for the ESP32 Preferences (NVS) library.
 *
 * Every Preferences instance reads and writes one shared in-memory store,
 * g_nvs, so a test can save from one Player and restore into a fresh one --
 * the same thing a reboot does on the device. Namespaces are ignored; the
 * firmware only uses one.
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>

struct NvsStore {
    std::map<std::string, uint32_t> u;   // every value type stored widened
};

extern NvsStore g_nvs;   // defined by the test


class Preferences {
public:
    bool begin(const char* /*name*/, bool read_only = false) { _ro = read_only; return true; }
    void end() { }

    uint8_t  getUChar(const char* key, uint8_t def = 0)  const { return (uint8_t)get(key, def); }
    bool     getBool (const char* key, bool def = false) const { return get(key, def) != 0; }
    uint32_t getULong(const char* key, uint32_t def = 0) const { return get(key, def); }

    size_t putUChar(const char* key, uint8_t v)  { return put(key, v, 1); }
    size_t putBool (const char* key, bool v)     { return put(key, v, 1); }
    size_t putULong(const char* key, uint32_t v) { return put(key, v, 4); }

private:
    bool _ro = false;

    uint32_t get(const char* key, uint32_t def) const {
        auto it = g_nvs.u.find(key);
        return it == g_nvs.u.end() ? def : it->second;
    }

    size_t put(const char* key, uint32_t v, size_t width) {
        if (_ro) return 0;   // the real library refuses writes on a read-only handle
        g_nvs.u[key] = v;
        return width;
    }
};
