#include "pwm_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// Helper function to write to sysfs file
static int sysfs_write(const char* path, const char* value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        // Try again after a short delay (sometimes sysfs needs time)
        usleep(10000); // 10ms
        fd = open(path, O_WRONLY);
        if (fd < 0) {
            fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
            return -1;
        }
    }
    
    ssize_t written = write(fd, value, strlen(value));
    close(fd);
    
    if (written < 0) {
        fprintf(stderr, "Failed to write to %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    return 0;
}

// Helper function to read from sysfs file
static int sysfs_read(const char* path, char* buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s for reading: %s\n", path, strerror(errno));
        return -1;
    }
    
    ssize_t bytes_read = read(fd, buffer, size - 1);
    close(fd);
    
    if (bytes_read < 0) {
        fprintf(stderr, "Failed to read from %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    return bytes_read;
}

int pwm_export_channel(int chip_id, int channel_id) {
    char export_path[256];
    snprintf(export_path, sizeof(export_path), "/sys/class/pwm/pwmchip%d/export", chip_id);
    
    char channel_str[16];
    snprintf(channel_str, sizeof(channel_str), "%d", channel_id);
    
    int result = sysfs_write(export_path, channel_str);
    if (result < 0) {
        // Channel might already be exported, that's ok
        printf("PWM: Channel %d on chip %d might already be exported\n", channel_id, chip_id);
        return 0;
    }
    
    printf("PWM: Exported channel %d on chip %d\n", channel_id, chip_id);
    
    // Give sysfs time to create the directory
    usleep(100000); // 100ms
    
    return 0;
}

int pwm_unexport_channel(int chip_id, int channel_id) {
    char unexport_path[256];
    snprintf(unexport_path, sizeof(unexport_path), "/sys/class/pwm/pwmchip%d/unexport", chip_id);
    
    char channel_str[16];
    snprintf(channel_str, sizeof(channel_str), "%d", channel_id);
    
    int result = sysfs_write(unexport_path, channel_str);
    if (result < 0) {
        // Channel might not be exported, that's ok
        return 0;
    }
    
    printf("PWM: Unexported channel %d on chip %d\n", channel_id, chip_id);
    return 0;
}

int pwm_set_period(int chip_id, int channel_id, long period_ns) {
    char period_path[256];
    snprintf(period_path, sizeof(period_path), "/sys/class/pwm/pwmchip%d/pwm%d/period", 
             chip_id, channel_id);
    
    char period_str[32];
    snprintf(period_str, sizeof(period_str), "%ld", period_ns);
    
    return sysfs_write(period_path, period_str);
}

int pwm_set_duty_cycle(int chip_id, int channel_id, long duty_ns) {
    char duty_path[256];
    snprintf(duty_path, sizeof(duty_path), "/sys/class/pwm/pwmchip%d/pwm%d/duty_cycle", 
             chip_id, channel_id);
    
    char duty_str[32];
    snprintf(duty_str, sizeof(duty_str), "%ld", duty_ns);
    
    return sysfs_write(duty_path, duty_str);
}

int pwm_enable_channel(int chip_id, int channel_id, bool enable) {
    char enable_path[256];
    snprintf(enable_path, sizeof(enable_path), "/sys/class/pwm/pwmchip%d/pwm%d/enable", 
             chip_id, channel_id);
    
    const char* enable_str = enable ? "1" : "0";
    
    return sysfs_write(enable_path, enable_str);
}

int pwm_set_polarity(int chip_id, int channel_id, const char* polarity) {
    char polarity_path[256];
    snprintf(polarity_path, sizeof(polarity_path), "/sys/class/pwm/pwmchip%d/pwm%d/polarity", 
             chip_id, channel_id);
    
    return sysfs_write(polarity_path, polarity);
}

long pwm_value_to_duty_ns(float value) {
    // Clamp value to -1.0..1.0 range
    if (value < -1.0f) value = -1.0f;
    if (value > 1.0f) value = 1.0f;
    
    // Convert: -1.0 -> 0.5ms, 0.0 -> 1.5ms, 1.0 -> 2.5ms
    // Formula: duty_ns = center_duty + value * (max_duty - center_duty)
    long duty_ns = (long)(PWM_CENTER_DUTY_NS + value * (PWM_MAX_DUTY_NS - PWM_CENTER_DUTY_NS));
    
    // Clamp to valid range
    if (duty_ns < PWM_MIN_DUTY_NS) duty_ns = PWM_MIN_DUTY_NS;
    if (duty_ns > PWM_MAX_DUTY_NS) duty_ns = PWM_MAX_DUTY_NS;
    
    return duty_ns;
}

static int pwm_init_channel(int chip_id, int channel_id) {
    // Export the PWM channel
    if (pwm_export_channel(chip_id, channel_id) != 0) {
        return -1;
    }
    
    // Set frequency (period)
    if (pwm_set_period(chip_id, channel_id, PWM_PERIOD_NS) != 0) {
        return -1;
    }
    
    // Set polarity to normal
    if (pwm_set_polarity(chip_id, channel_id, "normal") != 0) {
        return -1;
    }
    
    // Set initial duty cycle to center position
    if (pwm_set_duty_cycle(chip_id, channel_id, PWM_CENTER_DUTY_NS) != 0) {
        return -1;
    }
    
    // Enable the PWM channel
    if (pwm_enable_channel(chip_id, channel_id, true) != 0) {
        return -1;
    }
    
    return 0;
}

static int pwm_cleanup_channel(int chip_id, int channel_id) {
    // Disable the PWM channel
    pwm_enable_channel(chip_id, channel_id, false);
    
    // Unexport the PWM channel
    pwm_unexport_channel(chip_id, channel_id);
    
    return 0;
}

int pwm_control_init(pwm_control_t* pwm) {
    if (!pwm) {
        return -1;
    }
    
    memset(pwm, 0, sizeof(pwm_control_t));
    pwm->servo1_chip = PWM_CHIP0_ID;
    pwm->servo1_channel = PWM_CHANNEL0_ID;
    pwm->servo2_chip = PWM_CHIP1_ID;
    pwm->servo2_channel = PWM_CHANNEL1_ID;
    
    printf("PWM: Initializing servo control...\n");
    
    // Initialize servo1
    if (pwm_init_channel(pwm->servo1_chip, pwm->servo1_channel) != 0) {
        fprintf(stderr, "PWM: Failed to initialize servo1\n");
        return -1;
    }
    
    // Initialize servo2
    if (pwm_init_channel(pwm->servo2_chip, pwm->servo2_channel) != 0) {
        fprintf(stderr, "PWM: Failed to initialize servo2\n");
        pwm_cleanup_channel(pwm->servo1_chip, pwm->servo1_channel);
        return -1;
    }
    
    pwm->initialized = true;
    printf("PWM: Initialized servo control - chip%d/channel%d and chip%d/channel%d\n",
           pwm->servo1_chip, pwm->servo1_channel, pwm->servo2_chip, pwm->servo2_channel);
    
    return 0;
}

void pwm_control_cleanup(pwm_control_t* pwm) {
    if (!pwm || !pwm->initialized) {
        return;
    }
    
    printf("PWM: Cleaning up servo control...\n");
    
    // Cleanup servo1
    pwm_cleanup_channel(pwm->servo1_chip, pwm->servo1_channel);
    
    // Cleanup servo2
    pwm_cleanup_channel(pwm->servo2_chip, pwm->servo2_channel);
    
    pwm->initialized = false;
    printf("PWM: Cleanup completed\n");
}

int pwm_control_set_servo1(pwm_control_t* pwm, float value) {
    if (!pwm || !pwm->initialized) {
        return -1;
    }
    
    long duty_ns = pwm_value_to_duty_ns(value);
    return pwm_set_duty_cycle(pwm->servo1_chip, pwm->servo1_channel, duty_ns);
}

int pwm_control_set_servo2(pwm_control_t* pwm, float value) {
    if (!pwm || !pwm->initialized) {
        return -1;
    }
    
    long duty_ns = pwm_value_to_duty_ns(value);
    return pwm_set_duty_cycle(pwm->servo2_chip, pwm->servo2_channel, duty_ns);
}

int pwm_control_set_servos(pwm_control_t* pwm, const servo_values_t* values) {
    if (!pwm || !pwm->initialized || !values) {
        return -1;
    }
    
    // Set both servos
    int result1 = pwm_control_set_servo1(pwm, values->right_value);
    int result2 = pwm_control_set_servo2(pwm, values->left_value);
    
    return (result1 == 0 && result2 == 0) ? 0 : -1;
}

int pwm_control_enable(pwm_control_t* pwm, bool enable) {
    if (!pwm || !pwm->initialized) {
        return -1;
    }
    
    int result1 = pwm_enable_channel(pwm->servo1_chip, pwm->servo1_channel, enable);
    int result2 = pwm_enable_channel(pwm->servo2_chip, pwm->servo2_channel, enable);
    
    return (result1 == 0 && result2 == 0) ? 0 : -1;
}

int pwm_control_set_polarity(pwm_control_t* pwm, const char* polarity) {
    if (!pwm || !pwm->initialized || !polarity) {
        return -1;
    }
    
    int result1 = pwm_set_polarity(pwm->servo1_chip, pwm->servo1_channel, polarity);
    int result2 = pwm_set_polarity(pwm->servo2_chip, pwm->servo2_channel, polarity);
    
    return (result1 == 0 && result2 == 0) ? 0 : -1;
}