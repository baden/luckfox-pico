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

// Global control state
typedef struct {
    float axis_0;           // Horizontal (-1.0..1.0)
    float axis_1;           // Vertical (-1.0..1.0)
    bool arm_state;         // ARM/DISARM state
    lebidka_state_t lebidka_state;
    aktuator_state_t aktuator_state;
    pthread_mutex_t mutex;
    bool should_exit;
} control_state_t;

// Timing and connection state
typedef struct {
    double last_crsf_time;
    double last_udp_time;
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

// Thread handles
static pthread_t crsf_thread;
static pthread_t udp_thread;
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
static int init_modules(void) {
    printf("Initializing modules...\n");

    // Initialize CRSF
    if (crsf_init(&g_crsf, "/dev/ttyS3", 420000) != 0) {
        fprintf(stderr, "Failed to initialize CRSF\n");
        return -1;
    }

    // Initialize UDP client
    if (udp_client_init(&g_udp, NULL) != 0) {
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

    // Initialize mutex
    pthread_mutex_init(&g_control.mutex, NULL);

    printf("All modules initialized successfully\n");
    return 0;
}

// Cleanup all modules
static void cleanup_modules(void) {
    printf("Cleaning up modules...\n");

    crsf_cleanup(&g_crsf);
    udp_client_cleanup(&g_udp);
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

// CRSF reading thread
static void* crsf_thread_func(void* arg) {
    printf("CRSF thread started\n");

    while (!g_control.should_exit) {
        if (!crsf_is_connected(&g_crsf)) {
            printf("CRSF disconnected, attempting to reconnect...\n");
            if (crsf_reconnect(&g_crsf) != 0) {
                sleep(2);
                continue;
            }
        }

        if (crsf_process(&g_crsf) != 0) {
            // Error processing CRSF
            usleep(10000); // 10ms
            continue;
        }

        // Get latest channels
        crsf_channels_t channels;
        if (crsf_get_channels(&g_crsf, &channels) == 0) {
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

            // Update timing
            g_timing.last_crsf_time = get_time_seconds();
        }

        usleep(20000); // 20ms
    }

    printf("CRSF thread exiting\n");
    return NULL;
}

// UDP communication thread
static void* udp_thread_func(void* arg) {
    printf("UDP thread started\n");

    while (!g_control.should_exit) {
        if (!udp_client_is_connected(&g_udp)) {
            printf("UDP disconnected, attempting to reconnect...\n");
            if (udp_client_reconnect(&g_udp) != 0) {
                sleep(5);
                continue;
            }
        }

        // Send keep-alive
        udp_client_send_keep_alive(&g_udp);

        // Receive data
        udp_packet_t packet;
        int result = udp_client_receive(&g_udp, &packet, 1.0);

        if (result > 0) {
            pthread_mutex_lock(&g_control.mutex);

            // Check if CRSF has priority (last CRSF data < 5 seconds ago)
            bool crsf_has_priority = (get_time_seconds() - g_timing.last_crsf_time) < 5.0;

            if (!crsf_has_priority) {
                switch (packet.type) {
                    case UDP_COMMAND_JOY_UPDATE:
                        if (packet.data.joystick.valid) {
                            // Update ARM state
                            bool new_arm_state = packet.data.joystick.buttons[0] == 1;
                            if (g_control.arm_state != new_arm_state) {
                                g_control.arm_state = new_arm_state;
                                if (new_arm_state) {
                                    play_buzzer_pattern((bool[]){true, false}, 2, 100);
                                } else {
                                    play_buzzer_pattern((bool[]){true, false, true, false}, 4, 100);
                                }
                            }

                            if (new_arm_state) {
                                g_control.axis_0 = packet.data.joystick.axes[0];
                                g_control.axis_1 = packet.data.joystick.axes[1];
                            } else {
                                g_control.axis_0 = 0.0f;
                                g_control.axis_1 = 0.0f;
                            }

                            // Update lebidka from axis[2]
                            if (packet.data.joystick.axes[2] < -0.5f) {
                                g_control.lebidka_state = LEBIDKA_STATE_UP;
                            } else if (packet.data.joystick.axes[2] > 0.5f) {
                                g_control.lebidka_state = LEBIDKA_STATE_DOWN;
                            } else {
                                g_control.lebidka_state = LEBIDKA_STATE_NEUTRAL;
                            }

                            // Update aktuator from axis[3]
                            if (packet.data.joystick.axes[3] < -0.5f) {
                                g_control.aktuator_state = AKTUATOR_STATE_FORWARD;
                            } else if (packet.data.joystick.axes[3] > 0.5f) {
                                g_control.aktuator_state = AKTUATOR_STATE_BACKWARD;
                            } else {
                                g_control.aktuator_state = AKTUATOR_STATE_NEUTRAL;
                            }
                        }
                        break;

                    case UDP_COMMAND_RESTART:
                        printf("Received restart command via UDP\n");
                        g_control.should_exit = true;
                        break;

                    default:
                        break;
                }
            }

            pthread_mutex_unlock(&g_control.mutex);

            g_timing.last_udp_time = get_time_seconds();
        }

        usleep(100000); // 100ms
    }

    printf("UDP thread exiting\n");
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

        // Update servos if axes changed significantly
        if (fabsf(current_axis_0 - prev_axis_0) > 0.01f || fabsf(current_axis_1 - prev_axis_1) > 0.01f) {
            // Calculate servo values: left = axis0 + axis1, right = axis1 - axis0
            servo_values_t servo_vals = {
                .left_value = current_axis_0 + current_axis_1,
                .right_value = current_axis_1 - current_axis_0
            };

            // Clamp values to valid range
            if (servo_vals.left_value < -1.0f) servo_vals.left_value = -1.0f;
            if (servo_vals.left_value > 1.0f) servo_vals.left_value = 1.0f;
            if (servo_vals.right_value < -1.0f) servo_vals.right_value = -1.0f;
            if (servo_vals.right_value > 1.0f) servo_vals.right_value = 1.0f;

            pwm_control_set_servos(&g_pwm, &servo_vals);

            prev_axis_0 = current_axis_0;
            prev_axis_1 = current_axis_1;
        }

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

        // Determine last control time
        g_timing.last_control_time = fmax(g_timing.last_crsf_time, g_timing.last_udp_time);

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

    printf("Starting drone C application...\n");

    if (init_modules() != 0) {
        fprintf(stderr, "Failed to initialize modules\n");
        return 1;
    }

    printf("Starting threads...\n");

    // Create threads
    if (pthread_create(&crsf_thread, NULL, crsf_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create CRSF thread\n");
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&udp_thread, NULL, udp_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create UDP thread\n");
        g_control.should_exit = true;
        pthread_join(crsf_thread, NULL);
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&control_thread, NULL, control_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create control thread\n");
        g_control.should_exit = true;
        pthread_join(crsf_thread, NULL);
        pthread_join(udp_thread, NULL);
        cleanup_modules();
        return 1;
    }

    if (pthread_create(&watchdog_thread, NULL, watchdog_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create watchdog thread\n");
        g_control.should_exit = true;
        pthread_join(crsf_thread, NULL);
        pthread_join(udp_thread, NULL);
        pthread_join(control_thread, NULL);
        cleanup_modules();
        return 1;
    }

    printf("All threads started. Press Ctrl+C to exit\n");

    // Play startup sound
    play_buzzer_pattern((bool[]){true, false}, 2, 100);

    // Main loop - just wait for exit signal
    while (running) {
        sleep(1);

        // Print status every 10 seconds
        static int counter = 0;
        if (++counter >= 10) {
            counter = 0;

            pthread_mutex_lock(&g_control.mutex);
            printf("Status: ARM=%s, Axis0=%.2f, Axis1=%.2f, Lebidka=%d, Aktuator=%d\n",
                   g_control.arm_state ? "ON" : "OFF",
                   g_control.axis_0, g_control.axis_1,
                   g_control.lebidka_state, g_control.aktuator_state);
            pthread_mutex_unlock(&g_control.mutex);

            printf("Connections: CRSF=%s, UDP=%s\n",
                   crsf_is_connected(&g_crsf) ? "OK" : "DISCONNECTED",
                   udp_client_is_connected(&g_udp) ? "OK" : "DISCONNECTED");
        }
    }

    printf("\nShutting down...\n");

    // Signal threads to exit
    g_control.should_exit = true;

    // Wait for all threads to finish
    pthread_join(crsf_thread, NULL);
    pthread_join(udp_thread, NULL);
    pthread_join(control_thread, NULL);
    pthread_join(watchdog_thread, NULL);

    // Cleanup
    cleanup_modules();

    printf("Shutdown complete\n");
    return 0;
}
