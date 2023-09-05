cmake_minimum_required(VERSION 3.23)
project(DLSSPlugin)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)

cmake_path(SET DLSS_DIR "${CMAKE_SOURCE_DIR}/nvngx_dlss_sdk")
cmake_path(SET PLUGINS_DIR "${CMAKE_SOURCE_DIR}/../UnityProject/Assets/Plugins")
if(WIN32)
    cmake_path(SET PLUGIN_FILE "${CMAKE_BINARY_DIR}/GfxPluginDLSSPlugin.dll")
elseif(UNIX AND NOT APPLE)
    cmake_path(SET PLUGIN_FILE "${CMAKE_BINARY_DIR}/libGfxPluginDLSSPlugin.so")
endif()
if (WIN32)
    cmake_path(SET GLOBBING_EXPRESSION "C:/Program Files")
elseif (UNIX AND NOT APPLE)
    cmake_path(SET GLOBBING_EXPRESSION "$ENV{HOME}")
endif ()
string(APPEND GLOBBING_EXPRESSION "/Unity/Hub/Editor/*")
file(GLOB UNITY_FILES ${GLOBBING_EXPRESSION})
if (NOT UNITY_FILES STREQUAL "")
    list(GET UNITY_FILES 0 UNITY_FILES)
    cmake_path(APPEND UNITY_FILES "Editor/Data/PluginAPI")
endif ()

if (NOT EXISTS ${UNITY_FILES})
    message(FATAL_ERROR "Please install some version of the Unity Editor.")
endif ()

if (NOT EXISTS ${DLSS_DIR})
    message(FATAL_ERROR "Please extract the latest DLSS SDK into 'nvngx_dlss_sdk'.")
endif ()

find_package(Vulkan REQUIRED)

##############
# NGX plugin #
##############

add_library(ngx IMPORTED STATIC GLOBAL)
target_include_directories(ngx INTERFACE "${DLSS_DIR}/include")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET ngx APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_property(TARGET ngx APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
endif ()

cmake_path(SET DLSS_SHARED_LIB "${DLSS_DIR}/lib")
cmake_path(SET DLSS_STATIC_LIB "${DLSS_DIR}/lib")
if (WIN32)
    cmake_path(APPEND DLSS_SHARED_LIB "Windows_x86_64")
    cmake_path(APPEND DLSS_STATIC_LIB "Windows_x86_64/x86_64")
elseif (UNIX AND NOT APPLE)
    cmake_path(APPEND DLSS_SHARED_LIB "Linux_x86_64")
    cmake_path(APPEND DLSS_STATIC_LIB "Linux_x86_64")
endif ()
if (BUILD_SHARED_LIBS)
    set(GLOBBING_EXPRESSION "*d")
else ()
    set(GLOBBING_EXPRESSION "*s")
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    cmake_path(APPEND DLSS_SHARED_LIB "rel")
    string(APPEND GLOBBING_EXPRESSION ".lib")
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    cmake_path(APPEND DLSS_SHARED_LIB "dev")
    string(APPEND GLOBBING_EXPRESSION "_dbg.lib")
endif ()
if (UNIX AND NOT APPLE)
    set(GLOBBING_EXPRESSION "*")
endif ()

file(GLOB DLSS_SHARED_LIB LIST_DIRECTORIES FALSE "${DLSS_SHARED_LIB}/*")
set_property(TARGET ngx APPEND PROPERTY EXTRA_DLLS "${DLSS_SHARED_LIB}")

file(GLOB DLSS_STATIC_LIB LIST_DIRECTORIES FALSE "${DLSS_STATIC_LIB}/${GLOBBING_EXPRESSION}")
set_target_properties(ngx PROPERTIES IMPORTED_LOCATION "${DLSS_STATIC_LIB}")

##############
# Our plugin #
##############

add_library(GfxPluginDLSSPlugin main.cpp)
target_include_directories(GfxPluginDLSSPlugin PUBLIC ${Vulkan_INCLUDE_DIR} ${UNITY_FILES})
target_link_libraries(GfxPluginDLSSPlugin ngx)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (WIN32)
        target_compile_options(GfxPluginDLSSPlugin PRIVATE /Zi)
    elseif (UNIX AND NOT APPLE)
        target_compile_options(GfxPluginDLSSPlugin PRIVATE -g)
    endif ()
endif ()
add_custom_command(TARGET GfxPluginDLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_FILE} ${DLSS_SHARED_LIB} ${PLUGINS_DIR} COMMENT "Copying shared libraries. Please restart Unity." POST_BUILD)