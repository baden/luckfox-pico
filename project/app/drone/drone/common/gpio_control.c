#include "gpio_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

// Global GPIO pins
static gpio_pin_t buzzer_pin = {
    .pin_number = 82,  // GPIO2_A2 = 2*32 + (0*8 + 2) = 64 + 2 = 66? Wait, let me recalc
    .path = BUZZER_PIN_PATH,
    .active_low = false
};

static gpio_pin_t io1_pin = {
    .pin_number = 71,  // GPIO1_C7 = 1*32 + (2*8 + 7) = 32 + 23 = 55? Let me recalc
    .path = IO1_PIN_PATH,
    .active_low = true   // Active level is LOW
};

static gpio_pin_t io2_pin = {
    .pin_number = 70,   // GPIO1_C6 = 1*32 + (2*8 + 6) = 32 + 22 = 54? Let me recalc
    .path = IO2_PIN_PATH,
    .active_low = true   // Active level is LOW
};

static gpio_pin_t io3_pin = {
    .pin_number = 69,   // GPIO1_C5 = 1*32 + (2*8 + 5) = 32 + 21 = 53? Let me recalc
    .path = IO3_PIN_PATH,
    .active_low = true   // Active level is LOW
};

static gpio_pin_t io4_pin = {
    .pin_number = 68,   // GPIO1_C4 = 1*32 + (2*8 + 4) = 32 + 20 = 52? Let me recalc
    .path = IO4_PIN_PATH,
    .active_low = true   // Active level is LOW
};

// Recalculate pin numbers properly:
// Formula: pin = bank * 32 + (group * 8 + X)
// GPIO2_A2: bank=2, group=0, X=2 -> pin = 2*32 + (0*8 + 2) = 64 + 2 = 66
// GPIO1_C7: bank=1, group=2, X=7 -> pin = 1*32 + (2*8 + 7) = 32 + 23 = 55
// GPIO1_C6: bank=1, group=2, X=6 -> pin = 1*32 + (2*8 + 6) = 32 + 22 = 54
// GPIO1_C5: bank=1, group=2, X=5 -> pin = 1*32 + (2*8 + 5) = 32 + 21 = 53
// GPIO1_C4: bank=1, group=2, X=4 -> pin = 1*32 + (2*8 + 4) = 32 + 20 = 52

// Buzzer control thread data
typedef struct {
    bool running;
    bool pattern_active;
    buzzer_pattern_t current_pattern;
    int current_step;
    pthread_t thread;
    pthread_mutex_t mutex;
} buzzer_thread_data_t;

static buzzer_thread_data_t buzzer_data = {0};

// GPIO pin calculation (using the formula from Python code)
static int calculate_gpio_pin(int bank, int group, int number) {
    return bank * 32 + (group * 8 + number);
}

static void correct_pin_numbers(void) {
    buzzer_pin.pin_number = calculate_gpio_pin(BUZZER_PIN_BANK, BUZZER_PIN_GROUP, BUZZER_PIN_NUMBER);
    io1_pin.pin_number = calculate_gpio_pin(IO1_PIN_BANK, IO1_PIN_GROUP, IO1_PIN_NUMBER);
    io2_pin.pin_number = calculate_gpio_pin(IO2_PIN_BANK, IO2_PIN_GROUP, IO2_PIN_NUMBER);
    io3_pin.pin_number = calculate_gpio_pin(IO3_PIN_BANK, IO3_PIN_GROUP, IO3_PIN_NUMBER);
    io4_pin.pin_number = calculate_gpio_pin(IO4_PIN_BANK, IO4_PIN_GROUP, IO4_PIN_NUMBER);
    
    printf("GPIO: Pin numbers - Buzzer:%d, IO1:%d, IO2:%d, IO3:%d, IO4:%d\n",
           buzzer_pin.pin_number, io1_pin.pin_number, io2_pin.pin_number, 
           io3_pin.pin_number, io4_pin.pin_number);
}

static int gpio_export(int pin_number) {
    char buffer[64];
    int fd;
    
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        // Pin might already be exported
        return 0;
    }
    
    snprintf(buffer, sizeof(buffer), "%d", pin_number);
    ssize_t written = write(fd, buffer, strlen(buffer));
    close(fd);
    
    return (written > 0) ? 0 : -1;
}

