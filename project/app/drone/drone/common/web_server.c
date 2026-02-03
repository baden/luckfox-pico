#include "web_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_BUFFER 4096
#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// --- SHA1 Implementation (Minimal) ---
typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

#define ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void SHA1Transform(uint32_t state[5], const unsigned char buffer[64]) {
    uint32_t a, b, c, d, e;
    typedef union {
        unsigned char c[64];
        uint32_t l[16];
    } CHAR64LONG16;
    CHAR64LONG16 block[1];
    memcpy(block, buffer, 64);
    
    // Convert to big endian
    for (int i=0; i<16; i++) {
        block->l[i] = (block->c[i*4] << 24) | (block->c[i*4+1] << 16) | (block->c[i*4+2] << 8) | (block->c[i*4+3]);
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    
    // 80 operations omitted for brevity, but I need a working SHA1 for handshake.
    // Since I can't depend on libraries, I'll use a very compact loop or just trust a known implementation.
    // For this context, I will implement a simplified version or use a very standard one.
    // Actually, to save tokens and ensure correctness, I will use a very standard compact implementation structure.
    
    // ... Actually, writing a full SHA1 here is verbose.
    // Let's assume for a moment I can implement a "dumb" one or finding a simpler way?
    // No, WebSocket requires SHA1. I must implement it.
    
    uint32_t w[80];
    for(int i=0; i<16; i++) w[i] = block->l[i];
    for(int i=16; i<80; i++) w[i] = ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    
    for(int i=0; i<80; i++) {
        uint32_t f, k;
        if(i<20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if(i<40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if(i<60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        
        uint32_t temp = ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL(b, 30); b = a; a = temp;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void SHA1Init(SHA1_CTX* context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}

static void SHA1Update(SHA1_CTX* context, const unsigned char* data, uint32_t len) {
    uint32_t i, j;
    j = context->count[0];
    if ((context->count[0] += len << 3) < j) context->count[1]++;
    context->count[1] += (len >> 29);
    j = (j >> 3) & 63;
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        SHA1Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64) SHA1Transform(context->state, &data[i]);
        j = 0;
    } else i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}

static void SHA1Final(unsigned char digest[20], SHA1_CTX* context) {
    unsigned char finalcount[8];
    for (int i = 0; i < 8; i++) finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    unsigned char c = 0200;
    SHA1Update(context, &c, 1);
    while ((context->count[0] & 504) != 448) {
        c = 0000;
        SHA1Update(context, &c, 1);
    }
    SHA1Update(context, finalcount, 8);
    for (int i = 0; i < 20; i++) {
        digest[i] = (unsigned char)((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}

// --- Base64 Implementation ---
static void base64_encode(const unsigned char* input, int len, char* output) {
    const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j;
    for (i = 0, j = 0; i < len; i += 3) {
        uint32_t val = (input[i] << 16) | ((i + 1 < len ? input[i + 1] : 0) << 8) | (i + 2 < len ? input[i + 2] : 0);
        output[j++] = table[(val >> 18) & 0x3F];
        output[j++] = table[(val >> 12) & 0x3F];
        output[j++] = (i + 1 < len) ? table[(val >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < len) ? table[val & 0x3F] : '=';
    }
    output[j] = '\0';
}

// --- Web Server Logic ---

int web_server_init(web_server_t* server, int port, const char* www_root) {
    memset(server, 0, sizeof(web_server_t));
    server->port = port;
    server->www_root = www_root;
    server->client_fd = -1;

    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        perror("web_server: socket failed");
        return -1;
    }

    int opt = 1;
    setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Non-blocking
    int flags = fcntl(server->server_fd, F_GETFL, 0);
    fcntl(server->server_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("web_server: bind failed");
        close(server->server_fd);
        return -1;
    }

    if (listen(server->server_fd, 3) < 0) {
        perror("web_server: listen failed");
        close(server->server_fd);
        return -1;
    }

    printf("Web server listening on port %d, serving %s\n", port, www_root);
    return 0;
}

void web_server_cleanup(web_server_t* server) {
    if (server->client_fd >= 0) close(server->client_fd);
    if (server->server_fd >= 0) close(server->server_fd);
}

static void send_404(int fd) {
    const char* resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    send(fd, resp, strlen(resp), 0);
}

static const char* get_mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    return "text/plain";
}

static void serve_file(web_server_t* server, const char* path) {
    char full_path[512];
    
    // Prevent directory traversal
    if (strstr(path, "..")) {
        send_404(server->client_fd);
        return;
    }

    if (strcmp(path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", server->www_root);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", server->www_root, path);
    }

    FILE* f = fopen(full_path, "rb");
    if (!f) {
        printf("Web: File not found: %s\n", full_path);
        send_404(server->client_fd);
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char header[512];
    snprintf(header, sizeof(header), 
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Connection: close\r\n\r\n", 
             get_mime_type(full_path), fsize);
    send(server->client_fd, header, strlen(header), 0);

    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        send(server->client_fd, buf, n, 0);
    }
    fclose(f);
    
    // For simple HTTP GET, we close connection after serving
    // Only upgrade requests keep connection open
    close(server->client_fd);
    server->client_fd = -1;
    server->connected = false;
}

static void handle_websocket_handshake(web_server_t* server, const char* key) {
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, WEBSOCKET_GUID);
    
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, (unsigned char*)combined, strlen(combined));
    unsigned char hash[20];
    SHA1Final(hash, &ctx);
    
    char encoded[64];
    base64_encode(hash, 20, encoded);
    
    char response[512];
    snprintf(response, sizeof(response),
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n", encoded);
             
    send(server->client_fd, response, strlen(response), 0);
    server->connected = true;
    printf("Web: WebSocket connected!\n");
}

void web_server_run_step(web_server_t* server, web_control_input_t* input) {
    // 1. Accept new connections
    if (server->client_fd < 0) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int new_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        if (new_fd >= 0) {
            // Close old connection if open
            if (server->client_fd >= 0) {
                close(server->client_fd);
            }
            
            // Set non-blocking
            int flags = fcntl(new_fd, F_GETFL, 0);
            fcntl(new_fd, F_SETFL, flags | O_NONBLOCK);
            
            server->client_fd = new_fd;
            server->connected = false; // Waiting for handshake
            // printf("Web: Client connected\n");
        }
    }
    
    if (server->client_fd < 0) return;

    // 2. Read data
    uint8_t buffer[MAX_BUFFER];
    ssize_t n = recv(server->client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (n > 0) {
        buffer[n] = 0;
        
        if (!server->connected) {
            // Check if HTTP GET or Upgrade
            if (strncmp((char*)buffer, "GET ", 4) == 0) {
                // Parse headers
                char* line = strtok((char*)buffer, "\r\n");
                char path[256] = {0};
                sscanf(line, "GET %255s", path);
                
                // Look for Upgrade header
                char* key = NULL;
                bool is_upgrade = false;
                
                while ((line = strtok(NULL, "\r\n"))) {
                    if (strncasecmp(line, "Sec-WebSocket-Key: ", 19) == 0) {
                        key = line + 19;
                    }
                    if (strncasecmp(line, "Upgrade: websocket", 18) == 0) {
                        is_upgrade = true;
                    }
                }
                
                if (is_upgrade && key) {
                    // Remove leading spaces from key if any
                    while(*key == ' ') key++;
                    handle_websocket_handshake(server, key);
                } else {
                    serve_file(server, path);
                }
            }
        } else {
            // WebSocket Frame Parsing
            // Minimal parser for unfragmented text frames
            // Byte 0: FIN(1) RSV(3) Opcode(4)
            // Byte 1: Mask(1) PayloadLen(7)
            
            if (n < 2) return;
            
            uint8_t opcode = buffer[0] & 0x0F;
            uint8_t fin = (buffer[0] >> 7) & 0x01;
            uint8_t masked = (buffer[1] >> 7) & 0x01;
            uint64_t payload_len = buffer[1] & 0x7F;
            
            int offset = 2;
            if (payload_len == 126) {
                if (n < 4) return;
                payload_len = (buffer[2] << 8) | buffer[3];
                offset += 2;
            } else if (payload_len == 127) {
                // Too huge, ignore
                return;
            }
            
            uint8_t masking_key[4];
            if (masked) {
                if (n < offset + 4) return;
                memcpy(masking_key, &buffer[offset], 4);
                offset += 4;
            }
            
            if (n < offset + payload_len) return; // Incomplete frame
            
            if (opcode == 0x8) { // Close
                close(server->client_fd);
                server->client_fd = -1;
                server->connected = false;
                printf("Web: Client disconnected\n");
                return;
            }
            
            if (opcode == 0x1) { // Text frame
                // Unmask
                char* payload = (char*)&buffer[offset];
                if (masked) {
                    for (int i = 0; i < payload_len; i++) {
                        payload[i] ^= masking_key[i % 4];
                    }
                }
                payload[payload_len] = 0;
                
                // Parse JSON control input
                // Expected: {"a0":0.5,"a1":0.2,"a2":0.0,"a3":0.0,"arm":true,"lb":0,"ak":0}
                // Very naive parsing to avoid json lib dependency
                // We just look for keys string by string
                
                float a0=0, a1=0, a2=0, a3=0;
                int arm=0, disarm=0, lb=0, ak=0;
                
                // A0 (Roll)
                char* p = strstr(payload, "\"a0\":");
                if (p) a0 = strtof(p + 5, NULL);
                
                // A1 (Pitch)
                p = strstr(payload, "\"a1\":");
                if (p) a1 = strtof(p + 5, NULL);
                
                // A2 (Throttle)
                p = strstr(payload, "\"a2\":");
                if (p) a2 = strtof(p + 5, NULL);
                
                // A3 (Yaw)
                p = strstr(payload, "\"a3\":");
                if (p) a3 = strtof(p + 5, NULL);
                
                // ARM/DISARM
                p = strstr(payload, "\"arm\":true");
                if (p) arm = 1;
                p = strstr(payload, "\"arm\":false");
                if (p) disarm = 1; // Actually logic might differ, usually we send cmd
                
                // But for momentary switches in UI:
                p = strstr(payload, "\"cmd_arm\":true");
                if (p) arm = 1;
                p = strstr(payload, "\"cmd_disarm\":true");
                if (p) disarm = 1;
                
                // Lebidka
                p = strstr(payload, "\"lb\":");
                if (p) lb = atoi(p + 5);
                
                // Aktuator
                p = strstr(payload, "\"ak\":");
                if (p) ak = atoi(p + 5);
                
                // Update input struct
                input->axes[0] = a0;
                input->axes[1] = a1;
                input->axes[2] = a2;
                input->axes[3] = a3;
                input->cmd_arm = arm;
                input->cmd_disarm = disarm;
                input->lebidka_val = lb;
                input->aktuator_val = ak;
                input->valid = true;
                input->timestamp = 0; // Caller will set timestamp
                
                // printf("Web Control: %.2f %.2f Arm:%d\n", a0, a1, arm);
            }
        }
    } else if (n == 0) {
        // Closed by client
        if (server->client_fd >= 0) {
            close(server->client_fd);
            server->client_fd = -1;
            server->connected = false;
        }
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error
             if (server->client_fd >= 0) {
                close(server->client_fd);
                server->client_fd = -1;
                server->connected = false;
            }
        }
    }
}

void web_server_send_telemetry(web_server_t* server, float r, float p, float y, float t, bool armed) {
    if (!server->connected || server->client_fd < 0) return;
    
    char json[256];
    snprintf(json, sizeof(json), "{\"r\":%.2f,\"p\":%.2f,\"y\":%.2f,\"t\":%.2f,\"armed\":%s}", 
             r, p, y, t, armed ? "true" : "false");
    
    size_t len = strlen(json);
    uint8_t frame[270];
    
    frame[0] = 0x81; // FIN + Text
    if (len < 126) {
        frame[1] = len;
        memcpy(&frame[2], json, len);
        send(server->client_fd, frame, len + 2, 0);
    } else {
        frame[1] = 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        memcpy(&frame[4], json, len);
        send(server->client_fd, frame, len + 4, 0);
    }
}
