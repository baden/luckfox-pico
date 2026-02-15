#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <stdbool.h>
#include "drone_types.h"

// GPIO pin definitions for Luckfox Pico Pro
// Formula: pin = bank * 32 + (group * 8 + number)
// Group: A=0, B=1, C=2, D=3

// BEEPER                           | 26 | GPIO2_A2
#define BUZZER_PIN_BANK          2
#define BUZZER_PIN_GROUP         0
#define BUZZER_PIN_NUMBER        2
#define BUZZER_PIN_PATH          "/sys/class/gpio/gpio66"

// OLED-power                       | 27 | GPIO2_A3
#define OLEDVDD_PIN_BANK          2
#define OLEDVDD_PIN_GROUP         0
#define OLEDVDD_PIN_NUMBER        3
#define OLEDVDD_PIN_PATH          "/sys/class/gpio/gpio67"


// IO1 (лебідка вгору)              |  4 | GPIO1_C7
#define IO1_PIN_BANK             1
#define IO1_PIN_GROUP            2
#define IO1_PIN_NUMBER           7
#define IO1_PIN_PATH             "/sys/class/gpio/gpio55"

// IO2 (лебідка вниз)               |  5 | GPIO1_C6
#define IO2_PIN_BANK             1
#define IO2_PIN_GROUP            2
#define IO2_PIN_NUMBER           6
#define IO2_PIN_PATH             "/sys/class/gpio/gpio54"

// IO3 ()                           |  6 | GPIO1_C5
#define IO3_PIN_BANK             1
#define IO3_PIN_GROUP            2
#define IO3_PIN_NUMBER           5
#define IO3_PIN_PATH             "/sys/class/gpio/gpio53"

// IO4                              |  7 | GPIO1_C4
#define IO4_PIN_BANK             1
#define IO4_PIN_GROUP            2
#define IO4_PIN_NUMBER           4
#define IO4_PIN_PATH             "/sys/class/gpio/gpio52"

typedef struct {
    bool initialized;
} gpio_control_t;

// GPIO device structure
typedef struct {
    int pin_number;
    char path[256];
    bool active_low;
} gpio_pin_t;

// Initialize GPIO control
int gpio_control_init(gpio_control_t* gpio);

// Cleanup GPIO control
void gpio_control_cleanup(gpio_control_t* gpio);

// Initialize a specific GPIO pin
int gpio_pin_init(const gpio_pin_t* pin, bool output);

// Set GPIO pin value
int gpio_pin_set(const gpio_pin_t* pin, bool value);

// Get GPIO pin value
int gpio_pin_get(const gpio_pin_t* pin, bool* value);

// Control lebidka (winch)
int gpio_control_lebidka(gpio_control_t* gpio, lebidka_state_t state);

// Control aktuator (actuator)
int gpio_control_aktuator(gpio_control_t* gpio, aktuator_state_t state);

// Control buzzer
int gpio_control_buzzer(gpio_control_t* gpio, bool state);

// Control OLED VDD
int gpio_control_oledvdd(bool state);

// Buzzer patterns
typedef struct {
    bool* states;
    int count;
    double duration_ms;  // Duration for each state in milliseconds
} buzzer_pattern_t;

// Play buzzer pattern (non-blocking)
int gpio_buzzer_play_pattern(gpio_control_t* gpio, const buzzer_pattern_t* pattern);

// Stop buzzer
int gpio_buzzer_stop(gpio_control_t* gpio);

#endif // GPIO_CONTROL_H