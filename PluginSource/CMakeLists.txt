cmake_minimum_required(VERSION 3.23)
project(Upscaler)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)

cmake_path(SET PLUGINS_DIR "${CMAKE_SOURCE_DIR}/../UnityProject/Assets/Plugins")

###########
# Options #
###########

# At least one graphics API must be selected.
option(ENABLE_VULKAN "Compiles with Vulkan support." ON)
option(ENABLE_DX12 "Compiles with DX12 support. Value is only respected on Windows operating systems." ON)
option(ENABLE_DX11 "Compiles with DX11 support. Value is only respected on Windows operating systems." ON)

# At least one upscaler must be selected.
option(ENABLE_DLSS "Compiles with DLSS support. Value is ignored on macOS operating systems." ON)
option(ENABLE_FSR2 "Compiles with FSR2 support. Value is ignored on macOS operating systems." ON)

######################
# Paths and Warnings #
######################
if (ENABLE_VULKAN)
    find_package(Vulkan)
endif ()

# Ensure system supports compilation definitions
if (NOT Vulkan_FOUND)
    set(ENABLE_VULKAN OFF)
endif ()
if (NOT WIN32)
    set(ENABLE_DX12 OFF)
    set(ENABLE_DX11 OFF)
endif ()
if (APPLE)
    set(ENABLE_DLSS OFF)
    set(ENABLE_FSR2 OFF)
endif ()

# Fail if no upscaler was selected
if (NOT ENABLE_DLSS AND NOT ENABLE_FSR2)
    message(FATAL_ERROR "No upscaler was selected.")
endif ()

# Fail if no graphics API was selected
if (NOT ENABLE_VULKAN AND NOT ENABLE_DX12 AND NOT ENABLE_DX11)
    message(FATAL_ERROR "No graphics API was selected.")
endif ()

# If DLSS was enabled ensure that its SDK exists
if (ENABLE_DLSS)
    cmake_path(SET NGX_SDK_DIR "${CMAKE_SOURCE_DIR}/nvngx_dlss_sdk")
endif ()

# If FSR2 was enabled ensure that its SDK exists
if (ENABLE_FSR2)
    cmake_path(SET FFX_SDK_DIR "${CMAKE_SOURCE_DIR}/ffx_sdk")
endif ()

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
    list(GET UNITY_FILES 0 UNITY_FILES)
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
    message(FATAL_ERROR "Please extract the latest DLSS SDK into 'nvngx_dlss_sdk'.")
endif ()

# Ensure that at least one graphics API is selected
if (NOT ENABLE_VULKAN AND NOT ENABLE_DX12 AND NOT ENABLE_DX11)
    message(FATAL_ERROR "No graphics API was selected.")
endif ()

# Ensure that at least one upscaler is selected
if (NOT ENABLE_DLSS)
    message(FATAL_ERROR "No upscaler was selected.")
endif ()

# Ensure that at least one enabled graphics api supports at least one of the enabled upscalers.
set(IS_COMPATIBLE OFF)
if (NOT IS_COMPATIBLE AND ENABLE_DLSS)
    if (ENABLE_VULKAN OR ENABLE_DX12 OR ENABLE_DX11)
        set(IS_COMPATIBLE ON)
    endif ()
endif ()
if (NOT IS_COMPATIBLE)
    message(FATAL_ERROR "No selected upscaler is compatible with any selected graphics APIs.")
endif ()

#################
# Add libraries #
#################

if (WIN32)
    set(SDK_OS_PATH "Windows_x86_64")
elseif (UNIX AND NOT APPLE)
    set(SDK_OS_PATH "Linux_x86_64")
endif ()

