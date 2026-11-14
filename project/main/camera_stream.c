#include "app_state.h"
#include "app_config.h"
#include "esp_log.h"

static const char *const urls[CAMERA_COUNT][CAMERA_RESOLUTION_COUNT] = {
    {CAMERA_URL_0_QVGA, CAMERA_URL_0_VGA},
    {CAMERA_URL_1_QVGA, CAMERA_URL_1_VGA},
    {CAMERA_URL_2_QVGA, CAMERA_URL_2_VGA},
};

const char *camera_stream_url(void)
{
    return urls[g_app.active_camera][g_app.camera_resolution];
}

static void apply_selection(int index, camera_resolution_t resolution)
{
    camera_stream_stop();
    g_app.active_camera = index;
    g_app.camera_resolution = resolution;
    if (!camera_stream_url()[0]) {
        ESP_LOGW("camera", "No MJPEG endpoint configured for this selection");
        return;
    }
    // Start a single MJPEG worker with camera_stream_url() when playback is implemented.
}

void camera_stream_select(int index)
{
    if (index >= 0 && index < CAMERA_COUNT)
        apply_selection(index, g_app.camera_resolution);
}

void camera_stream_set_resolution(camera_resolution_t resolution)
{
    if ((unsigned)resolution < CAMERA_RESOLUTION_COUNT && resolution != g_app.camera_resolution)
        apply_selection(g_app.active_camera, resolution);
}

void camera_stream_stop(void)
{
    // Close the active MJPEG connection here when playback is implemented.
}
