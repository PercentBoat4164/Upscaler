cmake_minimum_required(VERSION 3.18)
project(Upscaler)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)

set(PLUGINS_DIR "${CMAKE_SOURCE_DIR}/../UnityProject/Assets/Plugins")
cmake_path(ABSOLUTE_PATH PLUGINS_DIR NORMALIZE)

find_package(Vulkan)

include(CMakeDependentOption)

###########
# Options #
###########

function (ConiferOnIfTruthy ITEM_LIST VARIABLE_LIST)
    foreach (ITEM IN ZIP_LISTS ITEM_LIST VARIABLE_LIST)
        if (${ITEM_1})
            set(${ITEM_0} ON PARENT_SCOPE)
        endif()
    endforeach ()
endfunction()

# At least one graphics API must be enabled.
cmake_dependent_option(ENABLE_VULKAN "Compiles with Vulkan support." ON "Vulkan_FOUND" OFF)
cmake_dependent_option(ENABLE_DX12 "Compiles with DX12 support. Value is only respected on Windows operating systems." ON "WIN32" OFF)
cmake_dependent_option(ENABLE_DX11 "Compiles with DX11 support. Value is only respected on Windows operating systems." ON "WIN32" OFF)

# At least one upscaler must be enabled.
cmake_dependent_option(ENABLE_DLSS "Compiles with DLSS support. Value is ignored on macOS operating systems." ON "NOT APPLE" OFF)
option(ENABLE_FSR2 "Compiles with FSR2 support." ON)
option(ENABLE_XESS "Compiles with XeSS support." ON)

if (ENABLE_DLSS)
    ConiferOnIfTruthy("SHOULD_ENABLE_VULKAN;SHOULD_ENABLE_DX12;SHOULD_ENABLE_DX11" "ON;ON;ON")
endif ()
if (ENABLE_FSR2)
    ConiferOnIfTruthy("SHOULD_ENABLE_VULKAN;SHOULD_ENABLE_DX12;SHOULD_ENABLE_DX11" "ON;ON;OFF")
endif ()
if (ENABLE_XESS)
    ConiferOnIfTruthy("SHOULD_ENABLE_VULKAN;SHOULD_ENABLE_DX12;SHOULD_ENABLE_DX11" "OFF;ON;OFF")
endif ()
foreach (ITEM SHOULD_ENABLE_VULKAN;SHOULD_ENABLE_DX12;SHOULD_ENABLE_DX11)
    if (NOT ${ITEM})
        string(SUBSTRING ${ITEM} 7 -1 VARIABLE)
        set(${VARIABLE} OFF)
    endif ()
endforeach ()

######################
# Paths and Warnings #
######################

function (ConiferFilterList ITEM_LIST VARIABLE_LIST _OUTPUT_VARIABLE)
    set(${_OUTPUT_VARIABLE} "")
    foreach (ITEM IN ZIP_LISTS ITEM_LIST VARIABLE_LIST)
        if (${${ITEM_1}})
            list(APPEND ${_OUTPUT_VARIABLE} ${ITEM_0})
        endif()
    endforeach ()
    set(${_OUTPUT_VARIABLE} ${${_OUTPUT_VARIABLE}} PARENT_SCOPE)
endfunction ()

function (ConiferListToString INPUT_LIST _OUTPUT_STRING)
    set(${_OUTPUT_STRING} "")
    list(LENGTH INPUT_LIST OUTPUT_LIST_LEN)
    if (OUTPUT_LIST_LEN EQUAL ZERO)
        return()
    endif ()
    list(GET INPUT_LIST 0 FIRST)
    list(GET INPUT_LIST -1 LAST)
    foreach (ITEM ${INPUT_LIST})
        if (NOT ${ITEM} STREQUAL ${FIRST})
            set(${_OUTPUT_STRING} "${${_OUTPUT_STRING}}, ")
        endif()
        if (${ITEM} STREQUAL ${LAST} AND ${OUTPUT_LIST_LEN} GREATER 1)
            set(${_OUTPUT_STRING} "${${_OUTPUT_STRING}}and ")
        endif()
        set(${_OUTPUT_STRING} "${${_OUTPUT_STRING}}${ITEM}")
    endforeach ()
    set(${_OUTPUT_STRING} ${${_OUTPUT_STRING}} PARENT_SCOPE)
