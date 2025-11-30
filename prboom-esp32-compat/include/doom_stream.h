// DOOM TCP Stream Header
#ifndef DOOM_STREAM_H
#define DOOM_STREAM_H

#include <stdint.h>
#include <stdbool.h>

// Initialize streaming buffers
void doom_stream_init(void);

// Initialize WiFi and start TCP server
void doom_stream_wifi_init(const char *ssid, const char *password);

// Update frame buffer with new RGB565 frame
void doom_stream_update_frame(const uint8_t *pixels, int width, int height);

// Set palette (for indexed color compatibility)
void doom_stream_set_palette(const unsigned char *palette);

// Check if a viewer is connected
bool doom_stream_is_connected(void);

// Process queued keyboard input (call from I_StartTic)
void doom_stream_process_keys(void);

#endif // DOOM_STREAM_H
