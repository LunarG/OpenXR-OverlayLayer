#
# Copyright 2019 LunarG Inc. and PlutoVR Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
# NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
# THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Author: Brad Grantham <brad@lunarg.com>
# Author: Dave Houlton <daveh@lunarg.com>
#

cmake_minimum_required(VERSION 3.12.2)

project(extx_overlay)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release"
    CACHE STRING "Configuration types" FORCE)

add_library(extx_overlay SHARED
  ../../external_headers/xr_generated_dispatch_table.h
  ../external_headers/xr_generated_dispatch_table.c
  xr_generated_overlays.cpp
  xr_generated_overlays.hpp
)

# Copy the api_layer_platform_defines.h file and place it in the binary (build) directory.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/api_layer_platform_defines.h ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

# Additional include directories
set_property(TARGET extx_overlay
  APPEND PROPERTY INCLUDE_DIRECTORIES
  $<$<CONFIG:Debug>:
    ${CMAKE_SOURCE_DIR}/external_headers>
  $<$<CONFIG:Release>:
    ${CMAKE_SOURCE_DIR}/external_headers>
)

# Preprocessor definitions
target_compile_definitions(extx_overlay PRIVATE
  $<$<CONFIG:Debug>:_UNICODE;_DEBUG;XR_OVERLAY_DLL_EXPORTS;_WINDOWS;_USRDLL>
  $<$<CONFIG:Release>:_UNICODE;NDEBUG;XR_OVERLAY_DLL_EXPORTS;_WINDOWS;_USRDLL>
)

# SDL check
target_compile_options(extx_overlay PRIVATE
  "$<$<CONFIG:Debug>:/sdl>"
  "$<$<CONFIG:Release>:/sdl>"
)

# Minimal rebuild
if (MSVC)
  target_compile_options(extx_overlay PRIVATE
    "$<$<CONFIG:Debug>:/Gm->"
    "$<$<CONFIG:Release>:/Gm->"
  )
endif ()

# Precompiled header files
if (MSVC)
  target_compile_options(extx_overlay PRIVATE
    ""
  )
endif ()

if(WIN32)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D11)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D12)
endif()
