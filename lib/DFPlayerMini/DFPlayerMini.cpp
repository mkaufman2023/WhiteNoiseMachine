/****************************************************************************************
*                                                                                       *
*   DFPlayerMini.cpp                                                                    *
*   Library for TD5580A-based DFPlayer Mini clones                                      *
*                                                                                       *
*   Adapted from https://github.com/keith721/TD5580A                                    *
*   Adapted for HiLetgo's clone (see: 2) by Matt Kaufman, December, 2025.               *
*                                                                                       *
*   See:                                                                                *
*     1. http://www.tudasemi.com/static/upload/file/20240905/1725499313437991.pdf       *
*     2. https://www.amazon.com/HiLetgo-Arduino-Player-Module-DFPlayer/dp/B01D1D0E7Q    *
*                                                                                       *
*****************************************************************************************/

#include <Arduino.h>
#include "Commands.h"
#include "DFPlayerMini.h"


/**
 * Clamps an integer to the specified byte range.
 * @param value The integer value to clamp.
 * @param low The lower bound.
 * @param high The upper bound.
 * @return The clamped integer, with type `byte`.
 **/
static byte clamp_u8(int value, int low, int high)
{
    if (value < low) {
        value = low;
    }
    if (value > high) {
        value = high;
    }
    return static_cast<byte>(value);
}



/**
 * Constructor for `DFPlayerMini` class.
 * @param mcu_rx The MCU pin connected to DFPlayer Mini TX - default: `D7`/`20`.
 * @param mcu_tx The MCU pin connected to DFPlayer Mini RX - default: `D6`/`21`.
 **/
DFPlayerMini::DFPlayerMini(
    int mcu_rx,
    int mcu_tx
) : _mcu_rx(mcu_rx), _mcu_tx(mcu_tx) {}



/**
 * Initializes the MCU ⟷ DFPlayer Mini connection.
 * @param debug Show serial debug messages - default: `false`.
 **/
void DFPlayerMini::begin(bool debug)
{
    _show_debug_messages = debug;

    // Initialize serial connection
    Serial1.begin(9600, SERIAL_8N1, _mcu_rx, _mcu_tx);
    _serial = &Serial1;

    // NOTE: The former delay(2500) here (the module's power-on settling time)
    // is now handled non-blockingly by Player::tick(), which refuses to
    // transmit until WARMUP_MS has elapsed since begin(). The device now boots
    // straight into its main loop and serves the UI while the module settles.
    
    if (_show_debug_messages) {
        Serial.println("DFPlayerMini: Serial connection initialized.");
    }
}



/**
 * Reads whatever the module has sent, without blocking.
 * @param out Filled with the frame when one completes.
 * @return `true` each time a complete frame arrives (call it in a loop).
 **/
bool DFPlayerMini::poll(dfplayer::Frame& out)
{
    if (_serial == nullptr) {
        return false;
    }

    while (_serial -> available()) {
        if (_parser.feed((uint8_t)_serial -> read(), out)) {
            if (_show_debug_messages) {
                Serial.println("Received: " + _sbyte2hex(out.cmd) + _sbyte2hex(out.data1) + _sbyte2hex(out.data2));
            }
            return true;
        }
    }

    return false;
}



/**
 * [ `0x01` ]
 * 
 * Plays the next track.
 **/
void DFPlayerMini::play_next()
{
    _send_command(dfplayer::cmd::NEXT);
}



/**
 * [ `0x02` ]
 * 
 * Plays the previous track.
 **/
void DFPlayerMini::play_previous()
{
    _send_command(dfplayer::cmd::PREVIOUS);
}



/**
 * [ `0x03` ]
 * 
 * Plays a specific track.
 * @param track The track number to play.
 **/
void DFPlayerMini::play_track(int track)
{
    _send_command(dfplayer::cmd::PLAY_N, clamp_u8(track, 1, 255));
}



/**
 * ```
 * 0x03
 * ```
 * 
 * Plays a specific track.
 * @param track The track number to play.
 **/
void DFPlayerMini::play_track(byte track)
{
    _send_command(dfplayer::cmd::PLAY_N, track);
}



/**
 * ```
 * 0x0F
 * ```
 * 
 * Plays a specific track in a specific folder.
 * @param folder The folder number.
 * @param track The track number to play.
 **/
void DFPlayerMini::play_track_in_folder(int folder, int track)
{
    _send_command(dfplayer::cmd::PLAY_F_FILE, clamp_u8(folder, 1, 99), clamp_u8(track, 1, 255));
}



/**
 * `0x0F`
 * 
 * Plays a specific track in a specific folder.
 * @param folder The folder number.
 * @param track The track number to play.
 **/
void DFPlayerMini::play_track_in_folder(byte folder, byte track)
{
    _send_command(dfplayer::cmd::PLAY_F_FILE, folder, track);
}



/**
 * `0x08`
 * 
 * Loops a specific track indefinitely.
 * @param track The track number to loop.
 **/
