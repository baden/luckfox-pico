#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>
#include <stdbool.h>
#include <termios.h>

#define CRSF_SYNC_BYTE 0xC8
#define CRSF_FRAMETYPE_RC_CHANNELS_PACKET 0x16
#define CRSF_MAX_BUFFER_SIZE 1024
#define CRSF_NUM_CHANNELS 16

typedef struct {
    float values[CRSF_NUM_CHANNELS];
    bool valid;
    double timestamp;
} crsf_channels_t;

typedef struct {
    int fd;
    uint8_t buffer[CRSF_MAX_BUFFER_SIZE];
    int buffer_len;
    crsf_channels_t channels;
    const char* device_path;
    int baudrate;
    bool connected;
} crsf_t;

// Initialize CRSF
int crsf_init(crsf_t* crsf, const char* device_path, int baudrate);

// Cleanup CRSF
void crsf_cleanup(crsf_t* crsf);

// Process incoming data
int crsf_process(crsf_t* crsf);

// Check if connected
bool crsf_is_connected(const crsf_t* crsf);

// Get latest channels
int crsf_get_channels(const crsf_t* crsf, crsf_channels_t* channels);

// Reconnect if disconnected
int crsf_reconnect(crsf_t* crsf);

#endif // CRSF_H