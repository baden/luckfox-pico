#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

#include "../../common/crsf.h"
#include "../../common/udp_client.h"
#include "../../common/gpio_control.h"
#include "../../common/pwm_control.h"
#include "../../common/drone_types.h"
#include "../../common/web_server.h"
#include "../../common/oled.h"
#include "../../common/network_monitor.h"
#include "../../common/settings.h"

// Global control state
typedef struct {
    float axis_0;           // Horizontal (-1.0..1.0)
    float axis_1;           // Vertical (-1.0..1.0)
    bool arm_state;         // ARM/DISARM state
    uint8_t flight_mode;    // Current MAVLink base_mode
    uint32_t custom_mode;   // Current MAVLink custom_mode
    lebidka_state_t lebidka_state;
    aktuator_state_t aktuator_state;
    pthread_mutex_t mutex;
    bool should_exit;
} control_state_t;

// Timing and connection state
typedef struct {
    double last_crsf_time;
    double last_udp_time;
    double last_web_time;
    double last_control_time;
    bool network_timeout;
    double arm_start_time;
} timing_state_t;

// Global variables
static control_state_t g_control = {0};
static timing_state_t g_timing = {0};

// Module instances
static crsf_t g_crsf = {0};
static udp_client_t g_udp = {0};
static gpio_control_t g_gpio = {0};
static pwm_control_t g_pwm = {0};
static web_server_t g_web = {0};

// Thread handles
static pthread_t oled_thread;
static pthread_t crsf_thread;
static pthread_t udp_thread;
static pthread_t web_thread;
static pthread_t control_thread;
static pthread_t watchdog_thread;

static volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
    g_control.should_exit = true;
}

static double get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Initialize all modules
static int init_modules(const char* udp_host, int udp_port) {
    printf("Initializing modules...\n");

    // Initialize default state
    // Default to MANUAL_INPUT_ENABLED (64)
    g_control.flight_mode = 64;
    g_control.custom_mode = 0;

    // Initialize CRSF
    if (crsf_init(&g_crsf, "/dev/ttyS3", 420000) != 0) {
        fprintf(stderr, "Failed to initialize CRSF\n");
        return -1;
    }

    // Initialize UDP client (MAVLink)
    if (udp_client_init(&g_udp, udp_host, udp_port) != 0) {
        fprintf(stderr, "Failed to initialize UDP client\n");
        printf("Continuing without UDP control\n");
    }

    // Initialize GPIO control
    if (gpio_control_init(&g_gpio) != 0) {
        fprintf(stderr, "Failed to initialize GPIO control\n");
        return -1;
    }

    // Initialize PWM control
    if (pwm_control_init(&g_pwm) != 0) {
        fprintf(stderr, "Failed to initialize PWM control\n");
        return -1;
    }

    // Initialize Web Server
    // Port 80, serving /oem/usr/share/drone/www
    if (web_server_init(&g_web, 80, "/oem/usr/share/drone/www") != 0) {
        fprintf(stderr, "Failed to initialize Web Server on port 80\n");
        printf("Continuing without Web control\n");
    }

    // Initialize mutex
    pthread_mutex_init(&g_control.mutex, NULL);

    // Initialize Network Monitor
    network_config_t net_config = {0};
    // Need to pass these from main args, but here we just copy what we have or defaults if not set in main (but main calls this).
    // Actually init_modules signature is fixed. I should probably init network monitor in main or pass config.
    // Let's modify main to handle the config struct.

    // For now, I'll modify init_modules to take the config if I can, OR just init it in main.
    // Let's init it in main() before creating threads, to keep init_modules clean or add it there.
    // init_modules is convenient. Let's stick to main() for network monitor since it has specific args.

    // Initialize OLED
    if (oled_init() != 0) {
        fprintf(stderr, "Failed to initialize OLED display\n");
        printf("Continuing without OLED display\n");
    }

    printf("All modules initialized successfully\n");
    return 0;
}


