# Minimum CMake version
cmake_minimum_required(VERSION 3.20.0)

# Locate the Zephyr RTOS source, ZEPHYR_BASE must be set in env.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Name the project
project(nrf_mp3)

# Locate the source code for the application
target_sources(app PRIVATE 
    src/main.c
    src/buttons_and_leds/lbs.c
    src/states/fs_nav.c
)
