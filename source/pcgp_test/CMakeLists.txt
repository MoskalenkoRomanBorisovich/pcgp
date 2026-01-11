cmake_minimum_required(VERSION 3.10)
project(pcgp_test CXX)

add_executable(${PROJECT_NAME}
    main.cpp
)
target_link_libraries(${PROJECT_NAME} PUBLIC 
    pcgp_parallel
    pcgp
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