// Cleanup all modules
static void cleanup_modules(void) {
    printf("Cleaning up modules...\n");

    crsf_cleanup(&g_crsf);
    udp_client_cleanup(&g_udp);
    web_server_cleanup(&g_web);
    gpio_control_cleanup(&g_gpio);
    pwm_control_cleanup(&g_pwm);

    pthread_mutex_destroy(&g_control.mutex);
}

// Play buzzer pattern
static void play_buzzer_pattern(bool* states, int count, double duration_ms) {
    buzzer_pattern_t pattern = {
        .states = states,
        .count = count,
        .duration_ms = duration_ms
    };
    gpio_buzzer_play_pattern(&g_gpio, &pattern);
}

// OLED reading thread
static void* oled_thread_func(void* arg) {
    printf("OLED thread started\n");
    usleep(1000000); // 1sec
    while (!g_control.should_exit) {
        double now = get_time_seconds();

        network_state_t net_state;
        network_monitor_get_state(&net_state);

        pthread_mutex_lock(&g_control.mutex);
        oled_status_t status = {
            .armed = g_control.arm_state,
            .axis0 = g_control.axis_0,
            .axis1 = g_control.axis_1,
            .udp_connected = (now - g_timing.last_udp_time) < 10.0,
            .web_connected = g_web.connected,
            .crsf_connected = (now - g_timing.last_crsf_time) < 3.0,

            // Network mapping
            .eth_status = net_state.eth_status,
            .wg_status = net_state.wg_status,
            .op_connected = net_state.operator_ping,
            .dev1_ping = net_state.dev1_ping,
            .dev2_ping = net_state.dev2_ping,
            .dev3_ping = net_state.dev3_ping
        };
        pthread_mutex_unlock(&g_control.mutex);

        // Update OLED display every 100ms (10fps for smooth animation)
        oled_display(&status);
        usleep(100000); // 100ms
    }
    printf("OLED thread exiting\n");
    return NULL;
}

// CRSF reading thread
static void* crsf_thread_func(void* arg) {
    printf("CRSF thread started\n");

    double last_packet_timestamp = 0;
    double last_crsf_telem = 0;

    while (!g_control.should_exit) {
        if (!crsf_is_connected(&g_crsf)) {
            printf("CRSF disconnected, attempting to reconnect...\n");
            if (crsf_reconnect(&g_crsf) != 0) {
                sleep(2);
                continue;
            }
        }

        // Send Telemetry (1Hz)
        double now = get_time_seconds();
        if (now - last_crsf_telem >= 1.0) {
            last_crsf_telem = now;
            
            network_state_t net_state;
            network_monitor_get_state(&net_state);
            
            pthread_mutex_lock(&g_control.mutex);
            bool udp_conn = (now - g_timing.last_udp_time) < 5.0; // 5s timeout
            bool web_conn = g_web.connected;
            pthread_mutex_unlock(&g_control.mutex);
            
            char telem_str[32];
            // E:Eth, W:Wg, U:Udp, B:Browser(Web), ms:Ping
            // Eth: >=1 means Link Up. Wg: ==2 means Ping OK.
            snprintf(telem_str, sizeof(telem_str), "E%d W%d U%d B%d %dms",
                (net_state.eth_status >= 1),
                (net_state.wg_status == 2),
                udp_conn,
                web_conn,
                net_state.operator_latency_ms
            );
            
            crsf_send_telemetry_flight_mode(&g_crsf, telem_str);
        }

        if (crsf_process(&g_crsf) != 0) {
            // Error processing CRSF
            usleep(10000); // 10ms
            continue;
        }

        // Get latest channels
        crsf_channels_t channels;
        if (crsf_get_channels(&g_crsf, &channels) == 0) {
            // Only process if we have NEW data
            if (channels.timestamp > last_packet_timestamp) {
                last_packet_timestamp = channels.timestamp;

                pthread_mutex_lock(&g_control.mutex);

                // Channel 4 is ARM button (>0.5 = armed)
                bool new_arm_state = channels.values[4] > 0.5f;

                if (new_arm_state) {
                    g_control.axis_0 = channels.values[0];
                    g_control.axis_1 = channels.values[1];
                } else {
                    g_control.axis_0 = 0.0f;
                    g_control.axis_1 = 0.0f;
                }

                // Handle ARM state changes
                if (g_control.arm_state != new_arm_state) {
                    g_control.arm_state = new_arm_state;
                    if (new_arm_state) {
                        g_timing.arm_start_time = get_time_seconds();
                        play_buzzer_pattern((bool[]){true, false}, 2, 100);
                    } else {
                        play_buzzer_pattern((bool[]){true, false, true, false}, 4, 100);
                    }
                }

                // Update lebidka (channel 2)
                if (channels.values[2] < -0.5f) {
                    g_control.lebidka_state = LEBIDKA_STATE_UP;
                } else if (channels.values[2] > 0.5f) {
                    g_control.lebidka_state = LEBIDKA_STATE_DOWN;
                } else {
                    g_control.lebidka_state = LEBIDKA_STATE_NEUTRAL;
                }

                // Update aktuator (channel 3)
                if (channels.values[3] < -0.5f) {
                    g_control.aktuator_state = AKTUATOR_STATE_FORWARD;
                } else if (channels.values[3] > 0.5f) {
                    g_control.aktuator_state = AKTUATOR_STATE_BACKWARD;
                } else {
                    g_control.aktuator_state = AKTUATOR_STATE_NEUTRAL;
                }

                pthread_mutex_unlock(&g_control.mutex);

                // Update timing - ONLY when new packet arrived
                g_timing.last_crsf_time = get_time_seconds();
            }
        }

        usleep(5000); // 5ms (increased polling rate slightly)
    }

    printf("CRSF thread exiting\n");
    return NULL;
}

