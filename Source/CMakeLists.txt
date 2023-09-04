cmake_minimum_required(VERSION 3.23)
project(DLSSPlugin)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)
cmake_path(SET DLSSDIR "${CMAKE_SOURCE_DIR}/DLSS")

if (NOT EXISTS ${DLSSDIR})
    if(WIN32)
        message(FATAL_ERROR "Please put the DLSS *.h, *.dll, and *.lib files into ${DLSSDIR}")
    elseif(UNIX AND NOT APPLE)
        message(FATAL_ERROR "Please put the DLSS *.h, *.a, and *.so files into ${DLSSDIR}")
    endif ()
endif ()

find_package(Vulkan)
add_library(ngx IMPORTED STATIC GLOBAL)
set_property(TARGET ngx APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_property(TARGET ngx APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)

if (WIN32)
    cmake_path(APPEND StaticLib "${DLSSDIR}" "nvsdk_ngx_d_dbg.lib")
    cmake_path(APPEND SharedLib "${DLSSDIR}" "nvngx_dlss.dll")
elseif(UNIX AND NOT APPLE)
    cmake_path(APPEND StaticLib "${DLSSDIR}" "libnvsdk_ngx.a")
    cmake_path(APPEND SharedLib "${DLSSDIR}" "libvidia-ngx-dlss.so.3.1.30")
endif ()

set_target_properties(ngx PROPERTIES IMPORTED_LOCATION "${StaticLib}")
set_property(TARGET ngx APPEND PROPERTY EXTRA_DLLS "${SharedLib}")
target_include_directories(ngx INTERFACE ${DLSSDIR})

add_library(DLSSPlugin main.cpp)

target_include_directories(DLSSPlugin PUBLIC "C:/VulkanSDK/1.3.261.0/Include" ) #get rid of this when you feel like it (look at vksdk path env variable, env variable/Include)(Cmake way of doing that)
target_link_libraries(DLSSPlugin ngx)
if(WIN32)
    add_custom_command(TARGET DLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/DLSSPlugin.dll ${CMAKE_SOURCE_DIR}/../Assets/Plugins POST_BUILD)
elseif(UNIX AND NOT APPLE)
    add_custom_command(TARGET DLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libDLSSPlugin.so ${CMAKE_SOURCE_DIR}/../Assets/Plugins POST_BUILD)
endif()
add_custom_command(TARGET DLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${SharedLib} ${CMAKE_SOURCE_DIR}/../Assets/Plugins POST_BUILD)
#line 38/40 -> look for a n cmake crossplatform way to get the path to the outputted library, target property or cmake command