# 
# Copyright (c) 2019-2021 LunarG Inc. and PlutoVR Inc.
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

cmake_minimum_required(VERSION 3.12.2)

find_package(PythonInterp 3)

# find openxr - OPENXR_SDK_SOURCE_ROOT
# TODO: move this as well as library related vars to a find module
if(NOT DEFINED OPENXR_SDK_SOURCE_ROOT)
    message(
        FATAL_ERROR
            "Set OPENXR_SDK_SOURCE_ROOT to root of OpenXR-SDK-Source 1.0.9 tree with built header and global_generaed_files"
    )
endif()

if(NOT OPENXR_LIB_DIR)
    message(
        FATAL_ERROR
            "Set OPENXR_LIB_DIR to desired built openxr loader"
    )
endif()

if(NOT DEFINED OPENXR_SDK_BUILD_SUBDIR)
    message(
        FATAL_ERROR
        "Set OPENXR_SDK_BUILD_SUBDIR to build subdirectory"
    )
endif()


# Duplicate rules from OpenXR-SDK-Source CMake
if(WIN32)
    set(OPENXR_DEBUG_POSTFIX d CACHE STRING "OpenXR loader debug postfix.")
else()
    set(OPENXR_DEBUG_POSTFIX "" CACHE STRING "OpenXR loader debug postfix.")
endif()

link_directories( ${OPENXR_LIB_DIR})

set(OPENXR_INCLUDE_DIR ${OPENXR_SDK_SOURCE_ROOT}/include/openxr)

project(XR_overlay)

add_subdirectory(overlay-sample)
add_subdirectory(api-layer)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake targets")