// UDP communication thread
static void* udp_thread_func(void* arg) {
    printf("UDP MAVLink thread started\n");

    double last_heartbeat = 0;
    double last_telemetry = 0;

    while (!g_control.should_exit) {
        if (!udp_client_is_connected(&g_udp)) {
            // printf("UDP disconnected, attempting to reconnect...\n");
            if (udp_client_reconnect(&g_udp) != 0) {
                sleep(5);
                continue;
            }
        }

        double now = get_time_seconds();

        // Send Heartbeat (1Hz)
        if (now - last_heartbeat >= 1.0) {
            pthread_mutex_lock(&g_control.mutex);
            bool is_armed = g_control.arm_state;
            uint8_t current_mode = g_control.flight_mode;
            uint32_t current_custom = g_control.custom_mode;
            pthread_mutex_unlock(&g_control.mutex);

            udp_client_send_heartbeat(&g_udp, is_armed, current_mode, current_custom);
            last_heartbeat = now;
        }

        // Send Telemetry (10Hz)
        if (now - last_telemetry >= 0.1) {
            pthread_mutex_lock(&g_control.mutex);
            float a0 = g_control.axis_0;
            float a1 = g_control.axis_1;
            int l_state = g_control.lebidka_state;
            int a_state = g_control.aktuator_state;
            pthread_mutex_unlock(&g_control.mutex);

            udp_client_send_telemetry(&g_udp, a0, a1, l_state, a_state);
            last_telemetry = now;
        }

        // Receive data
        udp_control_input_t input;
        int result = udp_client_receive(&g_udp, &input);

        if (result > 0) {
            pthread_mutex_lock(&g_control.mutex);

            double now = get_time_seconds();
            double time_since_crsf = now - g_timing.last_crsf_time;
            double time_since_web = now - g_timing.last_web_time;

            bool crsf_has_priority = time_since_crsf < 10.0;
            bool crsf_is_fresh = time_since_crsf < 1.0;
            bool web_has_priority = time_since_web < 2.0; // Web has 2s priority over UDP

            if (crsf_has_priority) {
                if (!crsf_is_fresh) {
                    // DEADZONE: RC was active recently, but signal is lost now.
                    // FAILSAFE: Force axes to neutral
                    if (g_control.arm_state) {
                         g_control.axis_0 = 0.0f;
                         g_control.axis_1 = 0.0f;
                    }
                }
            } else if (!web_has_priority) {
                // UDP takes over only if no CRSF and no Web

                // Handle Mode Change Requests
                if (input.cmd_set_mode) {
                    printf("UDP: Processing SET_MODE (Mode=%d, Custom=%d)\n",
                           input.target_mode, input.target_custom_mode);
                    g_control.flight_mode = input.target_mode;
                    g_control.custom_mode = input.target_custom_mode;

                    if (g_control.custom_mode == 9 && g_control.arm_state) {
                        printf("UDP: LAND mode detected. Simulating landing -> Disarming.\n");
                        g_control.arm_state = false;
                        play_buzzer_pattern((bool[]){true, false, true, false}, 4, 100);
                    }
                }

                // Update ARM state from Commands
                if (input.cmd_arm) {
                    if (!g_control.arm_state) {
                        g_control.arm_state = true;
                        play_buzzer_pattern((bool[]){true, false}, 2, 100);
                        printf("UDP: ARMED via MAVLink\n");
                    }
                }
                if (input.cmd_disarm) {
                    if (g_control.arm_state) {
                        g_control.arm_state = false;
                        play_buzzer_pattern((bool[]){true, false, true, false}, 4, 100);
                        printf("UDP: DISARMED via MAVLink\n");
                    }
                }

                // Update Axes
                if (input.valid && g_control.arm_state) {
                    // Map UDP axes to control axes (Swap Pitch/Roll)
                    g_control.axis_0 = input.axes[1]; // Roll
                    g_control.axis_1 = input.axes[0]; // Pitch
                } else if (!g_control.arm_state) {
                    g_control.axis_0 = 0.0f;
                    g_control.axis_1 = 0.0f;
                }
            }

            pthread_mutex_unlock(&g_control.mutex);

            g_timing.last_udp_time = get_time_seconds();
        }

        // Short sleep to prevent CPU hogging
        usleep(5000); // 5ms
    }

    printf("UDP thread exiting\n");
    return NULL;
}

