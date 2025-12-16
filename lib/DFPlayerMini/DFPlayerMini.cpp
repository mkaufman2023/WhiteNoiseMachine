/****************************************************************************************
*                                                                                       *
*   DFPlayerMini.cpp - Library for TD5580A-based DFPlayer Mini clones                   *
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
    
    if (_show_debug_messages) {
        Serial.println("DFPlayerMini: Serial connection initialized.");
    }
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
 * `0x08`
 * 
 * Loops a specific track in a specific folder indefinitely.
 * @param folder The folder number.
 * @param track The track number to loop.
 **/
void DFPlayerMini::loop_track_in_folder(int folder, int track)
{
    _send_command(dfplayer::cmd::PLAY_S_LOOP, clamp_u8(folder, 1, 99), clamp_u8(track, 1, 255));
}



/**
 * `0x08`
 * 
 * Loops a specific track in a specific folder indefinitely.
 * @param folder The folder number.
 * @param track The track number to loop.
 **/
void DFPlayerMini::loop_track_in_folder(byte folder, byte track)
{
    _send_command(dfplayer::cmd::PLAY_S_LOOP, folder, track);
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
 * Starts looping the current track indefinitely.
 **/
void DFPlayerMini::start_looping_current_track()
{
    _send_command(dfplayer::cmd::SET_SPLAY, 0, 1);
}



/**
 * `0x19`
 * 
 * Stops looping the current track.
 **/
void DFPlayerMini::stop_looping_current_track()
{
    _send_command(dfplayer::cmd::SET_SPLAY, 0, 0);
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
 * `0x42`
 * 
 * Queries the player's current status.
 * @return The player's status as a `uint16_t`.
 **/
uint16_t DFPlayerMini::get_status()
{
    _send_command(dfplayer::cmd::QRY_STATUS);
    return _read_response();
}



/**
 * `0x43`
 * 
 * Queries the player's current volume.
 * @return The player's volume as a `uint16_t`.
 **/
uint16_t DFPlayerMini::get_volume()
{
    _send_command(dfplayer::cmd::QRY_VOLUME);
    return _read_response();
}



/**
 * `0x4F`
 * 
 * Queries the number of folders on the storage device.
 * @return The number of folders as a `uint16_t`.
 **/
uint16_t DFPlayerMini::get_folder_count()
{
    _send_command(dfplayer::cmd::QUERY_FLDR_COUNT);
    return _read_response();
}



/**
 * `0x4E`
 * 
 * Queries the number of tracks in the current folder.
 * @return The number of tracks as a `uint16_t`.
 **/
uint16_t DFPlayerMini::get_folder_track_count()
{
    _send_command(dfplayer::cmd::QUERY_FLDR_TRACKS);
    return _read_response();
}



/**
 * `0x48`
 * 
 * Queries the total number of tracks on the storage device.
 * @return The total number of tracks as a `uint16_t`.
 **/
uint16_t DFPlayerMini::get_total_track_count()
{
    _send_command(dfplayer::cmd::QUERY_TOT_TRACKS);
    return _read_response();
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
    byte send_buf[8] = {0};    // Initialize data bytes buffer
    String send_str = "";      // Initialize string data buffer

    // Command Structure HEAD ADDR LEN  CMD ACK DATA CKL CKH END
    // Command Structure 0x7E 0xFF 0x06 CMD ACK DATA CK1 CK2 0xEF

    delay(20);

    send_buf[0] = 0x7E;     // HEAD byte constant
    send_buf[1] = 0xFF;     // ADDR byte constant
    send_buf[2] = 0x06;     // LEN  length excluding HEAD, END, CHECKSUM, and LENGTH bytes
    send_buf[3] = command;  // CMD
    send_buf[4] = 0x00;     // ACK  feedback 0x00 NO FEEDBACK, 0x01 FEEDBACK REQUESTED
    send_buf[5] = data1;    // DATA data byte 1
    send_buf[6] = data2;    // DATA data byyte 2
    send_buf[7] = 0xEF;     // END  byte constant

    for (int i = 0; i < 8; i++)
    {
        _serial -> write(send_buf[i]);
        send_str += _sbyte2hex(send_buf[i]);
    }

    if (_show_debug_messages) {
        Serial.print("Sending: ");
        Serial.println(send_str);     // Display hex bytes sent to DFPlayer
    }

    delay(1000);
}



/**
 * Reads and interprets the last frame received from the DFPlayer Mini.
 * @return The response as a `uint16_t`.
 **/
uint16_t DFPlayerMini::_read_response()
{
    uint16_t decoded_answer = 0x0000;       // Numeric value returned
    String   answer         = _sanswer();   // Read from DFPlayer serial port
    String   received       = "";           // Hex bytes received from DFPlayer

    // Load different numeric values based on which call is being answered (indicated by byte 4, ansbuf[3]).
    switch (_ansbuf[3])
    {
        /*
        * 0x3A == Device insertion (SD / USB / Flash device)
        * 0x3B == Device unplugged
        * 0x3C == UDISK Playback Completed
        * 0x3D == SD card playback completed
        * 0x3E == Flash playback completed
        * 0x3F == Send initialization parameters (set player status)
        */
        case 0x3A:
            decoded_answer = 0x0001;
            Serial.println ("\nMemory card inserted.");
            break;

        case 0x3B:
            decoded_answer = 0x0002;
            Serial.println("\nDevice unplugged.");
            break;

        case 0x3C:
            decoded_answer = 0x0003;
            Serial.println("\nDevice unplugged.");
            break;

        case 0x3D:
            decoded_answer = 0x0004;
            Serial.println("\nUDISK Playback Completed");
            break;

        case 0x3E:
            decoded_answer = 0x0005;
            Serial.println("\nFlash Playback Completed");
            break;

        case 0x3F:
            decoded_answer = 0x0006;
            Serial.println("\nSent initialization parameters");
            break;

        /*
        * 0x40 == Return error, request resend
        * 0x41 == Response
        * 0x42 == Query current status
        * 0x43 == Query the current volume
        * 0x44 == Query the current EQ
        * 0x45 == Query the current playback mode
        */
        case 0x40:
            decoded_answer = 0x0007;
            Serial.println("\nError, resend!");
            break;

        case 0x41:    
            decoded_answer = 0x0008;
            Serial.println("\nResponse received.");
            break;

        case 0x42:	     
            switch(_ansbuf[6]) {
                case 0: decoded_answer = 0x0A; break;  // |= (1<<4) | (1<<12); break;  // " -> Status: stopped"; break;
                case 1: decoded_answer = 0x0B; break;  // |= (1<<4) | (1<<11); break;  // " -> Status: playing"; break;
                case 2: decoded_answer = 0x0C; break;  // |= (1<<4) | (1<<10); break;  // " -> Status: paused"; break;
            }
            break;

        case 0x43:
            //+= " -> Vol playing: " + String(_ansbuf[6], DEC);
            decoded_answer = _ansbuf[6];	  
            break;

        case 0x48:
            //" -> File count: "
            decoded_answer = _ansbuf[6];  
            break;

        case 0x4C:
            //" -> Playing: "
            decoded_answer = _ansbuf[6];
            break;

        case 0x4E:
            //" -> Folder file count: "
            decoded_answer = _ansbuf[6];  
            break;

        case 0x4F:
            //" -> Folder count: "
            decoded_answer = _ansbuf[6];	
            break;
    }

    _ansbuf[3] = 0;  // Clear _ansbuf.
    return decoded_answer;
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



/**
 * Reads the response from the DFPlayer Mini and returns it as a string.
 * @return The response string.
 **/
String DFPlayerMini::_sanswer(void)
{
    byte b;
    int iansidx = 0;
    String answer = "";

    while (_serial -> available()) {
        b = _serial -> read();

        /*
         * 0x7E == Start byte
         *   reset ansbuf index to zero
         *   reset string response to null
         */
        if (b == 0x7E) {
            iansidx = 0;
            answer = "";
        }

        // Add each byte received to _ansbuf and also to the string response
        _ansbuf[iansidx] = b;
        answer += _sbyte2hex(_ansbuf[iansidx]);
        iansidx++;

        /*
         *  0x3A .. 0x3F
         *
         * 0x3A == Device insertion (SD / USB / Flash device)
         * 0x3B == Device unplugged
         * 0x3C == UDISK Playback Completed
         * 0x3D == SD card playback completed
         * 0x3E == Flash playback completed
         * 0x3F == Send initialization parameters (set player status)
         */
        if (b >= 0x3A && b <= 0x3F) {
            _ansbuf[3] = b;
        }

        // 0xEF == End byte
        if (b == 0xef) { }
    }

    // Debug - print output from DFPlayer device
    if (_show_debug_messages) {
        Serial.println("answer = " + String(answer));
    }

    return answer;
}