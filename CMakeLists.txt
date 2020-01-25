# SPDX-License-Identifier: Apache-2.0

set(BOARD nrf52840_pca10059)   # Dongle
#set(BOARD nrf52840_pca10056)   # DK

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(scan_adv)


FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

zephyr_library_include_directories($ENV{ZEPHYR_BASE}/samples/bluetooth)