// Web Server Thread
static void* web_thread_func(void* arg) {
    printf("Web Server thread started\n");

    double last_telemetry = 0;

    while (!g_control.should_exit) {

        web_control_input_t input = {0};
        web_server_run_step(&g_web, &input);

        double now = get_time_seconds();

        // Send telemetry (10Hz) to connected client
        if (now - last_telemetry >= 0.1) {
            pthread_mutex_lock(&g_control.mutex);
            float a0 = g_control.axis_0;
            float a1 = g_control.axis_1;
            float a2 = 0; // Throttle not tracked in control_state yet
            float a3 = 0; // Yaw not tracked
            bool armed = g_control.arm_state;
            pthread_mutex_unlock(&g_control.mutex);

            web_server_send_telemetry(&g_web, a0, a1, a3, a2, armed);
            last_telemetry = now;
        }

        if (input.valid) {
            
            // Check for Restart Command
            if (input.cmd_restart) {
                printf("Web: Restart command received. Exiting...\n");
                g_control.should_exit = true;
                running = 0;
            }

            pthread_mutex_lock(&g_control.mutex);

            double time_since_crsf = now - g_timing.last_crsf_time;
            bool crsf_has_priority = time_since_crsf < 10.0;
            bool crsf_is_fresh = time_since_crsf < 1.0;

            if (crsf_has_priority) {
                // Ignore Web, but if DEADZONE, handled by CRSF thread or UDP checks
                // Actually we should handle deadzone here too if we want robustness,
                // but CRSF/UDP threads check failsafe.
            } else {
                // Web has priority over UDP implicitly by being processed here and setting timestamp

                // Handle ARM/DISARM
                if (input.cmd_arm && !g_control.arm_state) {
                    g_control.arm_state = true;
                    play_buzzer_pattern((bool[]){true, false}, 2, 100);
                    printf("Web: ARMED\n");
                }
                if (input.cmd_disarm && g_control.arm_state) {
                    g_control.arm_state = false;
                    play_buzzer_pattern((bool[]){true, false, true, false}, 4, 100);
                    printf("Web: DISARMED\n");
                }

                // Axes
                if (g_control.arm_state) {
                    g_control.axis_0 = input.axes[0]; // Assuming Web sends Roll on 0
                    g_control.axis_1 = input.axes[1]; // Pitch on 1

                    // Aux
                    if (input.lebidka_val < 0) g_control.lebidka_state = LEBIDKA_STATE_UP;
                    else if (input.lebidka_val > 0) g_control.lebidka_state = LEBIDKA_STATE_DOWN;
                    else g_control.lebidka_state = LEBIDKA_STATE_NEUTRAL;

                    if (input.aktuator_val < 0) g_control.aktuator_state = AKTUATOR_STATE_FORWARD;
                    else if (input.aktuator_val > 0) g_control.aktuator_state = AKTUATOR_STATE_BACKWARD;
                    else g_control.aktuator_state = AKTUATOR_STATE_NEUTRAL;
                } else {
                    g_control.axis_0 = 0.0f;
                    g_control.axis_1 = 0.0f;
                }

                g_timing.last_web_time = now;
            }

            pthread_mutex_unlock(&g_control.mutex);
        }

        usleep(5000); // 5ms
    }

    printf("Web thread exiting\n");
    return NULL;
}

