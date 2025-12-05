cmake_minimum_required(VERSION 3.10)
project(pcgp_app C)

add_executable(pcgp_app
    pcgp.c
)
add_custom_command(TARGET pcgp_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/pcgp.bat
        $<TARGET_FILE_DIR:pcgp_app>/pcgp.bat
)
target_link_libraries(pcgp_app PUBLIC pcgp)

set_property(TARGET pcgp_app PROPERTY C_STANDARD 11)
