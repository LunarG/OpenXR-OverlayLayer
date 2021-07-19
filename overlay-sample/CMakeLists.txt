# 
# Copyright (c) 2020-2021 LunarG Inc. and PlutoVR Inc.
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
# Author: Michael Boone <fuag155555@gmail.com>
#

cmake_minimum_required(VERSION 3.12.2)

project(OverlaySample)

if(NOT DEFINED OPENXR_LIB_DIR OR
   NOT DEFINED OPENXR_INCLUDE_DIR)
    message(
        FATAL_ERROR
            "Please generate projects from parent directory"
    )
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release"
    CACHE STRING "Configuration types" FORCE)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(FREEIMAGE_FIND_REQUIRED, TRUE)
find_package(FreeImage)

set(FREEIMAGEPLUS_FIND_REQUIRED, TRUE)
find_package(FreeImagePlus)

add_executable(OverlaySample
  overlay_sample.cpp
)

target_include_directories(OverlaySample SYSTEM PRIVATE ${FREEIMAGE_INCLUDE_DIRS})
target_link_libraries(OverlaySample PRIVATE ${FREEIMAGE_LIBRARIES})

target_include_directories(OverlaySample SYSTEM PRIVATE ${FREEIMAGEPLUS_INCLUDE_DIRS})
target_link_libraries(OverlaySample PRIVATE ${FREEIMAGEPLUS_LIBRARIES})

set_target_properties(OverlaySample PROPERTIES FOLDER "Sample")
set_target_properties(OverlaySample PROPERTIES CXX_STANDARD 17)
target_link_libraries(OverlaySample PRIVATE debug openxr_loader${OPENXR_DEBUG_POSTFIX} optimized openxr)

# Additional include directories
set_property(TARGET OverlaySample
  APPEND PROPERTY INCLUDE_DIRECTORIES
  $<$<CONFIG:Debug>:
    ${CMAKE_SOURCE_DIR}/external_headers>
  $<$<CONFIG:Release>:
    ${CMAKE_SOURCE_DIR}/external_headers>
)

# Preprocessor definitions
target_compile_definitions(OverlaySample PRIVATE
  $<$<CONFIG:Debug>:_UNICODE;_DEBUG;_CONSOLE>
  $<$<CONFIG:Release>:_UNICODE;NDEBUG;_CONSOLE>
)

# SDL check
target_compile_options(OverlaySample PRIVATE
  "$<$<CONFIG:Debug>:/sdl>"
  "$<$<CONFIG:Release>:/sdl>"
)

# Minimal rebuild
if (MSVC)
  target_compile_options(OverlaySample PRIVATE
    "$<$<CONFIG:Debug>:/Gm->"
    "$<$<CONFIG:Release>:/Gm->"
  )
endif ()

# Precompiled header files
if (MSVC)
  target_compile_options(OverlaySample PRIVATE
    ""
  )
endif ()

# Additional library directories (broken CMake 3.12.4 ?)
# if (MSVC)
#   target_link_options(OverlaySample PRIVATE
#     $<$<CONFIG:Debug>:
#       /LIBPATH:${CMAKE_SOURCE_DIR}/x64/Debug>
#     $<$<CONFIG:Release>:
#       /LIBPATH:${CMAKE_SOURCE_DIR}/x64/Release>
#   )
# else ()
#   target_link_options(OverlaySample PRIVATE
#     $<$<CONFIG:Debug>:
#       -L${CMAKE_SOURCE_DIR}/x64/Debug>
#     $<$<CONFIG:Release>:
#       -L${CMAKE_SOURCE_DIR}/x64/Release>
#   )
# endif ()

# Link libraries
set_property(TARGET OverlaySample
  APPEND PROPERTY LINK_LIBRARIES
  "$<$<CONFIG:Debug>:d3d11>"
  "$<$<CONFIG:Release>:d3d11>"
  "$<$<CONFIG:Debug>:dxgi>"
  "$<$<CONFIG:Release>:dxgi>"
)

if(WIN32)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D11)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D12)
endif()

file(COPY assets/avatar1.png DESTINATION ${CMAKE_BINARY_DIR}/overlay-sample)
file(COPY assets/avatar2.png DESTINATION ${CMAKE_BINARY_DIR}/overlay-sample)
file(COPY assets/highlighted1.png DESTINATION ${CMAKE_BINARY_DIR}/overlay-sample)
file(COPY assets/selected1.png DESTINATION ${CMAKE_BINARY_DIR}/overlay-sample)
