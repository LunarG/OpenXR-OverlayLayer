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
