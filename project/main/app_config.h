#pragma once
#define CAMERA_COUNT 3
#define CAMERA_NAME_0 "North Gate"
#define CAMERA_NAME_1 "Loading Bay"
#define CAMERA_NAME_2 "Workshop"
/* Set each URL to an MJPEG endpoint that serves the indicated resolution.
 * Enter camera addresses on the device; no site-specific addresses are shipped.
 */
#define CAMERA_URL_0_QVGA ""
#define CAMERA_URL_1_QVGA ""
#define CAMERA_URL_2_QVGA ""
#define CAMERA_URL_0_VGA ""
#define CAMERA_URL_1_VGA ""
#define CAMERA_URL_2_VGA ""
#define HALOW_PROFILE_NAME "hallow-server"

/* POSIX time zone for Toronto. Use a local NTP server on isolated networks. */
#define CLOCK_TIME_ZONE "EST5EDT,M3.2.0,M11.1.0"
#define CLOCK_NTP_SERVER "pool.ntp.org"