// Control update thread (20ms)
static void* control_thread_func(void* arg) {
    printf("Control thread started\n");

    static float prev_axis_0 = 0.0f;
    static float prev_axis_1 = 0.0f;
    static lebidka_state_t prev_lebidka = LEBIDKA_STATE_NEUTRAL;
    static aktuator_state_t prev_aktuator = AKTUATOR_STATE_NEUTRAL;

    while (!g_control.should_exit) {
        pthread_mutex_lock(&g_control.mutex);

        float current_axis_0 = g_control.axis_0;
        float current_axis_1 = g_control.axis_1;
        lebidka_state_t current_lebidka = g_control.lebidka_state;
        aktuator_state_t current_aktuator = g_control.aktuator_state;

        pthread_mutex_unlock(&g_control.mutex);

        // Apply settings
        float damping = settings_get_steering_damping();
        float curve = settings_get_steering_damping_curve();
        
        // Apply damping (0.0 = no damping/full steer, 1.0 = full damping/no steer)
        float steer_factor = 1.0f - damping;
        if (steer_factor < 0.0f) steer_factor = 0.0f;
        
        // Apply simple curve logic if needed (Expo)
        // For now just linear damping
        float effective_steering = current_axis_0 * steer_factor;

        // Calculate servo values: left = axis1 + axis0, right = axis1 - axis0
        // With damping: left = axis1 + effective_steering
        
        servo_values_t servo_vals = {
            .left_value = current_axis_1 + effective_steering,
            .right_value = current_axis_1 - effective_steering
        };

        // Clamp values to valid range
        if (servo_vals.left_value < -1.0f) servo_vals.left_value = -1.0f;
        if (servo_vals.left_value > 1.0f) servo_vals.left_value = 1.0f;
        if (servo_vals.right_value < -1.0f) servo_vals.right_value = -1.0f;
        if (servo_vals.right_value > 1.0f) servo_vals.right_value = 1.0f;

        pwm_control_set_servos(&g_pwm, &servo_vals);

        prev_axis_0 = current_axis_0;
        prev_axis_1 = current_axis_1;

        // Update GPIO devices if state changed

        if (current_lebidka != prev_lebidka) {
            gpio_control_lebidka(&g_gpio, current_lebidka);
            prev_lebidka = current_lebidka;
        }

        if (current_aktuator != prev_aktuator) {
            gpio_control_aktuator(&g_gpio, current_aktuator);
            prev_aktuator = current_aktuator;
        }

        usleep(20000); // 20ms
    }

    printf("Control thread exiting\n");
    return NULL;
}

