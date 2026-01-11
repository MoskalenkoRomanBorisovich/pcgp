cmake_minimum_required(VERSION 3.10)
project(pcgpp_app CXX)

add_executable(${PROJECT_NAME}
    main.cpp
    pcgpp.cpp
)
target_link_libraries(${PROJECT_NAME} PUBLIC pcgp_parallel)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