static int gpio_set_direction(int pin_number, bool output) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin_number);
    
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("open direction");
        return -1;
    }
    
    const char* dir = output ? "out" : "in";
    ssize_t written = write(fd, dir, strlen(dir));
    close(fd);
    
    return (written > 0) ? 0 : -1;
}

static int gpio_write_value(int pin_number, bool value) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin_number);
    
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("open value");
        return -1;
    }
    
    const char* val = value ? "1" : "0";
    ssize_t written = write(fd, val, strlen(val));
    close(fd);
    
    return (written > 0) ? 0 : -1;
}

static int gpio_read_value(int pin_number, bool* value) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin_number);
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open value for read");
        return -1;
    }
    
    char buffer[8];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        *value = (buffer[0] == '1');
        return 0;
    }
    
    return -1;
}

// Buzzer thread function
static void* buzzer_thread_func(void* arg) {
    buzzer_thread_data_t* data = (buzzer_thread_data_t*)arg;
    
    while (data->running) {
        pthread_mutex_lock(&data->mutex);
        
        if (data->pattern_active) {
            // Set buzzer state for current step
            bool current_state = data->current_pattern.states[data->current_step];
            gpio_write_value(buzzer_pin.pin_number, current_state);
            
            // Move to next step
            data->current_step++;
            if (data->current_step >= data->current_pattern.count) {
                // Pattern finished
                data->pattern_active = false;
                gpio_write_value(buzzer_pin.pin_number, false); // Turn off buzzer
            }
        }
        
        pthread_mutex_unlock(&data->mutex);
        
        // Sleep for pattern duration
        usleep((int)(data->current_pattern.duration_ms * 1000));
    }
    
    return NULL;
}

int gpio_control_init(gpio_control_t* gpio) {
    if (!gpio) {
        return -1;
    }
    
    memset(gpio, 0, sizeof(gpio_control_t));
    
    // Correct pin numbers using the formula
    correct_pin_numbers();
    
    // Export GPIO pins
    gpio_export(buzzer_pin.pin_number);
    gpio_export(io1_pin.pin_number);
    gpio_export(io2_pin.pin_number);
    gpio_export(io3_pin.pin_number);
    gpio_export(io4_pin.pin_number);
    
    // Give some time for the system to create the sysfs entries
    usleep(100000); // 100ms
    
    // Set directions
    gpio_set_direction(buzzer_pin.pin_number, true);
    gpio_set_direction(io1_pin.pin_number, true);
    gpio_set_direction(io2_pin.pin_number, true);
    gpio_set_direction(io3_pin.pin_number, true);
    gpio_set_direction(io4_pin.pin_number, true);
    
    // Initialize all pins to inactive state
    gpio_write_value(buzzer_pin.pin_number, false);
    gpio_write_value(io1_pin.pin_number, true);  // Active low, so inactive = true
    gpio_write_value(io2_pin.pin_number, true);  // Active low, so inactive = true
    gpio_write_value(io3_pin.pin_number, true);  // Active low, so inactive = true
    gpio_write_value(io4_pin.pin_number, true);  // Active low, so inactive = true
    
    // Initialize buzzer thread
    pthread_mutex_init(&buzzer_data.mutex, NULL);
    buzzer_data.running = true;
    buzzer_data.current_pattern.duration_ms = 100; // Default duration
    
    if (pthread_create(&buzzer_data.thread, NULL, buzzer_thread_func, &buzzer_data) != 0) {
        perror("pthread_create buzzer");
        return -1;
    }
    
    gpio->initialized = true;
    printf("GPIO: Initialized control system\n");
    return 0;
}

void gpio_control_cleanup(gpio_control_t* gpio) {
    if (!gpio || !gpio->initialized) {
        return;
    }
    
    // Stop buzzer thread
    pthread_mutex_lock(&buzzer_data.mutex);
    buzzer_data.running = false;
    buzzer_data.pattern_active = false;
    pthread_mutex_unlock(&buzzer_data.mutex);
    
    pthread_join(buzzer_data.thread, NULL);
    pthread_mutex_destroy(&buzzer_data.mutex);
    
    // Turn off all outputs
    gpio_write_value(buzzer_pin.pin_number, false);
    gpio_write_value(io1_pin.pin_number, true);  // Active low, so inactive = true
    gpio_write_value(io2_pin.pin_number, true);  // Active low, so inactive = true
    gpio_write_value(io3_pin.pin_number, true);  // Active low, so inactive = true
    gpio_write_value(io4_pin.pin_number, true);  // Active low, so inactive = true
    
    gpio->initialized = false;
    printf("GPIO: Cleanup completed\n");
}

