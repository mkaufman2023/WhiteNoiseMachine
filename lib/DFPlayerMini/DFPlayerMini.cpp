/******************************************************************
*                                                                 *
*    lib / DFPlayerMini / DFPlayerMini.cpp                        *
*    Created by Matt Kaufman, December 4, 2025.                   *
*                                                                 *
*    Implementation of the DFPlayerMini class                     *
*      designed to control the DFPlayer Mini MP3 module.          *
*                                                                 *
*******************************************************************/

#include "DFPlayerMini.h"


// Constants
static const int BOOT_DELAY_MS = 5000;              // Delay to allow DFPlayer to boot
static const unsigned long DFPLAYER_BAUD = 9600;    // DFPlayer default baud rate
static const int DFPLAYER_DEFAULT_TRACK = 1;        // Default track to play
static const int DFPLAYER_DEFAULT_VOLUME = 20;      // Default volume level



DFPlayerMini::DFPlayerMini(
    int esp32_rx = 20,
    int esp32_tx = 21,
    HardwareSerial& serial = Serial1
) : _esp32_rx(esp32_rx), _esp32_tx(esp32_tx), _serial(serial), _is_online(false) {}


bool DFPlayerMini::begin() {
    // Initialize serial
    _serial.begin(DFPLAYER_BAUD, SERIAL_8N1, _esp32_rx, _esp32_tx);

    // Wait for DFPlayer to boot
    delay(BOOT_DELAY_MS);

    // Initialize DFPlayer
    if (!_driver.begin(_serial)) {
        _is_online = false;
        return false;
    }

    // Set default volume and start playing default track
    _driver.volume(DFPLAYER_DEFAULT_VOLUME);
    _driver.loop(DFPLAYER_DEFAULT_TRACK);

    _is_online = true;
    return true;
}


void DFPlayerMini::set_volume(int volume) {
    if (_is_online) {
        int vol = constrain(volume, 0, 30);
        _driver.volume(vol);
    }
}


void DFPlayerMini::loop_track(int track) {
    if (_is_online && track > 0) {
        _driver.loop(track);
    }
}


void DFPlayerMini::stop_playing() {
    if (_is_online) {
        _driver.stop();
    }
}


bool DFPlayerMini::is_playing() {
    if (_is_online) {
        return _driver.readState() == 1;
    }
    return false;
}


bool DFPlayerMini::is_online() const {
    return _is_online;
}
