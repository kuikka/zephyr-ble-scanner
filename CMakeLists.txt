# SPDX-License-Identifier: Apache-2.0

#set(BOARD nrf52840dongle_nrf52840)   # Dongle
set(BOARD nrf52840dk_nrf52840)   # DK

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr-ble-scanner)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

