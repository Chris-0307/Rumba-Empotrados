cmake_minimum_required(VERSION 3.16)

project(robot
    VERSION 1.0.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET "libgpiod>=2.0")

add_library(robot SHARED
    src/robot_hw.c

    src/motors.c
    src/motor_backend.c

    src/sensors.c
    src/sensor_backend.c
    src/adc.c

    src/leds.c
    src/audio.c
)

target_include_directories(robot
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(robot
    PRIVATE
        PkgConfig::GPIOD
        m
)

set_target_properties(robot PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

install(
    TARGETS robot
    LIBRARY DESTINATION lib
)

install(
    FILES include/robot_hw.h
    DESTINATION include
)