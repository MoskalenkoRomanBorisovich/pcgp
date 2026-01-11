cmake_minimum_required(VERSION 3.10)
project(pcgp_prof C)

add_executable(pcgp_prof
    pcgp_prof.c
)
target_link_libraries(pcgp_prof PUBLIC pcgp)
set_property(TARGET pcgp_prof PROPERTY C_STANDARD 11)
