cmake_minimum_required(VERSION 3.23)
project(DLSSPlugin)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)

cmake_path(SET DLSS_DIR "${CMAKE_SOURCE_DIR}/nvngx_dlss_sdk")
cmake_path(SET PLUGINS_DIR "${CMAKE_SOURCE_DIR}/../UnityProject/Assets/Plugins")
if(WIN32)
    cmake_path(SET PLUGIN_FILE "${CMAKE_BINARY_DIR}/DLSSPlugin.dll")
elseif(UNIX AND NOT APPLE)
    cmake_path(SET PLUGIN_FILE "${CMAKE_BINARY_DIR}/libDLSSPlugin.so")
endif()
if (WIN32)
    cmake_path(SET GLOBBING_EXPRESSION "C:/Program Files")
elseif (UNIX AND NOT APPLE)
    cmake_path(SET GLOBBING_EXPRESSION "~")
endif ()
string(APPEND GLOBBING_EXPRESSION "/Unity/Hub/Editor/*")
file(GLOB UNITY_FILES ${GLOBBING_EXPRESSION})
list(GET UNITY_FILES 0 UNITY_FILES)
cmake_path(APPEND UNITY_FILES "Editor/Data/PluginAPI")

if (NOT EXISTS ${UNITY_FILES})
    message(FATAL_ERROR "Please install some version of the Unity Editor.")
endif ()

if (NOT EXISTS ${DLSS_DIR})
    message(FATAL_ERROR "Please extract the latest DLSS SDK into 'nvngx_dlss_sdk'.")
endif ()

if (NOT EXISTS $ENV{VULKAN_SDK})
    message(FATAL_ERROR "Please install the Vulkan SDK.")
endif ()

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
    cmake_path(SET GLOBBING_EXPRESSION "*")
endif ()

file(GLOB DLSS_SHARED_LIB "${DLSS_SHARED_LIB}/*")
set_property(TARGET ngx APPEND PROPERTY EXTRA_DLLS "${DLSS_SHARED_LIB}")

file(GLOB DLSS_STATIC_LIB "${DLSS_STATIC_LIB}/${GLOBBING_EXPRESSION}")
set_target_properties(ngx PROPERTIES IMPORTED_LOCATION "${DLSS_STATIC_LIB}")

##############
# Our plugin #
##############

add_library(DLSSPlugin main.cpp)
target_include_directories(DLSSPlugin PUBLIC "$ENV{VULKAN_SDK}/Include" ${UNITY_FILES})
target_link_libraries(DLSSPlugin ngx)
add_custom_command(TARGET DLSSPlugin COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_FILE} ${DLSS_SHARED_LIB} ${PLUGINS_DIR} COMMENT "Copying shared libraries. Please restart Unity." POST_BUILD)