endfunction ()

ConiferFilterList("Vulkan;DirectX 12;DirectX 11" "ENABLE_VULKAN;ENABLE_DX12;ENABLE_DX11" FILTERED_LIST)
ConiferListToString("${FILTERED_LIST}" FINAL_STRING)
message(STATUS "Compiling with support for ${FINAL_STRING}.")
ConiferFilterList("NVIDIA's Deep Learning Super Sampling;AMD's FidelityFX Super Resolution 2;Intel's Xe Super Sampling" "ENABLE_DLSS;ENABLE_FSR2;ENABLE_XESS" FILTERED_LIST)
ConiferListToString("${FILTERED_LIST}" FINAL_STRING)
message(STATUS "Compiling with ${FINAL_STRING}.")

# Fail if no upscaler was selected
if (NOT ENABLE_DLSS AND NOT ENABLE_FSR2 AND NOT ENABLE_XESS)
    message(FATAL_ERROR "No upscaler(s) were enabled.")
endif ()

# Fail if no graphics API was selected
if (NOT ENABLE_VULKAN AND NOT ENABLE_DX12 AND NOT ENABLE_DX11)
    message(FATAL_ERROR "No graphics API(s) were enabled.")
endif ()

cmake_path(SET NGX_SDK_DIR "${CMAKE_SOURCE_DIR}/DLSS-3.7.10")
cmake_path(SET FFX_SDK_DIR "${CMAKE_SOURCE_DIR}/ffx_sdk")
cmake_path(SET XESS_SDK_DIR "${CMAKE_SOURCE_DIR}/XeSS_SDK-1.3.0")

# Add the plugin path the list of shared libraries to copy
if (WIN32)
    list(APPEND SHARED_LIBRARIES "${CMAKE_BINARY_DIR}/GfxPluginUpscaler.dll")
elseif (UNIX AND NOT APPLE)
    list(APPEND SHARED_LIBRARIES "${CMAKE_BINARY_DIR}/libGfxPluginUpscaler.so")
endif ()

# Find the Unity PluginAPI headers location
if (WIN32)
    cmake_path(SET GLOBBING_EXPRESSION "C:/Program Files")
elseif (UNIX AND NOT APPLE)
    cmake_path(SET GLOBBING_EXPRESSION "$ENV{HOME}")
endif ()
string(APPEND GLOBBING_EXPRESSION "/Unity/Hub/Editor/*")
file(GLOB UNITY_FILES ${GLOBBING_EXPRESSION})
if (NOT UNITY_FILES STREQUAL "")
    list(GET UNITY_FILES -1 UNITY_FILES)
    cmake_path(GET UNITY_FILES FILENAME UNITY_VERSION)
    message("Compiling against Unity version ${UNITY_VERSION}.")
    cmake_path(APPEND UNITY_FILES "Editor/Data/PluginAPI")
endif ()

# Ensure Unity was found
if (NOT EXISTS ${UNITY_FILES})
    message(FATAL_ERROR "Please install some version of the Unity Editor.")
endif ()

# Ensure DLSS was found
if (ENABLE_DLSS AND NOT EXISTS ${NGX_SDK_DIR})
    message(FATAL_ERROR "Please extract the latest DLSS SDK into '${NGX_SDK_DIR}'.")
endif ()

# Ensure FSR was found
if (ENABLE_FSR2 AND NOT EXISTS ${FFX_SDK_DIR})
    message(FATAL_ERROR "Please extract the latest FSR SDK into '${FFX_SDK_DIR}'.")
endif ()

# Ensure that at least one enabled graphics api supports at least one of the enabled upscalers.
set(IS_COMPATIBLE OFF)
if (NOT IS_COMPATIBLE AND ENABLE_DLSS)
    if (ENABLE_VULKAN OR ENABLE_DX12 OR ENABLE_DX11)
        set(IS_COMPATIBLE ON)
    endif ()
