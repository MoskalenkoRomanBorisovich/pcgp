cmake_minimum_required(VERSION 3.10)
project(pcgp C)

add_library(pcgp
pcgp_lib.c
)

target_include_directories(pcgp PUBLIC include)

set_property(TARGET pcgp PROPERTY C_STANDARD 11)