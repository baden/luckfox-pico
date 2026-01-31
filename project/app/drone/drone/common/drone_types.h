#ifndef DRONE_TYPES_H
#define DRONE_TYPES_H

#include <stdbool.h>



// Common enums (moved from gpio_control.h to avoid circular dependencies)
typedef enum {
    LEBIDKA_STATE_NEUTRAL = 0,
    LEBIDKA_STATE_UP,
    LEBIDKA_STATE_DOWN,
    LEBIDKA_STATE_STOP
} lebidka_state_t;

typedef enum {
    AKTUATOR_STATE_NEUTRAL = 0,
    AKTUATOR_STATE_FORWARD,
    AKTUATOR_STATE_BACKWARD,
    AKTUATOR_STATE_STOP
} aktuator_state_t;

#endif // DRONE_TYPES_H