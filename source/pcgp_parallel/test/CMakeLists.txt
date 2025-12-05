cmake_minimum_required(VERSION 3.10)
project(pcgp_parallel_test C)

add_executable( ${PROJECT_NAME}
    pcgp_parallel_test.cpp
)
target_link_libraries(${PROJECT_NAME} PUBLIC pcgp_parallel)
set_property(TARGET pcgp_prof PROPERTY C_STANDARD 11)
