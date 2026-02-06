# cmake_minimum_required(VERSION 3.10...3.31)

# Add common source files for our drone application
set(COMMON_SOURCES
    ${PROJECT_SOURCE_DIR}/common/crsf.c
    ${PROJECT_SOURCE_DIR}/common/udp_client.c
    ${PROJECT_SOURCE_DIR}/common/gpio_control.c
    ${PROJECT_SOURCE_DIR}/common/pwm_control.c
    ${PROJECT_SOURCE_DIR}/common/uart_utils.c
    ${PROJECT_SOURCE_DIR}/common/web_server.c
    ${PROJECT_SOURCE_DIR}/common/mongoose/mongoose.c
    ${PROJECT_SOURCE_DIR}/common/oled.c
    ${PROJECT_SOURCE_DIR}/common/linux-i2c.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/intf/ssd1306_interface.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/intf/i2c/ssd1306_i2c.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/intf/spi/ssd1306_spi.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/ssd1306_hal/linux/platform.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/lcd/oled_ssd1306.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/lcd/lcd_common.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/ssd1306_1bit.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/ssd1306_generic.c
    # ${PROJECT_SOURCE_DIR}/common/ssd1306/ssd1306_fonts.c

    ${PROJECT_SOURCE_DIR}/common/u8g2/sys/linux-fb/common/u8x8_d_framebuffer.c
)

file(GLOB U2G2_SOURCES
    ${PROJECT_SOURCE_DIR}/common/u8g2/csrc/*.c
)

# Add main source file
set(MAIN_SOURCES
    ${U2G2_SOURCES}
    ./main.c
)

# Combine all sources
set(ALL_SOURCES ${COMMON_SOURCES} ${MAIN_SOURCES})

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/common/mongoose
    ${PROJECT_SOURCE_DIR}/common/ssd1306
    ${PROJECT_SOURCE_DIR}/common/u8g2/csrc
)

# Add definitions for drone application
add_definitions(-DDRONE_APP)

# Suppress MAVLink warnings
add_compile_options(-Wno-address-of-packed-member)

# Add debug symbols for Debug build
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-g -ggdb -O0)
endif()

# Create executable
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# Link libraries needed for our drone application
target_link_libraries(${PROJECT_NAME}
    pthread
    m  # Math library for floating point operations
    rt # Real-time library for clock functions
    #i2cdev # I2C device library for Linux
)

# Install the executable
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(PROGRAMS DroneLunch.sh DESTINATION bin)
install(PROGRAMS DroneLunch-stop.sh DESTINATION bin)
# install(FILES ${PROJECT_SOURCE_DIR}/common/go2rtc.yaml DESTINATION share)
