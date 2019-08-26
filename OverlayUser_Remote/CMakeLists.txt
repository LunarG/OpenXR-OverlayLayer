cmake_minimum_required(VERSION 3.12.2)

project(OverlayUser_Remote)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release"
    CACHE STRING "Configuration types" FORCE)

add_executable(OverlayUser_Remote
  overlay_remote.cpp
)

set_target_properties(OverlayUser_Remote PROPERTIES FOLDER "Users")

# Additional include directories
set_property(TARGET OverlayUser_Remote
  APPEND PROPERTY INCLUDE_DIRECTORIES
  $<$<CONFIG:Debug>:
    ${CMAKE_SOURCE_DIR}/external_headers>
  $<$<CONFIG:Release>:
    ${CMAKE_SOURCE_DIR}/external_headers>
)

# Preprocessor definitions
target_compile_definitions(OverlayUser_Remote PRIVATE
  $<$<CONFIG:Debug>:_UNICODE;_DEBUG;_CONSOLE>
  $<$<CONFIG:Release>:_UNICODE;NDEBUG;_CONSOLE>
)

# SDL check
target_compile_options(OverlayUser_Remote PRIVATE
  "$<$<CONFIG:Debug>:/sdl>"
  "$<$<CONFIG:Release>:/sdl>"
)

# Minimal rebuild
if (MSVC)
  target_compile_options(OverlayUser_Remote PRIVATE
    "$<$<CONFIG:Debug>:/Gm->"
    "$<$<CONFIG:Release>:/Gm->"
  )
endif ()

# Precompiled header files
if (MSVC)
  target_compile_options(OverlayUser_Remote PRIVATE
    ""
  )
endif ()

# Additional library directories (broken CMake 3.12.4 ?)
# if (MSVC)
#   target_link_options(OverlayUser_Remote PRIVATE
#     $<$<CONFIG:Debug>:
#       /LIBPATH:${CMAKE_SOURCE_DIR}/x64/Debug>
#     $<$<CONFIG:Release>:
#       /LIBPATH:${CMAKE_SOURCE_DIR}/x64/Release>
#   )
# else ()
#   target_link_options(OverlayUser_Remote PRIVATE
#     $<$<CONFIG:Debug>:
#       -L${CMAKE_SOURCE_DIR}/x64/Debug>
#     $<$<CONFIG:Release>:
#       -L${CMAKE_SOURCE_DIR}/x64/Release>
#   )
# endif ()

# Link libraries
set_property(TARGET OverlayUser_Remote
  APPEND PROPERTY LINK_LIBRARIES
  "$<$<CONFIG:Debug>:XR_overlay_ext>"
  "$<$<CONFIG:Release>:XR_overlay_ext>"
)