void DFPlayerMini::loop_track(int track)
{
    _send_command(dfplayer::cmd::PLAY_S_LOOP, clamp_u8(track, 1, 255));
}



/**
 * `0x08`
 * 
 * Loops a specific track indefinitely.
 * @param track The track number to loop.
 **/
void DFPlayerMini::loop_track(byte track)
{
    _send_command(dfplayer::cmd::PLAY_S_LOOP, track);
}



/**
 * `0x17`
 * 
 * Loops all tracks in a specific folder indefinitely.
 * @param folder The folder number.
 **/
void DFPlayerMini::loop_folder(int folder)
{
    _send_command(dfplayer::cmd::FOLDER_CYCLE, clamp_u8(folder, 1, 99), 0);
}



/**
 * `0x17`
 * 
 * Loops all tracks in a specific folder indefinitely.
 * @param folder The folder number.
 **/
void DFPlayerMini::loop_folder(byte folder)
{
    _send_command(dfplayer::cmd::FOLDER_CYCLE, folder, 0);
}



/**
 * `0x11`
 * 
 * Loops all tracks indefinitely.
 **/
void DFPlayerMini::loop_all_tracks()
{
    _send_command(dfplayer::cmd::PLAY_LOOPS);
}



/**
 * `0x18`
 * 
 * Plays all tracks in random order.
 **/
void DFPlayerMini::shuffle_all_tracks()
{
    _send_command(dfplayer::cmd::PLAY_SHUFFLE);
}



/**
 * `0x19`
 * 
 * Starts looping the current track indefinitely (data `0` = loop on).
 **/
void DFPlayerMini::start_looping_current_track()
{
    _send_command(dfplayer::cmd::SET_SPLAY, 0, 0);
}



/**
 * `0x19`
 *
 * Stops looping the current track (data `1` = loop off).
 **/
void DFPlayerMini::stop_looping_current_track()
{
    _send_command(dfplayer::cmd::SET_SPLAY, 0, 1);
}



/**
 * `0x12`
 * 
 * Specifies the MP3 folder.
 * @param folder The folder number.
 **/
void DFPlayerMini::set_folder(int folder)
{
    _send_command(dfplayer::cmd::SET_FOLDER, clamp_u8(folder, 1, 99));
}



/**
 * `0x12`
 * 
 * Specifies the MP3 folder.
 * @param folder The folder number.
 **/
void DFPlayerMini::set_folder(byte folder)
{
    _send_command(dfplayer::cmd::SET_FOLDER, folder);
}



/**
 * `0x09`
 * 
 * Specifies the playback source.
 * @param source The source to set (`1`: USB Disk, `2`: SD Card, `3`: Aux, `4`: Flash, `5`: PC, `6`: Sleep). 
 **/
void DFPlayerMini::set_source(int source)
{
    _send_command(dfplayer::cmd::SET_SOURCE, clamp_u8(source, 1, 6));
}



/**
 * `0x09`
 * 
 * Specifies the playback source.
 * @param source The source to set (`1`: USB Disk, `2`: SD Card, `3`: Aux, `4`: Flash, `5`: PC, `6`: Sleep). 
 **/
void DFPlayerMini::set_source(byte source)
{
    _send_command(dfplayer::cmd::SET_SOURCE, source);
}



/**
 * `0x04`
 * 
 * Increases the volume by one step.
 **/
void DFPlayerMini::increment_volume()
{
    _send_command(dfplayer::cmd::VOL_UP);
}



/**
 * `0x05`
 * 
 * Decreases the volume by one step.
 **/
void DFPlayerMini::decrement_volume()
{
    _send_command(dfplayer::cmd::VOL_DOWN);
}



/**
 * `0x06`
 * 
 * Sets the volume (range from 0-30).
 * @param volume The desired volume level.
 **/
void DFPlayerMini::set_volume(int volume)
{
    _send_command(dfplayer::cmd::SET_VOL, clamp_u8(volume, 0, 30));
}



/**
 * `0x06`
 * 
 * Sets the volume (range from 0-30).
 * @param volume The desired volume level.
 **/
void DFPlayerMini::set_volume(byte volume)
{
    _send_command(dfplayer::cmd::SET_VOL, volume);
}



/**
 * `0x1B`
 * 
 * Sets the power-on volume level (range from 0-30).
 * @param volume The desired volume level (`24` seems to be practical maximum).
 **/
void DFPlayerMini::set_power_on_volume(int volume)
{
    _send_command(dfplayer::cmd::PWR_ON_VOL_MEM, clamp_u8(volume, 0, 30));
}



/**
 * `0x1B`
 * 
 * Sets the power-on volume level (range from 0-30).
 * @param volume The desired volume level (`24` seems to be practical maximum).
 **/
void DFPlayerMini::set_power_on_volume(byte volume)
{
    _send_command(dfplayer::cmd::PWR_ON_VOL_MEM, volume);
}



