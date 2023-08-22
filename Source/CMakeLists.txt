cmake_minimum_required(VERSION 3.25)
project(DLSSPlugin)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/DLSS)
    message(FATAL_ERROR "Please put the DLSS *.h, *.a, and *.so files into ${CMAKE_SOURCE_DIR}/DLSS")
endif ()

add_library(ngx IMPORTED STATIC GLOBAL)
set_property(TARGET ngx APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_property(TARGET ngx APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(ngx PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/DLSS/libnvsdk_ngx.a)
set_property(TARGET ngx APPEND PROPERTY EXTRA_DLLS "${CMAKE_SOURCE_DIR}/DLSS/libvidia-ngx-dlss.so.3.1.30")
target_include_directories(ngx INTERFACE ${CMAKE_SOURCE_DIR}/DLSS)

add_compile_options(-fms-extensions)

add_library(DLSSPlugin main.cpp)
target_link_libraries(DLSSPlugin ngx)
target_link_options(DLSSPlugin PUBLIC "-ldl")
add_custom_command(TARGET DLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libDLSSPlugin.so ${CMAKE_SOURCE_DIR}/../Assets/Plugins POST_BUILD)
add_custom_command(TARGET DLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/DLSS/libnvidia-ngx-dlss.so.3.1.30 ${CMAKE_SOURCE_DIR}/../Assets/Plugins POST_BUILD)
