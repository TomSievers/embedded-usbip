include_directories(.)

include(${CMAKE_SOURCE_DIR}/CheckTypeExists.cmake)

check_type_exists(uintptr_t)

set(SRCS
    usbip.c 
    conv.c 
    linked_list.c 
    mem_pool.c
    queue.c
    usb/vhci.c
    usb/dev.c
)

add_compile_options(-Wall -Werror -Wno-unused)

add_library(${PROJECT_NAME} STATIC ${SRCS} ${HEADERS})

if(NOT EXISTS LIBRARY_ONLY)

    add_executable(${PROJECT_NAME}-app main.c )

    target_link_libraries(${PROJECT_NAME}-app ${PROJECT_NAME})

endif()