endif ()
if (NOT IS_COMPATIBLE AND ENABLE_FSR2)
    if (ENABLE_VULKAN OR ENABLE_DX12)
        set(IS_COMPATIBLE ON)
    endif ()
endif ()
if (NOT IS_COMPATIBLE)
    message(FATAL_ERROR "No enabled upscaler(s) are compatible with any enabled graphics API(s).")
endif ()

#################
# Add libraries #
#################

# Set up the DLSS library
function (ConiferAddDLSSLibrary LIBRARY_NAME LIBRARY_PATH DYNAMIC_LIBRARY_FILENAME STATIC_LIBRARY_FILENAME)
    add_library(${LIBRARY_NAME} IMPORTED SHARED)
    target_include_directories(${LIBRARY_NAME} INTERFACE ${ARGN})
    if (WIN32)
        cmake_path(APPEND LIBRARY_PATH "Windows_x86_64")
        set(STATIC_LIBRARY_FILENAME "x86_64/${STATIC_LIBRARY_FILENAME}")
        if (BUILD_SHARED_LIBS)
            set(STATIC_LIBRARY_FILENAME "${STATIC_LIBRARY_FILENAME}_d")
        else ()
            set(STATIC_LIBRARY_FILENAME "${STATIC_LIBRARY_FILENAME}_s")
        endif ()
    elseif (UNIX AND NOT APPLE)
        cmake_path(APPEND LIBRARY_PATH "Linux_x86_64")
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        cmake_path(APPEND LIBRARY_PATH "dev")
        if (WIN32)
            set(STATIC_LIBRARY_FILENAME "${STATIC_LIBRARY_FILENAME}_dbg")
        endif ()
    else ()
        cmake_path(APPEND LIBRARY_PATH "rel")
    endif ()
    if (WIN32)
        set(STATIC_LIBRARY_FILENAME "${LIBRARY_PATH}/../${STATIC_LIBRARY_FILENAME}.lib")
        cmake_path(ABSOLUTE_PATH STATIC_LIBRARY_FILENAME NORMALIZE)
        set(DYNAMIC_LIBRARY_FILENAME "${LIBRARY_PATH}/${DYNAMIC_LIBRARY_FILENAME}.dll")
    elseif (UNIX AND NOT APPLE)
        set(STATIC_LIBRARY_FILENAME "${LIBRARY_PATH}/../lib${STATIC_LIBRARY_FILENAME}.a")
        cmake_path(ABSOLUTE_PATH STATIC_LIBRARY_FILENAME NORMALIZE)
        set(DYNAMIC_LIBRARY_FILENAME "${LIBRARY_PATH}/lib${DYNAMIC_LIBRARY_FILENAME}.so*")
        file(GLOB DYNAMIC_LIBRARY_FILENAME ${DYNAMIC_LIBRARY_FILENAME})
    endif ()
    if (NOT EXISTS ${STATIC_LIBRARY_FILENAME})
        message(SEND_ERROR "Could not find DLSS static library: ${STATIC_LIBRARY_FILENAME}")
    endif ()
    if (NOT EXISTS ${DYNAMIC_LIBRARY_FILENAME})
        message(SEND_ERROR "Could not find DLSS dynamic library: ${DYNAMIC_LIBRARY_FILENAME}")
    endif ()
    if (WIN32)
        set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_LOCATION "${DYNAMIC_LIBRARY_FILENAME}")
        set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_IMPLIB "${STATIC_LIBRARY_FILENAME}")
    else ()
        set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_LOCATION "${STATIC_LIBRARY_FILENAME}")
        set_target_properties(${LIBRARY_NAME} PROPERTIES EXTRA_DLLS "${DYNAMIC_LIBRARY_FILENAME}")
    endif ()
    list(APPEND CONIFER_LIBRARIES ${LIBRARY_NAME})
    set(CONIFER_LIBRARIES ${CONIFER_LIBRARIES} PARENT_SCOPE)
    list(APPEND SHARED_LIBRARIES ${DYNAMIC_LIBRARY_FILENAME})
    set(SHARED_LIBRARIES ${SHARED_LIBRARIES} PARENT_SCOPE)
