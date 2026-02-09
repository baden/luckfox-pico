#include "crsf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "uart_utils.h"

static double get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// CRC8 implementation (Polynomial: 0xD5)
static uint8_t crsf_crc8(const uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0xD5;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

static void parse_rc_channels(crsf_t* crsf, const uint8_t* payload, int payload_len) {
    if (payload_len < 22) {
        printf("CRSF: RC channels payload too short: %d bytes\n", payload_len);
        return;
    }

    // Parse 16 channels, 11 bits each
    // The channels are packed into 22 bytes (176 bits)
    uint64_t value = 0;
    int bit_pos = 0;

    for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
        // Extract 11 bits for this channel
        if (bit_pos + 11 <= 176) {
            int byte_pos = bit_pos / 8;
            int bit_offset = bit_pos % 8;

            value = 0;
            int bits_remaining = 11;
            int current_bit_offset = bit_offset;

            while (bits_remaining > 0 && byte_pos < 22) {
                int bits_available = 8 - current_bit_offset;
                int bits_to_take = (bits_remaining < bits_available) ? bits_remaining : bits_available;

                uint8_t mask = (1 << bits_to_take) - 1;
                uint8_t shift_value = (payload[byte_pos] >> current_bit_offset) & mask;

                value |= ((uint64_t)shift_value << (11 - bits_remaining));

                bits_remaining -= bits_to_take;
                current_bit_offset = 0;
                byte_pos++;
            }

            // Convert from CRSF range (172-992-1811) to -1.0..0..1.0
            // 172 = min, 992 = center, 1811 = max
            uint16_t raw_value = (uint16_t)value & 0x07FF;
            if (raw_value <= 992) {
                crsf->channels.values[i] = (raw_value - 992.0) / (992.0 - 172.0);
            } else {
                crsf->channels.values[i] = (raw_value - 992.0) / (1811.0 - 992.0);
            }

            // Clamp values
            if (crsf->channels.values[i] < -1.0f) crsf->channels.values[i] = -1.0f;
            if (crsf->channels.values[i] > 1.0f) crsf->channels.values[i] = 1.0f;
        }

        bit_pos += 11;
    }

    #if 0
    static int skips = 0;
    if(skips < 100) {
        skips++;
    } else {
        skips = 0;
        printf("CRSF: Parsed RC Channels: ");
        for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
            printf("%.2f ", crsf->channels.values[i]);
        }
        printf("\n");
    }
    #endif

    crsf->channels.valid = true;
    crsf->channels.timestamp = get_time_seconds();
}

static int parse_packet(crsf_t* crsf) {
    // Robust parser that searches for valid packets in the buffer

    // We need at least 4 bytes for a minimal packet: Sync, Length, Type, CRC
    while (crsf->buffer_len >= 4) {
        // 1. Find Sync Byte
        if (crsf->buffer[0] != CRSF_SYNC_BYTE) {
            // Shift buffer by 1 to search for next sync byte
            memmove(crsf->buffer, &crsf->buffer[1], --crsf->buffer_len);
            continue;
        }

        // 2. Check Length
        uint8_t length = crsf->buffer[1];
        // Length range sanity check (e.g., 2 to 62 bytes payload)
        // Length = Type(1) + Payload(N) + CRC(1). So min length is 2.
        // Max CRSF packet is usually small (64 bytes). Let's allow up to 64.
        if (length < 2 || length > 64) {
             // Invalid length, shift by 1 and retry
             memmove(crsf->buffer, &crsf->buffer[1], --crsf->buffer_len);
             continue;
        }

        // 3. Check if we have the full packet
        int packet_size = length + 2; // Sync + Length + (Type + Payload + CRC)
        if (crsf->buffer_len < packet_size) {
            // Wait for more data
            return 0;
        }

        // 4. Verify CRC
        // CRC is calculated over Type(buffer[2]) to end of Payload
        // Length field includes Type, Payload, CRC.
        // So data for CRC is buffer[2] ... buffer[2 + length - 2]
        // Count = length - 1 (everything after length byte except CRC byte)

        uint8_t received_crc = crsf->buffer[packet_size - 1];
        uint8_t calculated_crc = crsf_crc8(&crsf->buffer[2], length - 1);

        if (received_crc != calculated_crc) {
            // printf("CRSF: CRC Mismatch (Len=%d, Calc=%02X, Recv=%02X)\n", length, calculated_crc, received_crc);
            // CRC failed. Shift by 1 and retry.
            // We assume this Sync Byte was a false positive.
            memmove(crsf->buffer, &crsf->buffer[1], --crsf->buffer_len);
            continue;
        }

        // printf("CRSF: Valid packet received (Type=%02X, Len=%d)\n", crsf->buffer[2], length);

        // 5. Process Packet
        uint8_t type = crsf->buffer[2];
        const uint8_t* payload = &crsf->buffer[3];
        int payload_len = length - 2;

        if (type == CRSF_FRAMETYPE_RC_CHANNELS_PACKET) {
            parse_rc_channels(crsf, payload, payload_len);
        }

        // 6. Consume Packet
        memmove(crsf->buffer, &crsf->buffer[packet_size], crsf->buffer_len - packet_size);
        crsf->buffer_len -= packet_size;
        return 1; // Processed one packet
    }
    return 0; // No more packets to process
}

