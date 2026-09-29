include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(
    obstacle_avoidance
    STATIC
    obstacle_avoidance.c
)

add_library(
    reactive_coverage
    STATIC
    reactive_coverage.c
)

target_link_libraries(
    reactive_coverage
    obstacle_avoidance
)

add_library(
    route_map
    STATIC
    route_map.c
)