#include "crsf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

static double get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static int configure_uart(int fd, int baudrate) {
    struct termios tty;
    
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }
    
    // Clear old settings
    memset(&tty, 0, sizeof tty);
    
    // Configure serial port
    tty.c_cflag &= ~PARENB;   // No parity
    tty.c_cflag &= ~CSTOPB;   // One stop bit
    tty.c_cflag &= ~CSIZE;    // Clear data bits
    tty.c_cflag |= CS8;       // 8 data bits
    tty.c_cflag &= ~CRTSCTS;  // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control
    
    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    
    // Raw mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;
    
    // Set timeout
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; // 0.1 second timeout
    
    // Set baud rate
    speed_t speed;
    switch (baudrate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 500000: speed = B500000; break;
        default:
            // For non-standard baudrates like 420000
            if (cfsetispeed(&tty, baudrate) != 0 || cfsetospeed(&tty, baudrate) != 0) {
                perror("cfsetispeed/cfsetospeed");
                return -1;
            }
            goto skip_standard_speed;
    }
    
    if (cfsetispeed(&tty, speed) != 0 || cfsetospeed(&tty, speed) != 0) {
        perror("cfsetispeed/cfsetospeed");
        return -1;
    }
    
skip_standard_speed:
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }
    
    return 0;
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
    
    crsf->channels.valid = true;
    crsf->channels.timestamp = get_time_seconds();
}

static int parse_packet(crsf_t* crsf) {
    // Look for sync byte
    int sync_index = -1;
    for (int i = 0; i < crsf->buffer_len - 2; i++) {
        if (crsf->buffer[i] == CRSF_SYNC_BYTE) {
            sync_index = i;
            break;
        }
    }
    
    if (sync_index == -1) {
        // No sync byte found, clear buffer
        crsf->buffer_len = 0;
        return 0;
    }
    
    // Remove bytes before sync
    if (sync_index > 0) {
        memmove(crsf->buffer, &crsf->buffer[sync_index], crsf->buffer_len - sync_index);
        crsf->buffer_len -= sync_index;
        sync_index = 0;
    }
    
    // Check if we have enough for header
    if (crsf->buffer_len < 3) {
        return 0; // Need more data
    }
    
    // Get packet length (length includes type + payload + crc, but not sync)
    uint8_t packet_length = crsf->buffer[1];
    int total_packet_size = packet_length + 2; // + sync byte + length byte
    
    // Check if we have full packet
    if (crsf->buffer_len < total_packet_size) {
        return 0; // Need more data
    }
    
    // Extract packet components
    uint8_t frame_type = crsf->buffer[2];
    const uint8_t* payload = &crsf->buffer[3];
    int payload_len = packet_length - 2; // Subtract type and crc
    
    // Process based on frame type
    switch (frame_type) {
        case CRSF_FRAMETYPE_RC_CHANNELS_PACKET:
            parse_rc_channels(crsf, payload, payload_len);
            break;
        default:
            // Ignore other frame types
            break;
    }
    
    // Remove processed packet from buffer
    memmove(crsf->buffer, &crsf->buffer[total_packet_size], crsf->buffer_len - total_packet_size);
    crsf->buffer_len -= total_packet_size;
    
    return 1;
}

int crsf_init(crsf_t* crsf, const char* device_path, int baudrate) {
    if (!crsf || !device_path) {
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
    
    // Configure UART
    if (configure_uart(crsf->fd, crsf->baudrate) != 0) {
        close(crsf->fd);
        crsf->fd = -1;
        crsf->connected = false;
        return -1;
    }
    
    crsf->connected = true;
    printf("CRSF: Connected to %s at %d baud\n", crsf->device_path, crsf->baudrate);
    return 0;
}

int crsf_process(crsf_t* crsf) {
    if (!crsf || crsf->fd < 0) {
        return -1;
    }
    
    // Read available data
    uint8_t temp_buffer[64];
    ssize_t bytes_read = read(crsf->fd, temp_buffer, sizeof(temp_buffer));
    
    if (bytes_read > 0) {
        // Add to buffer
        if (crsf->buffer_len + bytes_read < CRSF_MAX_BUFFER_SIZE) {
            memcpy(&crsf->buffer[crsf->buffer_len], temp_buffer, bytes_read);
            crsf->buffer_len += bytes_read;
        } else {
            printf("CRSF: Buffer overflow, resetting\n");
            crsf->buffer_len = 0;
            return -1;
        }
        
        // Parse packets
        while (parse_packet(crsf) > 0) {
            // Continue parsing
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