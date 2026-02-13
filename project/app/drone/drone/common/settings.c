#include "settings.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

static app_settings_t g_settings;
static pthread_mutex_t g_settings_mutex = PTHREAD_MUTEX_INITIALIZER;

// Default values
#define DEFAULT_STEERING_DAMPING 0.5f
#define DEFAULT_STEERING_DAMPING_CURVE 0.0f
#define DEFAULT_UDP_HOST "192.168.1.10"
#define DEFAULT_UDP_PORT 8080

// Helper to ensure directory exists (not strictly needed if we assume /oem/... exists, but good for safety)
// We won't implement recursive mkdir here to keep it simple, just writing to the path.

static void load_defaults(void) {
    g_settings.steering_damping = DEFAULT_STEERING_DAMPING;
    g_settings.steering_damping_curve = DEFAULT_STEERING_DAMPING_CURVE;
    strncpy(g_settings.udp_host, DEFAULT_UDP_HOST, sizeof(g_settings.udp_host) - 1);
    g_settings.udp_host[sizeof(g_settings.udp_host) - 1] = '\0';
    g_settings.udp_port = DEFAULT_UDP_PORT;
}

static void load_from_env(void) {
    char *val;

    val = getenv("DRONE_STEERING_DAMPING");
    if (val) g_settings.steering_damping = strtof(val, NULL);

    val = getenv("DRONE_STEERING_DAMPING_CURVE");
    if (val) g_settings.steering_damping_curve = strtof(val, NULL);

    val = getenv("DRONE_UDP_HOST");
    if (val) {
        strncpy(g_settings.udp_host, val, sizeof(g_settings.udp_host) - 1);
        g_settings.udp_host[sizeof(g_settings.udp_host) - 1] = '\0';
    }

    val = getenv("DRONE_UDP_PORT");
    if (val) g_settings.udp_port = atoi(val);
}

static int load_from_file(void) {
    FILE *f = fopen(SETTINGS_FILE_PATH, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (length <= 0) {
        fclose(f);
        return -1;
    }

    char *buffer = malloc(length + 1);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    fread(buffer, 1, length, f);
    buffer[length] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buffer);
    if (!json) {
        free(buffer);
        return -1; // Parse error
    }

    cJSON *item;

    item = cJSON_GetObjectItem(json, "steering_damping");
    if (cJSON_IsNumber(item)) g_settings.steering_damping = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "steering_damping_curve");
    if (cJSON_IsNumber(item)) g_settings.steering_damping_curve = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "udp_host");
    if (cJSON_IsString(item)) {
        strncpy(g_settings.udp_host, item->valuestring, sizeof(g_settings.udp_host) - 1);
        g_settings.udp_host[sizeof(g_settings.udp_host) - 1] = '\0';
    }

    item = cJSON_GetObjectItem(json, "udp_port");
    if (cJSON_IsNumber(item)) g_settings.udp_port = item->valueint;

    cJSON_Delete(json);
    free(buffer);
    return 0;
}

void settings_init(void) {
    pthread_mutex_lock(&g_settings_mutex);
    
    // 1. Load defaults
    load_defaults();

    // 2. Load from Env (overrides defaults)
    load_from_env();

    // 3. Load from File (overrides Env/Defaults if exists)
    if (load_from_file() != 0) {
        // If file load fails (doesn't exist), we might want to save the current (Env/Default) state to create the file?
        // For now, we just proceed with Env/Defaults.
        // printf("Settings file not found or invalid, using defaults/env.\n");
    }

    pthread_mutex_unlock(&g_settings_mutex);
}

void settings_save(void) {
    pthread_mutex_lock(&g_settings_mutex);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "steering_damping", g_settings.steering_damping);
    cJSON_AddNumberToObject(json, "steering_damping_curve", g_settings.steering_damping_curve);
    cJSON_AddStringToObject(json, "udp_host", g_settings.udp_host);
    cJSON_AddNumberToObject(json, "udp_port", g_settings.udp_port);

    char *json_str = cJSON_Print(json);
    
    // Create directory if it doesn't exist (basic check)
    // For /oem/usr/share/drone, we assume the path exists or is created by installation.
    // We'll write blindly for now.
    
    FILE *f = fopen(SETTINGS_FILE_PATH, "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
    } else {
        perror("Failed to write settings file");
    }

    free(json_str);
    cJSON_Delete(json);

    pthread_mutex_unlock(&g_settings_mutex);
}