# Set up the DLSS library
if (ENABLE_DLSS)
    add_library(dlss IMPORTED STATIC GLOBAL)
    target_include_directories(dlss INTERFACE "${NGX_SDK_DIR}/include")

    cmake_path(SET DLSS_SHARED_LIB_DIR "${NGX_SDK_DIR}/lib/${SDK_OS_PATH}")
    cmake_path(SET DLSS_STATIC_LIB_DIR "${NGX_SDK_DIR}/lib/${SDK_OS_PATH}")
    if (WIN32)
        cmake_path(APPEND DLSS_STATIC_LIB_DIR "x86_64")
    endif ()
    if (BUILD_SHARED_LIBS)
        set(GLOBBING_EXPRESSION "*d")
    else ()
        set(GLOBBING_EXPRESSION "*s")
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        cmake_path(APPEND DLSS_SHARED_LIB_DIR "dev")
        string(APPEND GLOBBING_EXPRESSION "_dbg.lib")
    else ()
        cmake_path(APPEND DLSS_SHARED_LIB_DIR "rel")
        string(APPEND GLOBBING_EXPRESSION ".lib")
    endif ()
    if (UNIX AND NOT APPLE)
        set(GLOBBING_EXPRESSION "*")
    endif ()

    file(GLOB DLSS_SHARED_LIB LIST_DIRECTORIES FALSE "${DLSS_SHARED_LIB_DIR}/*")
    list(APPEND SHARED_LIBRARIES ${DLSS_SHARED_LIB})
    set_property(TARGET dlss APPEND PROPERTY EXTRA_DLLS "${DLSS_SHARED_LIB}")

    file(GLOB DLSS_STATIC_LIB LIST_DIRECTORIES FALSE "${DLSS_STATIC_LIB_DIR}/${GLOBBING_EXPRESSION}")
    set_target_properties(dlss PROPERTIES IMPORTED_LOCATION "${DLSS_STATIC_LIB}")
endif ()

# Set up the FSR2 library
if (ENABLE_FSR2)
    add_library(fsr2_core IMPORTED SHARED)
    target_include_directories(fsr2_core INTERFACE "${FFX_SDK_DIR}" "${FFX_SDK_DIR}/FidelityFX/host")

    set(FSR2_FILENAME "ffx_fsr2_x86_64")
    cmake_path(SET FFX_STATIC_LIB_DIR "${FFX_SDK_DIR}/lib/${SDK_OS_PATH}")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        cmake_path(SET FFX_SHARED_LIB_DIR "${FFX_STATIC_LIB_DIR}/dev")
        set(FSR2_FILENAME "${FSR2_FILENAME}d")
    else ()
        cmake_path(SET FFX_SHARED_LIB_DIR "${FFX_STATIC_LIB_DIR}/rel")
    endif ()
    if (WIN32)
        cmake_path(SET FSR2_SHARED_LIB "${FFX_SHARED_LIB_DIR}/${FSR2_FILENAME}.dll")
        cmake_path(SET FSR2_STATIC_LIB "${FFX_STATIC_LIB_DIR}/${FSR2_FILENAME}.lib")
    elseif (UNIX AND NOT APPLE)
        cmake_path(SET FSR2_SHARED_LIB "${FFX_SHARED_LIB_DIR}/${FSR2_FILENAME}.so")
    endif ()

    list(APPEND SHARED_LIBRARIES ${FSR2_SHARED_LIB})
    set_target_properties(fsr2_core PROPERTIES IMPORTED_LOCATION "${FSR2_SHARED_LIB}")
    if (FSR2_STATIC_LIB)
        set_target_properties(fsr2_core PROPERTIES IMPORTED_IMPLIB "${FSR2_STATIC_LIB}")
    endif ()
    set(fsr2 "fsr2_core")

    if (ENABLE_VULKAN)
        add_library(ffx_vk IMPORTED SHARED)
        target_include_directories(ffx_vk INTERFACE "${FFX_SDK_DIR}/FidelityFX/host/backends/vk")

        set(FFX_VK_FILENAME "ffx_backend_vk_x86_64")
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(FFX_VK_FILENAME "${FFX_VK_FILENAME}d")
        endif ()
        if (WIN32)
            set(FFX_VK_SHARED_LIB "${FFX_SHARED_LIB_DIR}/${FFX_VK_FILENAME}.dll")
            set(FFX_VK_STATIC_LIB "${FFX_STATIC_LIB_DIR}/${FFX_VK_FILENAME}.lib")
        elseif (UNIX AND NOT APPLE)
            set(FFX_VK_SHARED_LIB "${FFX_SHARED_LIB_DIR}/${FFX_VK_FILENAME}.so")
        endif ()

        list(APPEND SHARED_LIBRARIES ${FFX_VK_SHARED_LIB})
        set_target_properties(ffx_vk PROPERTIES IMPORTED_LOCATION "${FFX_VK_SHARED_LIB}")
        if (FFX_VK_STATIC_LIB)
            set_target_properties(ffx_vk PROPERTIES IMPORTED_IMPLIB "${FFX_VK_STATIC_LIB}")
        endif ()
        if (NOT WIN32)
            target_link_options(ffx_vk INTERFACE "-fPIC")
        endif ()
        list(APPEND fsr2 "ffx_vk")
    endif ()

    if (ENABLE_DX12)
        add_library(ffx_dx12 IMPORTED SHARED)
        target_include_directories(ffx_dx12 INTERFACE "${FFX_SDK_DIR}/FidelityFX/host/backends/dx12")

        set(FFX_DX12_FILENAME "ffx_backend_dx12_x86_64")
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(FFX_DX12_FILENAME "${FFX_DX12_FILENAME}d")
        endif ()

        set(FFX_DX12_SHARED_LIB "${FFX_SHARED_LIB_DIR}/${FFX_DX12_FILENAME}.dll")
        set(FFX_DX12_STATIC_LIB "${FFX_STATIC_LIB_DIR}/${FFX_DX12_FILENAME}.lib")

        list(APPEND SHARED_LIBRARIES ${FFX_DX12_SHARED_LIB})
        set_target_properties(ffx_dx12 PROPERTIES IMPORTED_LOCATION "${FFX_DX12_SHARED_LIB}")
        set_target_properties(ffx_dx12 PROPERTIES IMPORTED_IMPLIB "${FFX_DX12_STATIC_LIB}")
        list(APPEND fsr2 "ffx_dx12")
    endif ()
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

