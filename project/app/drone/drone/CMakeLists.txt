cmake_minimum_required(VERSION 3.10...3.31)

project(drone)

# generate version info
include(cmake/Version.cmake)

option(COMPILE_FOR_RV1103_IPC "compile for rv1103 ipc" OFF)
option(COMPILE_FOR_RV1106_IPC "compile for rv1106 ipc" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
	add_definitions(-g -ggdb -gdwarf -funwind-tables -rdynamic -O0)
	add_definitions(-fno-stack-protector -fno-omit-frame-pointer)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
	message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
	add_definitions(-Os)
endif()

set(MAVLINK_DIR ${CMAKE_SOURCE_DIR}/../../component/third_libs/mavlink)
include_directories(${MAVLINK_DIR})

if(DRONE_CROSS_COMPILE)
message(STATUS "CROSS COMPILE TOOL: ${DRONE_CROSS_COMPILE}")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections -Wl,-rpath-link,${PROJECT_SOURCE_DIR}/lib/${DRONE_CROSS_COMPILE}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -Wl,-rpath-link,${PROJECT_SOURCE_DIR}/lib/${DRONE_CROSS_COMPILE}")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -Wl,--as-needed")
endif()

if(COMPILE_FOR_RV1103_IPC)
  add_subdirectory(src/rv1103_ipc)
endif()

if(COMPILE_FOR_RV1106_IPC)
  add_subdirectory(src/rv1106_ipc)
endif()

