/******************************************************************
*                                                                 *
*    include / config / webserver.h                               *
*    Created by Matt Kaufman, December 4, 2025.                   *
*                                                                 *
*    Configuration file for Web Server settings and constants.    *
*                                                                 *
*******************************************************************/

#ifndef WEB_SERVER_CONFIG_H
#define WEB_SERVER_CONFIG_H


namespace webserver
{
    constexpr int port = 80;
    const char* hostname = "whitenoise";
    const char* hostname_full = "http://whitenoise.local";
}


#endif  // WEB_SERVER_CONFIG_H