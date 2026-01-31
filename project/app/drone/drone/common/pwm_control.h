#ifndef PWM_CONTROL_H
#define PWM_CONTROL_H

#include <stdbool.h>
#include "drone_types.h"

// PWM configuration
#define PWM_FREQUENCY_HZ         50     // 50Hz for servos
#define PWM_PERIOD_NS            20000000 // 20ms in nanoseconds (1/50Hz)
#define PWM_MIN_DUTY_NS          500000   // 0.5ms in nanoseconds
#define PWM_MAX_DUTY_NS          2500000  // 2.5ms in nanoseconds
#define PWM_CENTER_DUTY_NS       1500000  // 1.5ms in nanoseconds (center)

// PWM chip and channel definitions for Luckfox Pico Pro
#define PWM_CHIP0_ID             5       // For servo1 (HPWM(5, 0))
#define PWM_CHIP1_ID             6       // For servo2 (HPWM(6, 0))
#define PWM_CHANNEL0_ID          0       // Channel 0
#define PWM_CHANNEL1_ID          0       // Channel 0

typedef struct {
    bool initialized;
    int servo1_chip;
    int servo1_channel;
    int servo2_chip;
    int servo2_channel;
} pwm_control_t;

typedef struct {
    float left_value;   // -1.0 to 1.0
    float right_value;  // -1.0 to 1.0
} servo_values_t;

// Initialize PWM control
int pwm_control_init(pwm_control_t* pwm);

// Cleanup PWM control
void pwm_control_cleanup(pwm_control_t* pwm);

// Set servo values (-1.0 to 1.0 range)
int pwm_control_set_servos(pwm_control_t* pwm, const servo_values_t* values);

// Set individual servo value
int pwm_control_set_servo1(pwm_control_t* pwm, float value);
int pwm_control_set_servo2(pwm_control_t* pwm, float value);

// Convert from -1.0..1.0 to duty cycle in nanoseconds
long pwm_value_to_duty_ns(float value);

// Enable/disable PWM channels
int pwm_control_enable(pwm_control_t* pwm, bool enable);

// Set PWM polarity
int pwm_control_set_polarity(pwm_control_t* pwm, const char* polarity);

// Helper functions
int pwm_export_channel(int chip_id, int channel_id);
int pwm_set_period(int chip_id, int channel_id, long period_ns);
int pwm_set_duty_cycle(int chip_id, int channel_id, long duty_ns);
int pwm_enable_channel(int chip_id, int channel_id, bool enable);
int pwm_set_polarity(int chip_id, int channel_id, const char* polarity);
int pwm_unexport_channel(int chip_id, int channel_id);

#endif // PWM_CONTROL_H