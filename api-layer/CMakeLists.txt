# 
# Copyright (c) 2019-2021 LunarG Inc. and PlutoVR Inc.
# Copyright (c) 2017-2020 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: Brad Grantham <brad@lunarg.com>
# Author: Dave Houlton <daveh@lunarg.com>
# Author: John Zulauf <jzulauf@lunarg.com>
#
# Author: others not specified in original file
#

cmake_minimum_required(VERSION 3.12.2)

# find openxr - OPENXR_SDK_SOURCE_ROOT
if(NOT DEFINED OPENXR_SDK_SOURCE_ROOT OR
   NOT DEFINED OPENXR_INCLUDE_DIR)
    message(
        FATAL_ERROR
            "Please generate projects from parent directory"
    )
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    set(CODEGEN_PYTHON_PATH
        "${OPENXR_SDK_SOURCE_ROOT}/specification/scripts;${OPENXR_SDK_SOURCE_ROOT}/src/scripts;$ENV{PYTHONPATH}"
    )
else()
    set(CODEGEN_PYTHON_PATH
        "${OPENXR_SDK_SOURCE_ROOT}/specification/scripts:${OPENXR_SDK_SOURCE_ROOT}/src/scripts:$ENV{PYTHONPATH}"
    )
endif()

macro(run_overlay_layer_generator generator output)
    if(NOT PYTHON_EXECUTABLE)
        message(
            FATAL_ERROR
                "Python 3 not found, but pre-generated ${CMAKE_CURRENT_SOURCE_DIR}/${output} not found"
        )
    endif()
    add_custom_command(
        OUTPUT ${output}
        COMMAND
            ${CMAKE_COMMAND} -E env "PYTHONPATH=${CODEGEN_PYTHON_PATH}"
            ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/${generator}
            ${OPENXR_SDK_SOURCE_ROOT}/specification/registry/xr.xml
            ${output}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS "${OPENXR_SDK_SOURCE_ROOT}/specification/registry/xr.xml"
                "${CMAKE_CURRENT_SOURCE_DIR}/${generator}"
                ${ARGN}
        COMMENT "Generating ${output} using ${PYTHON_EXECUTABLE} on ${generator}"
    )
    set_source_files_properties(${output} PROPERTIES GENERATED TRUE)
    list(APPEND GENERATED_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${output}")
    list(APPEND GENERATED_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${output}")
endmacro()

# Force all compilers to output to binary folder without additional output (like Windows adds "Debug" and "Release" folders)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

project(xr_extx_overlay)

# Copy the api_layer_platform_defines.h file and place it in the binary (build) directory.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/api_layer_platform_defines.h ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

set(GENERATED_OUTPUT)
set(GENERATED_DEPENDS)
run_overlay_layer_generator(generate.py xr_generated_overlays.cpp)
run_overlay_layer_generator(generate.py xr_generated_overlays.hpp)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release"
    CACHE STRING "Configuration types" FORCE)

add_library(xr_extx_overlay SHARED
    ${OPENXR_SDK_SOURCE_ROOT}/${OPENXR_SDK_BUILD_SUBDIR}/src/xr_generated_dispatch_table.h
    ${OPENXR_SDK_SOURCE_ROOT}/${OPENXR_SDK_BUILD_SUBDIR}/src/xr_generated_dispatch_table.c
    ${OPENXR_SDK_SOURCE_ROOT}/src/common/hex_and_handles.h
    overlays.cpp
    adjusted_ref_space.cpp
    ${GENERATED_OUTPUT}
)

target_include_directories(xr_extx_overlay
    PRIVATE
    ${OPENXR_SDK_SOURCE_ROOT}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}

    # for OpenXR headers
    ${OPENXR_SDK_SOURCE_ROOT}/${OPENXR_SDK_BUILD_SUBDIR}/include
    ${OPENXR_INCLUDE_DIR}

    # for generated dispatch table
    ${OPENXR_SDK_SOURCE_ROOT}/${OPENXR_SDK_BUILD_SUBDIR}/src

    # for target-specific generated files
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Additional include directories
# set_property(TARGET xr_extx_overlay
# APPEND PROPERTY INCLUDE_DIRECTORIES
# $<$<CONFIG:Debug>:
# ${CMAKE_SOURCE_DIR}/external_headers>
# $<$<CONFIG:Release>:
# ${CMAKE_SOURCE_DIR}/external_headers>
# )

# Preprocessor definitions
target_compile_definitions(xr_extx_overlay PRIVATE
  $<$<CONFIG:Debug>:_UNICODE;_DEBUG;XR_OVERLAY_DLL_EXPORTS;_WINDOWS;_USRDLL>
  $<$<CONFIG:Release>:_UNICODE;NDEBUG;XR_OVERLAY_DLL_EXPORTS;_WINDOWS;_USRDLL>
)

if (MSVC)
    # SDL check
    target_compile_options(xr_extx_overlay PRIVATE
      "$<$<CONFIG:Debug>:/sdl>"
      "$<$<CONFIG:Release>:/sdl>"
    )

    # Minimal rebuild
    target_compile_options(xr_extx_overlay PRIVATE
      "$<$<CONFIG:Debug>:/Gm->"
      "$<$<CONFIG:Release>:/Gm->"
    )

endif ()

if(WIN32)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D11)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D12)
endif()

if(WIN32)
    # Windows-specific information
    target_compile_definitions(xr_extx_overlay PRIVATE _CRT_SECURE_NO_WARNINGS)

    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    # target_compile_options(xr_extx_overlay PRIVATE "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>")

    # FILE(TO_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_api_dump.def DEF_FILE)
    # add_custom_target(copy-api_dump-def-file ALL
        # COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEF_FILE} ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.def
        # VERBATIM
    # )
        # set_target_properties(copy-api_dump-def-file PROPERTIES FOLDER ${HELPER_FOLDER})
endif()

set_property(TARGET xr_extx_overlay PROPERTY CXX_STANDARD 17)