int crsf_init(crsf_t* crsf, const char* device_path, int baudrate) {
    if (!crsf || !device_path) {
        perror("crsf_init: invalid arguments");
        return -1;
    }

    memset(crsf, 0, sizeof(crsf_t));
    crsf->device_path = device_path;
    crsf->baudrate = baudrate;

    return crsf_reconnect(crsf);
}

void crsf_cleanup(crsf_t* crsf) {
    if (!crsf) {
        return;
    }

    if (crsf->fd >= 0) {
        close(crsf->fd);
        crsf->fd = -1;
    }

    crsf->connected = false;
}

int crsf_reconnect(crsf_t* crsf) {
    if (!crsf || !crsf->device_path) {
        return -1;
    }

    // Close existing connection
    if (crsf->fd >= 0) {
        close(crsf->fd);
        crsf->fd = -1;
    }

    // Reset buffer
    crsf->buffer_len = 0;
    crsf->channels.valid = false;

    // Open UART device
    crsf->fd = open(crsf->device_path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (crsf->fd < 0) {
        perror("open UART");
        crsf->connected = false;
        return -1;
    }

    // Configure UART using termios2
    if (uart_set_custom_speed(crsf->fd, crsf->baudrate) != 0) {
        close(crsf->fd);
        crsf->fd = -1;
        crsf->connected = false;
        return -1;
    }

    crsf->connected = true;
    printf("CRSF: Connected to %s at %d baud (termios2)\n", crsf->device_path, crsf->baudrate);
    return 0;
}

int crsf_process(crsf_t* crsf) {
    if (!crsf || crsf->fd < 0) {
        perror("crsf_process: invalid CRSF instance");
        return -1;
    }

    // Determine how much space is left
    int space_left = CRSF_MAX_BUFFER_SIZE - crsf->buffer_len;

    // Safety check: if buffer is full, we must clear it to avoid getting stuck
    if (space_left <= 0) {
        printf("CRSF: Buffer overflow (full), resetting buffer\n");
        crsf->buffer_len = 0;
        space_left = CRSF_MAX_BUFFER_SIZE;
    }

    uint8_t temp_buffer[128];
    // Read up to what we can fit, or a reasonable chunk
    int to_read = (space_left < sizeof(temp_buffer)) ? space_left : sizeof(temp_buffer);

    ssize_t bytes_read = read(crsf->fd, temp_buffer, to_read);

    if (bytes_read > 0) {
        // Debug raw data (commented out for production, useful for debugging baudrate)
        // printf("RAW: ");
        // for(int i=0; i<bytes_read; i++) printf("%02X ", temp_buffer[i]);
        // printf("\n");

        // Add to buffer
        memcpy(&crsf->buffer[crsf->buffer_len], temp_buffer, bytes_read);
        crsf->buffer_len += bytes_read;

        // Parse packets loop
        while (parse_packet(crsf) > 0) {
            // Continue parsing until buffer is empty or no full packets left
        }
    } else if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("CRSF read error");
            crsf->connected = false;
            return -1;
        }
    }

    return 0;
}

static int crsf_write_packet(crsf_t* crsf, uint8_t type, const uint8_t* payload, uint8_t payload_len) {
    if (!crsf || crsf->fd < 0) return -1;

    // Packet structure:
    // [Sync] [Length] [Type] [Payload...] [CRC]
    // Length = Size(Type) + Size(Payload) + Size(CRC) = 1 + payload_len + 1
    
    uint8_t buffer[64];
    uint8_t length = 1 + payload_len + 1;
    
    if (length > 62) return -1; // Too large
    
    buffer[0] = CRSF_SYNC_BYTE;
    buffer[1] = length;
    buffer[2] = type;
    
    if (payload_len > 0) {
        memcpy(&buffer[3], payload, payload_len);
    }
    
    // CRC includes Type and Payload
    // Buffer indices involved: 2 ... (2 + payload_len)
    // Count = 1 + payload_len
    buffer[3 + payload_len] = crsf_crc8(&buffer[2], 1 + payload_len);
    
    int packet_size = length + 2;
    
    return write(crsf->fd, buffer, packet_size);
}

int crsf_send_telemetry_flight_mode(crsf_t* crsf, const char* mode_string) {
    if (!crsf || !mode_string) return -1;
    
    // Flight Mode frame type is 0x21
    // Payload is just the null-terminated string (including null terminator)
    int len = strlen(mode_string) + 1; 
    
    // CRSF limits are tight, ensure we don't overflow
    if (len > 60) len = 60; // Truncate if necessary (leaving room for header/crc)
    
    return crsf_write_packet(crsf, 0x21, (const uint8_t*)mode_string, len);
}

bool crsf_is_connected(const crsf_t* crsf) {
    return crsf ? crsf->connected : false;
}

int crsf_get_channels(const crsf_t* crsf, crsf_channels_t* channels) {
    if (!crsf || !channels) {
        return -1;
    }

    if (!crsf->channels.valid) {
        return -1;
    }

    *channels = crsf->channels;
    return 0;
}