endfunction ()

if (ENABLE_DLSS)
    if (WIN32)
        ConiferAddDLSSLibrary(dlss "${NGX_SDK_DIR}/lib" "nvngx_dlss" "nvsdk_ngx" "${NGX_SDK_DIR}/include")
    else ()
        ConiferAddDLSSLibrary(dlss "${NGX_SDK_DIR}/lib" "nvidia-ngx-dlss" "nvsdk_ngx" "${NGX_SDK_DIR}/include")
    endif ()
endif ()

# Set up the FSR libraries
function (ConiferAddFSRLibrary LIBRARY_NAME LIBRARY_PATH DYNAMIC_LIBRARY_FILENAME)
    add_library(${LIBRARY_NAME} IMPORTED SHARED)
    target_include_directories(${LIBRARY_NAME} INTERFACE ${ARGN})
    if (WIN32)
        cmake_path(APPEND LIBRARY_PATH "Windows_x86_64")
    elseif (UNIX AND NOT APPLE)
        cmake_path(APPEND LIBRARY_PATH "Linux_x86_64")
    endif ()
    set(STATIC_LIBRARY_FILENAME "${LIBRARY_PATH}")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        cmake_path(SET LIBRARY_PATH "${LIBRARY_PATH}/dev")
        set(DYNAMIC_LIBRARY_FILENAME "${DYNAMIC_LIBRARY_FILENAME}d")
    else ()
        cmake_path(SET LIBRARY_PATH "${LIBRARY_PATH}/rel")
    endif ()
    if (WIN32)
        set(STATIC_LIBRARY_FILENAME "${STATIC_LIBRARY_FILENAME}/${DYNAMIC_LIBRARY_FILENAME}.lib")
        set(DYNAMIC_LIBRARY_FILENAME "${LIBRARY_PATH}/${DYNAMIC_LIBRARY_FILENAME}.dll")
    elseif (UNIX AND NOT APPLE)
        set(DYNAMIC_LIBRARY_FILENAME "${LIBRARY_PATH}/lib${DYNAMIC_LIBRARY_FILENAME}.so")
    endif ()
    set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_LOCATION "${DYNAMIC_LIBRARY_FILENAME}")
    if (STATIC_LIBRARY_FILENAME)
        set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_IMPLIB "${STATIC_LIBRARY_FILENAME}")
    endif ()
    list(APPEND CONIFER_LIBRARIES ${LIBRARY_NAME})
    set(CONIFER_LIBRARIES ${CONIFER_LIBRARIES} PARENT_SCOPE)
    list(APPEND SHARED_LIBRARIES ${DYNAMIC_LIBRARY_FILENAME})
    set(SHARED_LIBRARIES ${SHARED_LIBRARIES} PARENT_SCOPE)
endfunction ()

if (ENABLE_FSR2)
    ConiferAddFSRLibrary(fsr2_core "${FFX_SDK_DIR}/lib" "ffx_fsr2_x86_64" "${FFX_SDK_DIR}" "${FFX_SDK_DIR}/FidelityFX/host")

    if (ENABLE_VULKAN)
        ConiferAddFSRLibrary(fsr2_vk "${FFX_SDK_DIR}/lib" "ffx_backend_vk_x86_64" "${FFX_SDK_DIR}/FidelityFX/host/backends/vk")
    endif ()

    if (ENABLE_DX12)
        ConiferAddFSRLibrary(fsr2_dx12 "${FFX_SDK_DIR}/lib" "ffx_backend_dx12_x86_64" "${FFX_SDK_DIR}/FidelityFX/host/backends/dx12")
    endif ()
endif ()