app_settings_t settings_get(void) {
    pthread_mutex_lock(&g_settings_mutex);
    app_settings_t copy = g_settings;
    pthread_mutex_unlock(&g_settings_mutex);
    return copy;
}

void settings_set(const app_settings_t *new_settings) {
    pthread_mutex_lock(&g_settings_mutex);
    g_settings = *new_settings;
    pthread_mutex_unlock(&g_settings_mutex);
    settings_save(); // Auto-save on set? Or manual save? Prompt implied persistence, so auto-save makes sense for "set_setting" commands.
}

float settings_get_steering_damping(void) {
    pthread_mutex_lock(&g_settings_mutex);
    float val = g_settings.steering_damping;
    pthread_mutex_unlock(&g_settings_mutex);
    return val;
}

float settings_get_steering_damping_curve(void) {
    pthread_mutex_lock(&g_settings_mutex);
    float val = g_settings.steering_damping_curve;
    pthread_mutex_unlock(&g_settings_mutex);
    return val;
}

void settings_get_udp_host(char *buffer, size_t size) {
    pthread_mutex_lock(&g_settings_mutex);
    strncpy(buffer, g_settings.udp_host, size - 1);
    buffer[size - 1] = '\0';
    pthread_mutex_unlock(&g_settings_mutex);
}

int settings_get_udp_port(void) {
    pthread_mutex_lock(&g_settings_mutex);
    int val = g_settings.udp_port;
    pthread_mutex_unlock(&g_settings_mutex);
    return val;
}

void settings_set_steering_damping(float value) {
    pthread_mutex_lock(&g_settings_mutex);
    g_settings.steering_damping = value;
    pthread_mutex_unlock(&g_settings_mutex);
    settings_save();
}

void settings_set_steering_damping_curve(float value) {
    pthread_mutex_lock(&g_settings_mutex);
    g_settings.steering_damping_curve = value;
    pthread_mutex_unlock(&g_settings_mutex);
    settings_save();
}

void settings_set_udp_host(const char *host) {
    pthread_mutex_lock(&g_settings_mutex);
    strncpy(g_settings.udp_host, host, sizeof(g_settings.udp_host) - 1);
    g_settings.udp_host[sizeof(g_settings.udp_host) - 1] = '\0';
    pthread_mutex_unlock(&g_settings_mutex);
    settings_save();
}

void settings_set_udp_port(int port) {
    pthread_mutex_lock(&g_settings_mutex);
    g_settings.udp_port = port;
    pthread_mutex_unlock(&g_settings_mutex);
    settings_save();
}

char *settings_get_json_string(void) {
    pthread_mutex_lock(&g_settings_mutex);
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "steering_damping", g_settings.steering_damping);
    cJSON_AddNumberToObject(json, "steering_damping_curve", g_settings.steering_damping_curve);
    cJSON_AddStringToObject(json, "udp_host", g_settings.udp_host);
    cJSON_AddNumberToObject(json, "udp_port", g_settings.udp_port);
    
    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    pthread_mutex_unlock(&g_settings_mutex);
    return str;
}

int settings_apply_json_string(const char *json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (!json) return -1;

    pthread_mutex_lock(&g_settings_mutex);

    cJSON *item;

    item = cJSON_GetObjectItem(json, "steering_damping");
    if (cJSON_IsNumber(item)) g_settings.steering_damping = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "steering_damping_curve");
    if (cJSON_IsNumber(item)) g_settings.steering_damping_curve = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "udp_host");
    if (cJSON_IsString(item)) {
        strncpy(g_settings.udp_host, item->valuestring, sizeof(g_settings.udp_host) - 1);
        g_settings.udp_host[sizeof(g_settings.udp_host) - 1] = '\0';
    }

    item = cJSON_GetObjectItem(json, "udp_port");
    if (cJSON_IsNumber(item)) g_settings.udp_port = item->valueint;

    pthread_mutex_unlock(&g_settings_mutex);
    
    cJSON_Delete(json);
    settings_save();
    return 0;
}