int gpio_control_lebidka(gpio_control_t* gpio, lebidka_state_t state) {
    if (!gpio || !gpio->initialized) {
        return -1;
    }
    
    switch (state) {
        case LEBIDKA_STATE_UP:
            // IO1=0 (up), IO2=1 (stop down)
            gpio_write_value(io1_pin.pin_number, false);
            gpio_write_value(io2_pin.pin_number, true);
            break;
            
        case LEBIDKA_STATE_DOWN:
            // IO1=1 (stop up), IO2=0 (down)
            gpio_write_value(io1_pin.pin_number, true);
            gpio_write_value(io2_pin.pin_number, false);
            break;
            
        case LEBIDKA_STATE_NEUTRAL:
        case LEBIDKA_STATE_STOP:
        default:
            // IO1=1 (stop up), IO2=1 (stop down)
            gpio_write_value(io1_pin.pin_number, true);
            gpio_write_value(io2_pin.pin_number, true);
            break;
    }
    
    printf("GPIO: Lebidka set to state %d\n", state);
    return 0;
}

int gpio_control_aktuator(gpio_control_t* gpio, aktuator_state_t state) {
    if (!gpio || !gpio->initialized) {
        return -1;
    }
    
    switch (state) {
        case AKTUATOR_STATE_FORWARD:
            // IO3=0 (forward), IO4=1 (stop backward)
            gpio_write_value(io3_pin.pin_number, false);
            gpio_write_value(io4_pin.pin_number, true);
            break;
            
        case AKTUATOR_STATE_BACKWARD:
            // IO3=1 (stop forward), IO4=0 (backward)
            gpio_write_value(io3_pin.pin_number, true);
            gpio_write_value(io4_pin.pin_number, false);
            break;
            
        case AKTUATOR_STATE_NEUTRAL:
        case AKTUATOR_STATE_STOP:
        default:
            // IO3=1 (stop forward), IO4=1 (stop backward)
            gpio_write_value(io3_pin.pin_number, true);
            gpio_write_value(io4_pin.pin_number, true);
            break;
    }
    
    printf("GPIO: Aktuator set to state %d\n", state);
    return 0;
}

int gpio_control_buzzer(gpio_control_t* gpio, bool state) {
    if (!gpio || !gpio->initialized) {
        return -1;
    }
    
    return gpio_write_value(buzzer_pin.pin_number, state);
}

int gpio_buzzer_play_pattern(gpio_control_t* gpio, const buzzer_pattern_t* pattern) {
    if (!gpio || !gpio->initialized || !pattern || !pattern->states || pattern->count <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(&buzzer_data.mutex);
    
    // Copy pattern data
    if (buzzer_data.current_pattern.states) {
        free(buzzer_data.current_pattern.states);
    }
    
    buzzer_data.current_pattern.states = malloc(pattern->count * sizeof(bool));
    if (!buzzer_data.current_pattern.states) {
        pthread_mutex_unlock(&buzzer_data.mutex);
        return -1;
    }
    
    memcpy(buzzer_data.current_pattern.states, pattern->states, pattern->count * sizeof(bool));
    buzzer_data.current_pattern.count = pattern->count;
    buzzer_data.current_pattern.duration_ms = pattern->duration_ms;
    buzzer_data.current_step = 0;
    buzzer_data.pattern_active = true;
    
    pthread_mutex_unlock(&buzzer_data.mutex);
    
    printf("GPIO: Playing buzzer pattern with %d steps\n", pattern->count);
    return 0;
}

int gpio_buzzer_stop(gpio_control_t* gpio) {
    if (!gpio || !gpio->initialized) {
        return -1;
    }
    
    pthread_mutex_lock(&buzzer_data.mutex);
    buzzer_data.pattern_active = false;
    buzzer_data.current_step = 0;
    pthread_mutex_unlock(&buzzer_data.mutex);
    
    gpio_write_value(buzzer_pin.pin_number, false);
    return 0;
}