function (ConiferAddXeSSLibrary LIBRARY_NAME LIBRARY_PATH LIBRARY_FILENAME)
    add_library(${LIBRARY_NAME} IMPORTED SHARED)
    target_include_directories(${LIBRARY_NAME} INTERFACE ${ARGN})
    set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_LOCATION "${LIBRARY_PATH}/bin/${LIBRARY_FILENAME}.dll")
    set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_IMPLIB "${LIBRARY_PATH}/lib/${LIBRARY_FILENAME}.lib")
    list(APPEND CONIFER_LIBRARIES ${LIBRARY_NAME})
    set(CONIFER_LIBRARIES ${CONIFER_LIBRARIES} PARENT_SCOPE)
    list(APPEND SHARED_LIBRARIES "${LIBRARY_PATH}/bin/${LIBRARY_FILENAME}.dll")
    set(SHARED_LIBRARIES ${SHARED_LIBRARIES} PARENT_SCOPE)
endfunction ()

if (ENABLE_XESS)
    ConiferAddXeSSLibrary(xess "${XESS_SDK_DIR}" "libxess" "${XESS_SDK_DIR}/inc")
endif ()

##############
# Our plugin #
##############

# Add source files for selected graphics APIs
if (ENABLE_VULKAN)
    set(VULKAN_SOURCES GraphicsAPI/Vulkan.cpp)
endif ()
if (ENABLE_DX12)
    set(DX12_SOURCES GraphicsAPI/DX12.cpp)
endif ()
if (ENABLE_DX11)
    set(DX11_SOURCES GraphicsAPI/DX11.cpp)
endif ()

# Add source files for selected upscalers
if (ENABLE_DLSS)
    set(DLSS_SOURCES Upscaler/DLSS.cpp)
endif ()
if (ENABLE_FSR2)
    set(FSR2_SOURCES Upscaler/FSR2.cpp)
endif ()
if (ENABLE_XESS)
    set(XESS_SOURCES Upscaler/XeSS.cpp)
endif ()

# Add library and link Unity files
add_library(GfxPluginUpscaler main.cpp
        ${DLSS_SOURCES}
        ${FSR2_SOURCES}
        ${XESS_SOURCES}

        ${DX11_SOURCES}
        ${DX12_SOURCES}
        ${VULKAN_SOURCES}

        GraphicsAPI/GraphicsAPI.cpp
        Upscaler/NoUpscaler.cpp
        Upscaler/Upscaler.cpp
        Plugin.hpp
)

add_custom_command(TARGET GfxPluginUpscaler PRE_BUILD COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --blue "Compiling against Unity version ${UNITY_VERSION}.")
target_include_directories(GfxPluginUpscaler PUBLIC ${UNITY_FILES} ${CMAKE_SOURCE_DIR})
if (NOT WIN32)
    target_link_options(GfxPluginUpscaler PUBLIC -Wl,-rpath=$ORIGIN)
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(GfxPluginUpscaler PUBLIC -s)
    endif ()
endif ()

# Link selected graphics APIs
if (ENABLE_VULKAN)
    target_include_directories(GfxPluginUpscaler PUBLIC ${Vulkan_INCLUDE_DIR})
endif ()

# Link selected upscaler libraries
target_link_libraries(GfxPluginUpscaler ${CONIFER_LIBRARIES})

# Add compile definitions
foreach (ITEM ENABLE_VULKAN;ENABLE_DX12;ENABLE_DX11;ENABLE_DLSS;ENABLE_FSR2;ENABLE_XESS)
    if (${ITEM})
        target_compile_definitions(GfxPluginUpscaler PUBLIC ${ITEM})
    endif ()
endforeach ()

# Copy the resulting shared library to the Unity Project's Asset/Plugins directory.
ConiferListToString("${SHARED_LIBRARIES}" SHARED_LIBRARIES_STRING)
add_custom_command(TARGET GfxPluginUpscaler POST_BUILD COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --blue "Copying ${SHARED_LIBRARIES_STRING} to ${PLUGINS_DIR}.")
add_custom_command(TARGET GfxPluginUpscaler POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SHARED_LIBRARIES} ${PLUGINS_DIR})