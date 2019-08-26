cmake_minimum_required(VERSION 3.12.2)

project(XR_overlay)

add_subdirectory(XR_overlay_ext)
add_subdirectory(OverlayUser_Host)
add_subdirectory(OverlayUser_Remote)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake targets")

