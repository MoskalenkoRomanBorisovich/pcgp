cmake_minimum_required(VERSION 3.10)
project(routing_test C)

add_executable( ${PROJECT_NAME}
    routing_test.cpp
)
target_link_libraries(${PROJECT_NAME} PUBLIC routing)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 20)
