cmake_minimum_required(VERSION 3.20.0)

if(DEFINED CUSTOM_BOILERPLATE)
    include(${CUSTOM_BOILERPLATE})
    return()
endif()

set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../common)
include(${COMMON_PATH}/common.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NON_BLOCKING)

GET_DEVICE_CONFIG_FILES(${BOARD} ../boards)
target_sources(app PRIVATE 
    main.c 
    services/ble.c
    drivers/mux/mux.c
    drivers/timers/timer.c
    drivers/gppi/gppi.c
    drivers/gpiote/gpiote.c
    drivers/saadc/saadc.c
    drivers/dac/dac.c
    drivers/dac/dac_test.c
)
target_include_directories(app PRIVATE 
    ../../../common 
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/mux
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/timers
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/gppi
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/gpiote
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/saadc
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dac
)
