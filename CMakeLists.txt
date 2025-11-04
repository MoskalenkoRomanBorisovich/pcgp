cmake_minimum_required(VERSION 3.10)
project(pcgp_project C)

add_subdirectory("pcgp_lib")

add_executable(pcgp_app
    pcgp.c
    pcgpmem.c
)

set_property(TARGET pcgp_app PROPERTY C_STANDARD 11)