// Watchdog thread for timeout handling
static void* watchdog_thread_func(void* arg) {
    printf("Watchdog thread started\n");

    while (!g_control.should_exit) {
        double current_time = get_time_seconds();

        // Determine last control time (max of CRSF, UDP, Web)
        double last_net = fmax(g_timing.last_udp_time, g_timing.last_web_time);
        g_timing.last_control_time = fmax(g_timing.last_crsf_time, last_net);

        // Check for timeout
        double time_since_last_control = current_time - g_timing.last_control_time;

        if (time_since_last_control > 1.5) {
            g_timing.network_timeout = true;

            pthread_mutex_lock(&g_control.mutex);

            // Gradually reduce axes to zero
            const float step = 0.02f / 1.0f; // 20ms update, 1 second to zero
            if (fabsf(g_control.axis_0) > 0.001f) {
                if (g_control.axis_0 > 0) {
                    g_control.axis_0 = fmaxf(0.0f, g_control.axis_0 - step);
                } else {
                    g_control.axis_0 = fminf(0.0f, g_control.axis_0 + step);
                }
            }
            if (fabsf(g_control.axis_1) > 0.001f) {
                if (g_control.axis_1 > 0) {
                    g_control.axis_1 = fmaxf(0.0f, g_control.axis_1 - step);
                } else {
                    g_control.axis_1 = fminf(0.0f, g_control.axis_1 + step);
                }
            }

            // Auto-disarm after 3 minutes
            if (time_since_last_control > 180.0 && g_control.arm_state) {
                g_control.arm_state = false;
                play_buzzer_pattern((bool[]){true, false, true, false}, 4, 100);
                printf("Auto-disarm due to timeout\n");
            }

            pthread_mutex_unlock(&g_control.mutex);
        } else {
            g_timing.network_timeout = false;
        }

        usleep(20000); // 20ms
    }

    printf("Watchdog thread exiting\n");
    return NULL;
}

