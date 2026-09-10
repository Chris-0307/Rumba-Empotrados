include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(
    test_obstacle_avoidance
    test_obstacle_avoidance.c
)

target_link_libraries(
    test_obstacle_avoidance
    obstacle_avoidance
)
