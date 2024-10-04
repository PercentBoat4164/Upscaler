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
cmake_dependent_option(ENABLE_DX12 "Compiles with DX12 support." ON "WIN32" OFF)
cmake_dependent_option(ENABLE_DX11 "Compiles with DX11 support." ON "WIN32" OFF)

# At least one upscaler must be enabled.
cmake_dependent_option(ENABLE_DLSS "Compiles with DLSS support." ON "NOT UNIX" OFF)
option(ENABLE_FSR "Compiles with FSR3 support." ON)
cmake_dependent_option(ENABLE_XESS "Compiles with XeSS support." ON "WIN32" OFF)

cmake_dependent_option(ENABLE_FRAME_GENERATION "Compiles with frame generation support." ON "WIN32" OFF)

if (ENABLE_DLSS)
    ConiferOnIfTruthy("SHOULD_ENABLE_VULKAN;SHOULD_ENABLE_DX12;SHOULD_ENABLE_DX11" "ON;ON;ON")
endif ()
if (ENABLE_FSR)
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
ConiferFilterList("NVIDIA's Deep Learning Super Sampling;AMD's FidelityFX Super Resolution 3;Intel's Xe Super Sampling" "ENABLE_DLSS;ENABLE_FSR;ENABLE_XESS" FILTERED_LIST)
ConiferListToString("${FILTERED_LIST}" FINAL_STRING)
message(STATUS "Compiling with ${FINAL_STRING}.")
if (ENABLE_FRAME_GENERATION)
    message(STATUS "Compiling with Frame Generation.")
endif ()

# Fail if no upscaler was selected
if (NOT ENABLE_DLSS AND NOT ENABLE_FSR AND NOT ENABLE_XESS)
    message(FATAL_ERROR "No upscaler(s) were enabled.")
endif ()

# Fail if no graphics API was selected
if (NOT ENABLE_VULKAN AND NOT ENABLE_DX12 AND NOT ENABLE_DX11)
    message(FATAL_ERROR "No graphics API(s) were enabled.")
endif ()

cmake_path(SET STREAMLINE_SDK_DIR "${CMAKE_SOURCE_DIR}/external/Streamline-2.4.15")
cmake_path(SET FFX_SDK_DIR "${CMAKE_SOURCE_DIR}/external/ffx_sdk")
cmake_path(SET XESS_SDK_DIR "${CMAKE_SOURCE_DIR}/external/XeSS_SDK-1.3.1")
set(DESIRED_UNITY_VERSION "2022.3.47f1")
set(REQUIRE_DESIRED_UNITY_VERSION OFF)

# Add the plugin path the list of shared libraries to copy
if (WIN32)
    list(APPEND LIBRARIES_TO_COPY "${CMAKE_BINARY_DIR}/GfxPluginUpscaler.dll")
elseif (UNIX AND NOT APPLE)
    list(APPEND LIBRARIES_TO_COPY "${CMAKE_BINARY_DIR}/libGfxPluginUpscaler.so")
endif ()

# Find the Unity PluginAPI headers location
if (WIN32)
    cmake_path(SET GLOBBING_EXPRESSION "C:/Program Files")
elseif (UNIX AND NOT APPLE)
    cmake_path(SET GLOBBING_EXPRESSION "$ENV{HOME}")
endif ()
string(APPEND GLOBBING_EXPRESSION "/Unity/Hub/Editor/*")
file(GLOB UNITY_DIRS ${GLOBBING_EXPRESSION})
foreach (DESIRED_UNITY_DIR ${UNITY_DIRS})
    cmake_path(GET DESIRED_UNITY_DIR FILENAME UNITY_VERSION)
    if (${UNITY_VERSION} STREQUAL ${DESIRED_UNITY_VERSION})
        set(UNITY_DIR "${DESIRED_UNITY_DIR}/Editor/Data/PluginAPI")
        message(STATUS "Compiling against desired Unity version ${UNITY_VERSION} (${UNITY_DIR}).")
        break()
    endif ()
endforeach ()
if (NOT EXISTS ${UNITY_DIR})
    if (REQUIRE_DESIRED_UNITY_VERSION)
        message(FATAL_ERROR "Please install the required Unity Editor version: ${DESIRED_UNITY_VERSION}.")
    else ()
        message(WARNING "Could not find desired Unity version (${DESIRED_UNITY_VERSION}).")
    endif ()
    if (NOT UNITY_DIRS STREQUAL "")
        list(GET UNITY_DIRS -1 UNITY_DIR)
        cmake_path(GET UNITY_DIR FILENAME UNITY_VERSION)
        cmake_path(APPEND UNITY_DIR "Editor/Data/PluginAPI")
        message(STATUS "Compiling against latest installed Unity version ${UNITY_VERSION} (${UNITY_DIR}).")
    endif ()
    if (NOT EXISTS ${UNITY_DIR})
        message(FATAL_ERROR "Please install the Unity Editor.")
    endif ()
endif ()

# Ensure DLSS was found
if (ENABLE_DLSS AND NOT EXISTS ${STREAMLINE_SDK_DIR})
    message(FATAL_ERROR "Please extract the latest Streamline SDK into '${STREAMLINE_SDK_DIR}'.")
endif ()

# Ensure FSR was found
if (ENABLE_FSR AND NOT EXISTS ${FFX_SDK_DIR})
    message(FATAL_ERROR "Please extract the latest FFX SDK into '${FFX_SDK_DIR}'.")
endif ()

# Ensure that at least one enabled graphics api supports at least one of the enabled upscalers.
set(IS_COMPATIBLE OFF)
if (NOT IS_COMPATIBLE AND ENABLE_DLSS)
    if (ENABLE_VULKAN OR ENABLE_DX12 OR ENABLE_DX11)
        set(IS_COMPATIBLE ON)
    endif ()
