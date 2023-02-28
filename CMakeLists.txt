cmake_minimum_required(VERSION 3.20)

project(embedded-usbip C)

include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)

set(CMAKE_C_STANDARD 11)



set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

if (DEFINED CMAKE_C_BYTE_ORDER AND ${CMAKE_C_BYTE_ORDER} STREQUAL LITTLE_ENDIAN)
    message("Target is little endian")
    add_compile_definitions(ENDIAN_CONV)
elseif(DEFINED CMAKE_C_BYTE_ORDER)
    message("Target is big endian")
else()
    message(SEND_ERROR "Target endianness could not be determined")
endif()


add_subdirectory(src)

if(NOT EXISTS LIBRARY_ONLY)

add_subdirectory(test)
enable_testing()
add_test(NAME mem_pool COMMAND mem_pool)
add_test(NAME linked_list COMMAND linked_list)

endif()