/**
 * `0x07`
 * 
 * Sets the equalization.
 * @param eq The desired EQ (`0`: Normal, `1`: Rock, `2`: ???, `3`: Pop, `4`: Classical, `5`: Country, `6`: Jazz).
 **/
void DFPlayerMini::set_EQ(int eq)
{
    _send_command(dfplayer::cmd::SET_EQ, clamp_u8(eq, 0, 6));
}



/**
 * `0x07`
 * 
 * Sets the equalization.
 * @param eq The desired EQ (`0`: Normal, `1`: Rock, `2`: ???, `3`: Pop, `4`: Classical, `5`: Country, `6`: Jazz).
 **/
void DFPlayerMini::set_EQ(byte eq)
{
    _send_command(dfplayer::cmd::SET_EQ, eq);
}



/**
 * `0x0D`
 * 
 * Plays/resumes playback.
 **/
void DFPlayerMini::play()
{
    _send_command(dfplayer::cmd::PLAY);
}



/**
 * `0x0E`
 * 
 * Pauses playback.
 **/
void DFPlayerMini::pause()
{
    _send_command(dfplayer::cmd::PAUSE);
}



/**
 * `0x16`
 * 
 * Stops all playback actions.
 **/
void DFPlayerMini::stop_all_playback()
{
    _send_command(dfplayer::cmd::STOP_PLAY);
}



/**
 * `0x0C`
 * 
 * Resets the player.
 **/
void DFPlayerMini::reset()
{
    _send_command(dfplayer::cmd::RESET);
}



/**
 * `0x1A`
 * 
 * Enables the DAC (disables DAC high impedance).
 **/
void DFPlayerMini::enable_DAC()
{
    _send_command(dfplayer::cmd::DAC_IMP_HIGH, 0);
}



/**
 * `0x1A`
 * 
 * Disables the DAC (enables DAC high impedance).
 **/
void DFPlayerMini::disable_DAC()
{
    _send_command(dfplayer::cmd::DAC_IMP_HIGH, 1);
}



/**
 * `0x0A`
 * 
 * Puts the player in low-power (sleep) mode.
 **/
void DFPlayerMini::sleep()
{
    _send_command(dfplayer::cmd::SLEEP_MODE);
}



/**
 * `0x0B`
 * 
 * Wakes the player from low-power (sleep) mode.
 **/
void DFPlayerMini::wakeup()
{
    _send_command(dfplayer::cmd::WAKE_UP);
}



/**
 * Sends a one-byte command with no data (zero-fills both data bytes).
 * @param command The command byte.
 **/
void DFPlayerMini::_send_command(byte command)
{
    _send_command(command, 0, 0);
}



/**
 * Sends a one-byte command with one byte of data (zero-fills the first data byte).
 * @param command The command byte.
 * @param data2 The second data byte.
 **/
void DFPlayerMini::_send_command(byte command, byte data2)
{
    _send_command(command, 0, data2);
}



/**
 * Sends a one-byte command with two bytes of data.
 * @param command The command byte.
 * @param data1 The first data byte.
 * @param data2 The second data byte.
 **/
void DFPlayerMini::_send_command(byte command, byte data1, byte data2)
{
    if (_serial == nullptr) {
        if (_show_debug_messages) {
            Serial.println("DFPlayerMini: _send_command called before begin()");
        }
        return;
    }

    byte send_buf[8] = {0};    // Initialize data bytes buffer

    // Command Structure HEAD ADDR LEN  CMD ACK DATA CKL CKH END
    // Command Structure 0x7E 0xFF 0x06 CMD ACK DATA CK1 CK2 0xEF

    // NOTE: This function is NON-BLOCKING. Spacing between commands, waiting
    // for the module's reply and resending lost commands are all handled by
    // Player::tick(), which never blocks. Do not call this directly -- go
    // through Player.

    send_buf[0] = 0x7E;     // HEAD byte constant
    send_buf[1] = 0xFF;     // ADDR byte constant
    send_buf[2] = 0x06;     // LEN  length excluding HEAD, END, CHECKSUM, and LENGTH bytes
    send_buf[3] = command;  // CMD
    send_buf[4] = 0x01;     // ACK  feedback requested: the module replies 0x41 ("got it")
    send_buf[5] = data1;    // DATA data byte 1
    send_buf[6] = data2;    // DATA data byte 2
    send_buf[7] = 0xEF;     // END  byte constant

    _serial -> write(send_buf, sizeof(send_buf));

    if (_show_debug_messages) {
        String send_str = "";
        for (byte b : send_buf) {
            send_str += _sbyte2hex(b);
        }
        Serial.print("Sending: ");
        Serial.println(send_str);     // Display hex bytes sent to DFPlayer
    }
}



/**
 * Converts a byte to a hexadecimal string representation.
 * @param b The byte to convert.
 * @return The hexadecimal string representation of the byte.
 **/
String DFPlayerMini::_sbyte2hex(byte b)
{
    String shex;

    shex = "0X";
    if (b < 16) {
        shex += "0";
    }
    shex += String(b, HEX);
    shex += " ";
    return shex;
}