endif ()
if (NOT IS_COMPATIBLE AND ENABLE_FSR)
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
if (ENABLE_DLSS)
    add_library(streamline INTERFACE)
    target_include_directories(streamline INTERFACE "${STREAMLINE_SDK_DIR}/include")
    list(APPEND CONIFER_LIBRARIES streamline)
    set(STREAMLINE_DLL_DIR "${STREAMLINE_SDK_DIR}/bin/x64/")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(STREAMLINE_DLL_DIR "${STREAMLINE_DLL_DIR}development/")
    endif ()
    list(APPEND LIBRARIES_TO_COPY "${STREAMLINE_DLL_DIR}sl.interposer.dll" "${STREAMLINE_DLL_DIR}sl.common.dll" "${STREAMLINE_DLL_DIR}sl.dlss.dll" "${STREAMLINE_DLL_DIR}nvngx_dlss.dll")
endif ()

# Set up the FSR libraries
if (ENABLE_FSR)
    function (ConiferAddFSRLibrary LIBRARY_NAME LIBRARY_PATH LIBRARY_FILENAME)
        if (CMAKE_BUILD_TYPE STREQUAL Debug)
            set(LIBRARY_FILENAME "${LIBRARY_FILENAME}")
        endif ()
        add_library(${LIBRARY_NAME} SHARED IMPORTED)
        set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_LOCATION "${LIBRARY_PATH}/${LIBRARY_FILENAME}.dll")
        set_target_properties(${LIBRARY_NAME} PROPERTIES IMPORTED_IMPLIB "${LIBRARY_PATH}/${LIBRARY_FILENAME}.lib")
        target_include_directories(${LIBRARY_NAME} INTERFACE ${ARGN})
        list(APPEND CONIFER_LIBRARIES ${LIBRARY_NAME})
        set(CONIFER_LIBRARIES ${CONIFER_LIBRARIES} PARENT_SCOPE)
        list(APPEND LIBRARIES_TO_COPY "${LIBRARY_PATH}/${LIBRARY_FILENAME}.dll")
        set(LIBRARIES_TO_COPY ${LIBRARIES_TO_COPY} PARENT_SCOPE)
    endfunction ()

    if (ENABLE_VULKAN)
        ConiferAddFSRLibrary(fsr_vk "${FFX_SDK_DIR}/lib" "amd_fidelityfx_vk" "${FFX_SDK_DIR}/ffx_api" "${FFX_SDK_DIR}")
    endif ()
    if (ENABLE_DX12)
        ConiferAddFSRLibrary(fsr_dx12 "${FFX_SDK_DIR}/lib" "amd_fidelityfx_dx12" "${FFX_SDK_DIR}/ffx_api" "${FFX_SDK_DIR}")
    endif ()
endif ()

if (ENABLE_XESS)
    add_library(xess INTERFACE)
    target_include_directories(xess INTERFACE "${XESS_SDK_DIR}/inc")
    list(APPEND CONIFER_LIBRARIES xess)
    list(APPEND LIBRARIES_TO_COPY "${XESS_SDK_DIR}/bin/libxess.dll")
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
    set(DLSS_SOURCES Upscaler/DLSS_Upscaler.cpp)
endif ()
if (ENABLE_FSR)
    set(FSR_SOURCES Upscaler/FSR_Upscaler.cpp)
    if (ENABLE_FRAME_GENERATION)
        set(FSR_SOURCES ${FSR_SOURCES} FrameGenerator/FSR_FrameGenerator.cpp)
    endif ()
endif ()
if (ENABLE_XESS)
    set(XESS_SOURCES Upscaler/XeSS_Upscaler.cpp)
endif ()

# Add library and link Unity files
add_library(GfxPluginUpscaler main.cpp
        ${DLSS_SOURCES}
        ${FSR_SOURCES}
        ${XESS_SOURCES}

        ${DX11_SOURCES}
        ${DX12_SOURCES}
        ${VULKAN_SOURCES}

        GraphicsAPI/GraphicsAPI.cpp
        Upscaler/None_Upscaler.cpp
        Upscaler/Upscaler.cpp
        Plugin.hpp
        FrameGenerator/FrameGenerator.cpp
        FrameGenerator/FrameGenerator.hpp
)

add_custom_command(TARGET GfxPluginUpscaler PRE_BUILD COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --blue "Compiling against Unity version ${UNITY_VERSION}.")
target_compile_definitions(GfxPluginUpscaler PUBLIC VK_NO_PROTOTYPES VK_USE_PLATFORM_WIN32_KHR)
target_include_directories(GfxPluginUpscaler PUBLIC ${UNITY_DIR} ${CMAKE_SOURCE_DIR})
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
foreach (ITEM ENABLE_VULKAN;ENABLE_DX12;ENABLE_DX11;ENABLE_DLSS;ENABLE_FSR;ENABLE_XESS;ENABLE_FRAME_GENERATION)
    if (${ITEM})
        target_compile_definitions(GfxPluginUpscaler PUBLIC ${ITEM})
    endif ()
endforeach ()

# Copy the resulting shared library to the Unity Project's Asset/Plugins directory.
ConiferListToString("${LIBRARIES_TO_COPY}" LIBRARIES_TO_COPY_STRING)
add_custom_command(TARGET GfxPluginUpscaler POST_BUILD COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --blue "Copying ${LIBRARIES_TO_COPY_STRING} to ${PLUGINS_DIR}.")
add_custom_command(TARGET GfxPluginUpscaler POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${LIBRARIES_TO_COPY} ${PLUGINS_DIR})