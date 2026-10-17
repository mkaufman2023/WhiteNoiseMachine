/******************************************************************
*                                                                 *
*    include / config / secrets_example.h                         *
*    Created by Matt Kaufman, December 4, 2025.                   *
*                                                                 *
*    Credentials for the home Wi-Fi network, and the key that     *
*    wireless firmware updates must present.                      *
*                                                                 *
*******************************************************************/

#ifndef SECRETS_CONFIG_H
#define SECRETS_CONFIG_H

// Copy this file to secrets.h (gitignored) and fill it in.
namespace secrets
{
    constexpr const char* wifi_ssid     = "";
    constexpr const char* wifi_password = "";

    // Any phrase you like, up to 64 letters, digits and dashes (it ends up in
    // a shell command, so no spaces, quotes or $). Every firmware upload must
    // carry it: the app asks for it once, and `pio run -e ota` reads it from
    // the WNM_OTA_KEY environment variable. Left empty, wireless updates are off.
    constexpr const char* ota_key       = "";
}


#endif  // SECRETS_CONFIG_H
