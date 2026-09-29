include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(
    test_obstacle_avoidance
    test_obstacle_avoidance.c
)

target_link_libraries(
    test_obstacle_avoidance
    obstacle_avoidance
)

add_executable(
    test_reactive_coverage
    test_reactive_coverage.c
)

target_link_libraries(
    test_reactive_coverage
    reactive_coverage
    obstacle_avoidance
)

add_executable(
    test_route_map
    test_route_map.c
)

target_link_libraries(
    test_route_map
    route_map
)

add_executable(
    test_navigation_map
    test_navigation_map.c
)

target_link_libraries(
    test_navigation_map
    reactive_coverage
    route_map
    obstacle_avoidance
)