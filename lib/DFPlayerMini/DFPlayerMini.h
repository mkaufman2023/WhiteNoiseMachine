/******************************************************************
*                                                                 *
*    lib / DFPlayerMini / DFPlayerMini.h                          *
*    Created by Matt Kaufman, December 4, 2025.                   *
*                                                                 *
*    Declaration of the DFPlayerMini class                        *
*      designed to control the DFPlayer Mini MP3 module.          *
*                                                                 *
*******************************************************************/

#ifndef DF_PLAYER_MINI_H
#define DF_PLAYER_MINI_H

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>


class DFPlayerMini {
public:
    /**
     * Constructor for DFPlayerMini class.
     * @param esp32_rx The ESP32 pin connected to DFPlayer TX (default `20`)
     * @param esp32_tx The ESP32 pin connected to DFPlayer RX (default `21`)
     * @param serial The hardware serial port to use (default `Serial1`)
     */
    DFPlayerMini(int esp32_rx = 20, int esp32_tx = 21, HardwareSerial& serial = Serial1);
    
    /**
     * Initializes the Serial connection and the DFPlayer hardware.
     * @return `true` if communication succeeded, `false` otherwise.
     */
    bool begin();

    /**
     * Sets the volume (range from 0-30).
     * @param volume The desired volume level.
     */
    void set_volume(int volume);

    /**
     * Loops a specific track number indefinitely.
     * @param track The track number to loop.
     */
    void loop_track(int track);

    /**
     * Stops playback.
     */
    void stop_playing();

    /**
     * Checks if the DFPlayer is currently playing audio.
     * @return `true` if playing, `false` otherwise.
     */
    bool is_playing();

    /**
     * Checks if the DFPlayer has been successfully initialized.
     * @return `true` if initialized, `false` otherwise.
     */
    bool is_online() const;

private:
    int _esp32_rx;                  /**< ESP32 pin connected to DFPlayer TX */
    int _esp32_tx;                  /**< ESP32 pin connected to DFPlayer RX */
    bool _is_online;                /**< Flag indicating if DFPlayer initialized successfully */
    HardwareSerial& _serial;        /**< Reference to the hardware serial port used */
    DFRobotDFPlayerMini _driver;    /**< DFRobotDFPlayerMini instance as the driver */
};


#endif  // DF_PLAYER_MINI_H