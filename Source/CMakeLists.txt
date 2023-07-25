cmake_minimum_required(VERSION 3.25)
project(DLSSPlugin)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)

add_compile_options(-fms-extensions)
add_library(DLSSPlugin main.cpp)
target_link_libraries(DLSSPlugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/DLSS/libnvsdk_ngx.a)
add_custom_command(TARGET DLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libDLSSPlugin.so ${CMAKE_SOURCE_DIR}/../Assets/Plugins POST_BUILD)

