include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_executable(mem_pool mem_pool.c)
target_link_libraries(mem_pool ${PROJECT_NAME})

add_executable(linked_list linked_list.c)
target_link_libraries(linked_list ${PROJECT_NAME})

add_executable(queue queue.c)
target_link_libraries(queue ${PROJECT_NAME})

add_executable(heap heap.c)
target_link_libraries(heap ${PROJECT_NAME})