# Add library and link Unity files
add_library(GfxPluginUpscaler main.cpp
        ${DLSS_SOURCES}
        ${FSR2_SOURCES}

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

# Link selected graphics APIs
if (ENABLE_VULKAN)
    target_include_directories(GfxPluginUpscaler PUBLIC ${Vulkan_INCLUDE_DIR})
endif ()

# Link selected upscaler libraries
if (ENABLE_DLSS)
    target_link_libraries(GfxPluginUpscaler dlss)
endif ()

if (ENABLE_FSR2)
    target_link_libraries(GfxPluginUpscaler ${fsr2})
endif ()

# Add compile definitions for selected graphics APIs
if (ENABLE_VULKAN)
    target_compile_definitions(GfxPluginUpscaler PUBLIC ENABLE_VULKAN=${ENABLE_VULKAN})
endif ()

if (ENABLE_DX12)
    target_compile_definitions(GfxPluginUpscaler PUBLIC ENABLE_DX12=${ENABLE_DX12})
endif ()

if (ENABLE_DX11)
    target_compile_definitions(GfxPluginUpscaler PUBLIC ENABLE_DX11=${ENABLE_DX11})
endif ()

# Add compile definitions for selected upscalers
if (ENABLE_DLSS)
    target_compile_definitions(GfxPluginUpscaler PUBLIC ENABLE_DLSS=${ENABLE_DLSS})
endif ()

if (ENABLE_FSR2)
    target_compile_definitions(GfxPluginUpscaler PUBLIC ENABLE_FSR2=${ENABLE_FSR2})
endif ()

# Copy the resulting shared library to the Unity Project's Asset/Plugins directory.
add_custom_command(TARGET GfxPluginUpscaler POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SHARED_LIBRARIES} ${PLUGINS_DIR} COMMENT "Copying shared libraries. Please restart Unity.")