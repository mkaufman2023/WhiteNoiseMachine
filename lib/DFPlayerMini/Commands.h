/****************************************************************************************
*                                                                                       *
*   Commands.h - Commands for TD5580A-based DFPlayer Mini clones                        *
*                                                                                       *
*   Adapted from https://github.com/keith721/TD5580A                                    *
*   Adapted for HiLetgo's clone (see: 2) by Matt Kaufman, December, 2025.               *
*                                                                                       *
*   See:                                                                                *
*     1. http://www.tudasemi.com/static/upload/file/20240905/1725499313437991.pdf       *
*     2. https://www.amazon.com/HiLetgo-Arduino-Player-Module-DFPlayer/dp/B01D1D0E7Q    *
*                                                                                       *
*****************************************************************************************/

#pragma once
#include <stdint.h>


namespace dfplayer::cmd
{
    constexpr uint8_t NEXT             = 0x01;  // Play next file
    constexpr uint8_t PREVIOUS         = 0x02;  // Play previous file
    constexpr uint8_t PLAY_N           = 0x03;  // Play n file from (1-255)
    constexpr uint8_t VOL_UP           = 0x04;  // Volume +
    constexpr uint8_t VOL_DOWN         = 0x05;  // Volume -
    constexpr uint8_t SET_VOL          = 0x06;  // Set Volume (0-30)
    constexpr uint8_t SET_EQ           = 0x07;  // Set Equalization 0..6 0:Normal, 1:Rock, 3:Pop, 4:Classic, 5:Country, 6:Jazz
    constexpr uint8_t PLAY_S_LOOP      = 0x08;  // Single loop play.
    constexpr uint8_t SET_SOURCE       = 0x09;  // Set source: 1-USB Disk, 2-SD Card, 3-Aux, 4-Flash, 5-PC, 6-Sleep
    constexpr uint8_t SLEEP_MODE       = 0x0A;  // low power mode (sleep)
    constexpr uint8_t WAKE_UP          = 0x0B;  // Reserved
    constexpr uint8_t RESET            = 0x0C;  // Chip Reset
    constexpr uint8_t PLAY             = 0x0D;  // Play
    constexpr uint8_t PAUSE            = 0x0E;  // Pause
    constexpr uint8_t PLAY_F_FILE      = 0x0F;  // Play Folder and File
    constexpr uint8_t AMPLIFIER        = 0x10;  // Amplifier settings (none)
    constexpr uint8_t PLAY_LOOPS       = 0x11;  // Play all tracks in a loop
    constexpr uint8_t SET_FOLDER       = 0x12;  // Specify the MP3 folder name - must be 'MP3'
    constexpr uint8_t PLAY_ADVERT      = 0x13;  // Insert ADVERT file - folder name must be 'ADVERT'
    constexpr uint8_t PLAY_F_FILE2     = 0x14;  // Play Folder and File (Same as= 0x0F)
    constexpr uint8_t STOP_ADVERTS     = 0x15;  // Stop inserting ads
    constexpr uint8_t STOP_PLAY        = 0x16;  // Stop playing
    constexpr uint8_t FOLDER_CYCLE     = 0x17;  // Loop playback of files in specified folder
    constexpr uint8_t PLAY_SHUFFLE     = 0x18;  // All files are played randomly in physical order
    constexpr uint8_t SET_SPLAY        = 0x19;  // Current track loop play [00] close single cycle play [01]
    constexpr uint8_t DAC_IMP_HIGH     = 0x1A;  // Enable DAC high impedance 0-Disable; 1-Enable 
    constexpr uint8_t PWR_ON_VOL_MEM   = 0x1B;  // 0-Power-on memory; 1-30 Fixed power-on volume
    constexpr uint8_t SET_BAUD_RATE    = 0x1C;  // 0-9600;1-19200;2-38400; 3-57600; 4-115200; Restart to take effect
    constexpr uint8_t FACTORY_RESET    = 0x1D;  // Restore factory settings
    constexpr uint8_t COMBO_REPORT     = 0x20;  // Combination Report

    // Query Commands
    constexpr uint8_t QU_DEV_INSERTED  = 0x3A;  // Device inserted? 1-UDISK, 2-SD Card, 3-PC
    constexpr uint8_t QU_DEV_UNPLUGGED = 0x3B;  // Device unplugged
    constexpr uint8_t QU_UDISK_COMPL   = 0x3C;  // UDISK completed playing
    constexpr uint8_t QU_TF_SD_COMPL   = 0x3D;  // TF/SD card completed playing
    constexpr uint8_t QU_FLASH_COMPL   = 0x3E;  // Flash mem completed playing
    constexpr uint8_t SEND_INIT_PARAMS = 0x3F;  // Send initialization parameters

    // Status / Responses
    constexpr uint8_t ERROR_RESEND         = 0x40;  // Return error, request resend
    constexpr uint8_t RESPONSE             = 0x41;  // Response
    constexpr uint8_t QRY_STATUS           = 0x42;  // Query the status
    constexpr uint8_t QRY_VOLUME           = 0x43;  // Query the volume - sometimes board fails
    constexpr uint8_t QRY_EQUALIZATION     = 0x44;  // Query the Equalization setting
    constexpr uint8_t QRY_PLAYBACK_MODE    = 0x45;  // Query the Playback mode
    constexpr uint8_t QRY_SW_VERSION       = 0x46;  // Query the Software version
    constexpr uint8_t QRY_TOTAL_FILES_USB  = 0x47;  // Query the number of files on USB-stick
    constexpr uint8_t QRY_TOTAL_FILES_TFC  = 0x48;  // Query the number of files on TF/SD card
    constexpr uint8_t QRY_TOTAL_FILES_FLSH = 0x49;  // Query the number of files on Flash storage
    constexpr uint8_t QRY_RESERVED_4A      = 0x4A;  // Reserved
    constexpr uint8_t QRY_TRACK_USB        = 0x4B;  // Query the current track of the USB disk
    constexpr uint8_t QRY_TRACK_SD_CARD    = 0x4C;  // Query the current track on the SD card
    constexpr uint8_t QRY_TRACK_CHK_FLASH  = 0x4D;  // Check FLASH current track
    constexpr uint8_t QUERY_FLDR_TRACKS    = 0x4E;  // Query the number of tracks folders
    constexpr uint8_t QUERY_TOT_TRACKS     = 0x48;  // Query the total of tracks
    constexpr uint8_t QUERY_FLDR_COUNT     = 0x4F;  // Query the number of folders
    constexpr uint8_t STS_USB_DRIVE_ONLINE = 0x01;  // USB device is attached and online
    constexpr uint8_t STS_SD_CARD_ONLINE   = 0x02;  // TF or SD card is inserted and online
    constexpr uint8_t STS_U_DISK_ONLINE    = 0x03;  // USB Disk is attached and online
    constexpr uint8_t STS_PC_ONLINE        = 0x04;  // PC device is attached and online
    constexpr uint8_t STS_FLASH_ONLINE     = 0x08;  // Flash device is attached and online
}
