cmake_minimum_required(VERSION 3.10)
project(pcgp_project C)

add_executable(pcgp
    pcgp.c
    pcgpmem.c
)

set_property(TARGET pcgp PROPERTY C_STANDARD 99)