int main(int argc, char *argv[])
{
    /* Setup signal handler for clean exit */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Set line buffering
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    // Initialize Settings Module
    settings_init();

    // Get initial settings
    char udp_host[64];
    settings_get_udp_host(udp_host, sizeof(udp_host));
    int udp_port = settings_get_udp_port();

    // Network Config Defaults
    network_config_t net_config;
    strcpy(net_config.eth_gateway, "192.168.2.1");
    strcpy(net_config.wg_gateway, "10.8.0.1");
    strcpy(net_config.operator_ip, "10.8.0.3");
    strcpy(net_config.dev1_ip, "10.0.7.101");
    strcpy(net_config.dev2_ip, "10.0.7.102");
    strcpy(net_config.dev3_ip, "10.0.7.103");
    
    // Check Env for Network Config overrides
    char *env_val;
    if ((env_val = getenv("DRONE_ETH_GW"))) strncpy(net_config.eth_gateway, env_val, sizeof(net_config.eth_gateway)-1);
    if ((env_val = getenv("DRONE_WG_GW"))) strncpy(net_config.wg_gateway, env_val, sizeof(net_config.wg_gateway)-1);
    if ((env_val = getenv("DRONE_OP_IP"))) strncpy(net_config.operator_ip, env_val, sizeof(net_config.operator_ip)-1);

    printf("Starting drone C application (MAVLink + Web enabled)...\n");
    printf("UDP Server: %s:%d\n", udp_host, udp_port);
    printf("Net Monitor: EthGW=%s, WgGW=%s, Op=%s\n",
           net_config.eth_gateway, net_config.wg_gateway, net_config.operator_ip);

    if (init_modules(udp_host, udp_port) != 0) {
        fprintf(stderr, "Failed to initialize modules\n");
        return 1;
    }

    if (network_monitor_init(&net_config) != 0) {
        fprintf(stderr, "Failed to initialize Network Monitor\n");
    }

    printf("Starting threads...\n");

    // Create threads
    if (pthread_create(&oled_thread, NULL, oled_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create OLED thread\n");
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&crsf_thread, NULL, crsf_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create CRSF thread\n");
        g_control.should_exit = true;
        pthread_join(oled_thread, NULL);
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&udp_thread, NULL, udp_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create UDP thread\n");
        g_control.should_exit = true;
        pthread_join(oled_thread, NULL);
        pthread_join(crsf_thread, NULL);
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&web_thread, NULL, web_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create Web thread\n");
        g_control.should_exit = true;
        pthread_join(oled_thread, NULL);
        pthread_join(crsf_thread, NULL);
        pthread_join(udp_thread, NULL);
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&control_thread, NULL, control_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create control thread\n");
        g_control.should_exit = true;
        pthread_join(oled_thread, NULL);
        pthread_join(crsf_thread, NULL);
        pthread_join(udp_thread, NULL);
        pthread_join(web_thread, NULL);
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&watchdog_thread, NULL, watchdog_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create watchdog thread\n");
        g_control.should_exit = true;
        pthread_join(oled_thread, NULL);
        pthread_join(crsf_thread, NULL);
        pthread_join(udp_thread, NULL);
        pthread_join(web_thread, NULL);
        pthread_join(control_thread, NULL);
        cleanup_modules();
        return 1;
    }

    printf("All threads started. Press Ctrl+C to exit\n");

    // Play startup sound
    play_buzzer_pattern((bool[]){true, false}, 2, 100);

    // Keep track of current settings to detect changes
    char current_host[64];
    strncpy(current_host, udp_host, sizeof(current_host));
    int current_port = udp_port;

    // Main loop
    while (running) {
        sleep(1);

        // Check for settings changes (Dynamic Reconfiguration)
        char new_host[64];
        settings_get_udp_host(new_host, sizeof(new_host));
        int new_port = settings_get_udp_port();
        
        if (strcmp(new_host, current_host) != 0 || new_port != current_port) {
            printf("Settings changed: Reconnecting UDP to %s:%d\n", new_host, new_port);
            
            // Reconnect UDP
            udp_client_cleanup(&g_udp); 
            udp_client_init(&g_udp, new_host, new_port);
            
            // Update tracking
            strncpy(current_host, new_host, sizeof(current_host));
            current_port = new_port;
        }

        // Print status every 10 seconds
        static int counter = 0;
        if (++counter >= 10) {
            counter = 0;

            // Get current ARM state from heartbeat (actual state sent to QGC)
            bool current_heartbeat_arm_state = false;
            if (udp_client_is_connected(&g_udp)) {
                // The heartbeat sends the actual ARM state
                current_heartbeat_arm_state = g_control.arm_state;
            }

            pthread_mutex_lock(&g_control.mutex);
            printf("Status: ARM=%s, Axis0=%.2f, Axis1=%.2f, Lebidka=%d, Aktuator=%d, Damping=%.2f\n",
                   current_heartbeat_arm_state ? "ON" : "OFF",
                   g_control.axis_0, g_control.axis_1,
                   g_control.lebidka_state, g_control.aktuator_state,
                   settings_get_steering_damping());
            pthread_mutex_unlock(&g_control.mutex);

            printf("Connections: CRSF=%s, UDP=%s, Web=%s, Host=%s:%d\n",
                   crsf_is_connected(&g_crsf) ? "OK" : "DISCONNECTED",
                   udp_client_is_connected(&g_udp) ? "OK" : "DISCONNECTED",
                   g_web.connected ? "CONNECTED" : "WAITING",
                   current_host, current_port);
        }
    }

    printf("\nShutting down...\n");

    // Signal threads to exit
    g_control.should_exit = true;

    // Wait for all threads to finish
    pthread_join(oled_thread, NULL);
    pthread_join(crsf_thread, NULL);
    pthread_join(udp_thread, NULL);
    pthread_join(web_thread, NULL);
    pthread_join(control_thread, NULL);
    pthread_join(watchdog_thread, NULL);

    // Cleanup
    cleanup_modules();
    network_monitor_cleanup();

    printf("Shutdown complete\n");
    return 0;
}
