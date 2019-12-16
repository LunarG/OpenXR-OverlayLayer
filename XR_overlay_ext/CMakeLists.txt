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

project(XR_overlay_ext)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release"
    CACHE STRING "Configuration types" FORCE)

add_library(XR_overlay_ext SHARED
  ../../external_headers/xr_generated_dispatch_table.h
  ../external_headers/xr_generated_dispatch_table.c
  xr_layer_interface.cpp
  xr_overlay_dll.cpp
  xr_overlay_dll.h
)

# Additional include directories
set_property(TARGET XR_overlay_ext
  APPEND PROPERTY INCLUDE_DIRECTORIES
  $<$<CONFIG:Debug>:
    ${CMAKE_SOURCE_DIR}/external_headers>
  $<$<CONFIG:Release>:
    ${CMAKE_SOURCE_DIR}/external_headers>
)

# Preprocessor definitions
target_compile_definitions(XR_overlay_ext PRIVATE
  $<$<CONFIG:Debug>:_UNICODE;_DEBUG;XR_OVERLAY_DLL_EXPORTS;_WINDOWS;_USRDLL>
  $<$<CONFIG:Release>:_UNICODE;NDEBUG;XR_OVERLAY_DLL_EXPORTS;_WINDOWS;_USRDLL>
)

# SDL check
target_compile_options(XR_overlay_ext PRIVATE
  "$<$<CONFIG:Debug>:/sdl>"
  "$<$<CONFIG:Release>:/sdl>"
)

# Minimal rebuild
if (MSVC)
  target_compile_options(XR_overlay_ext PRIVATE
    "$<$<CONFIG:Debug>:/Gm->"
    "$<$<CONFIG:Release>:/Gm->"
  )
endif ()

# Precompiled header files
if (MSVC)
  target_compile_options(XR_overlay_ext PRIVATE
    ""
  )
endif ()

find_package(OpenGL)
if (OPENGL_FOUND)
    add_definitions(-DXR_USE_GRAPHICS_API_OPENGL)
elseif(BUILD_ALL_EXTENSIONS)
    message(FATAL_ERROR "OpenGL not found")
endif()

if(WIN32)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D11)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D12)
endif()

