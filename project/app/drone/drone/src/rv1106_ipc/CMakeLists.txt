# cmake_minimum_required(VERSION 3.10...3.31)

# Add common source files for our drone application
set(COMMON_SOURCES
    ${PROJECT_SOURCE_DIR}/common/crsf.c
    ${PROJECT_SOURCE_DIR}/common/udp_client.c
    ${PROJECT_SOURCE_DIR}/common/gpio_control.c
    ${PROJECT_SOURCE_DIR}/common/pwm_control.c
)

# Add main source file
set(MAIN_SOURCES
    ./main.c
)

# Combine all sources
set(ALL_SOURCES ${COMMON_SOURCES} ${MAIN_SOURCES})

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/common
)

# Add definitions for drone application
add_definitions(-DDRONE_APP)

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
)

# Install the executable
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(PROGRAMS DroneLunch.sh DESTINATION bin)
install(PROGRAMS DroneLunch-stop.sh DESTINATION bin)
install(FILES ${PROJECT_SOURCE_DIR}/common/go2rtc.yaml DESTINATION share)
