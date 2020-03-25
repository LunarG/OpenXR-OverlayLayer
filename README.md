# OpenXR `XR_EXTX_overlay` Test Implementation

## Introduction 

Existing XR APIs support the notion of 2D composition layers “on top” of an existing application’s content.  Valve’s `IVROverlay` is an existing example.

Overlay applications can add a rich variety of content into other XR Applications:
* Desktop OS windows in-world
* In-game HUD
* Virtual keyboard
* Chat (e.g. Pluto VR)

The `XR_EXTX_overlay` experimental extension to OpenXR is intended to prove the concept and uncover issues.  Some issues to address include:
* Establishing Security and access control requirements
* How to address input focus requirements

We hope OpenXR runtimes will incorporate a future version of the extension after any concerns are addressed and perhaps the extension is promoted to `EXT` or `KHR`.

## Implementation Specifics

The `XR_EXTX_overlay` test implementation for Windows and Direct3D 11 consists of an OpenXR API layer and a separate remote overlay application.  The layer and application are implemented against the OpenXR 1.0.8 specification.  The layer is named `XR_ext_overlay.dll` and the remote client is named `OverlayUser_Remote.exe`.  The API layer receives OpenXR commands over RPC (using shared memory) from the remote application.

The test implementation demonstrates the remote overlay client application opening an Overlay session within “unaware” host OpenXR applications.  At commit time we have tested only the OpenXR-SDK-Source `hello_xr` as the host application.

The overlay client executable can also be compiled as a standalone OpenXR program by changing the preprocessor symbol `COMPILE_REMOTE_OVERLAY_APP` to 0.  (Note that this may be visually disorienting in VR as there will be only the one layer and no other environmental cues.)

Development used CMake 3.12.4, although other versions may be compatible.

Building this test implementation used Visual Studio project files generated using the commands:

```
mkdir build
cd build
cmake -D FREEIMAGE_ROOT=freeimage-header-lib-dir -D FREEIMAGEPLUS_ROOT=freeimageplus-header-lib-dir -G "Visual Studio 15 2017" -A x64 ..
```

As an example, if building using `bash` in `mintty` (msysgit "Git Bash") and you have downloaded the FreeImage dist into `$HOME/Downloads` and unpacked it there, your `cmake` command may look like this:

```
cmake -D FREEIMAGE_ROOT=$HOME/Downloads/FreeImage/dist/x64 -D FREEIMAGEPLUS_ROOT=$HOME/Downloads/FreeImage/Wrapper/FreeImagePlus/dist/x64 -G "Visual Studio 15 2017" -A x64 ..
```

The layer DLLs and executables were compiled with Visual Studio 2017, version 15.9.12 in “Debug” configuration.  Only the 64-bit target is supported at this time.

## Operation

The implementation has been tested on Microsoft Windows Mixed Reality OpenXR Developer runtime version 100.1910.1004 and on Oculus OpenXR developer channel runtime 1.45p0 with `hello_xr` from https://github.com/KhronosGroup/OpenXR-SDK-Source/tree/release-1.0.7 .

Replace references to `$OVERLAY_PROJECT` below with the top-level directory name containing the clone of the overlay project repository.  (E.g. if you performed `git clone https://github.com/LunarG/OpenXR-OverlayLayer.git` from your home directory, this would be `$HOME/OpenXR-OverlayLayer`.)  Replace references to `$WORKING_DIRECTORY` to a directory name which will contain runtime files relevant to the test implementation.  (Perhaps `$HOME/Overlay_OpenXR`.)

Copy to `$WORKING_DIRECTORY` the following files:

```
$OVERLAY_PROJECT/build/OverlayUser_Remote/Debug/OverlayUser_Remote.exe
$OVERLAY_PROJECT/XR_overlay_ext/XrApiLayer_Overlay.json
$OVERLAY_PROJECT/build/XR_overlay_ext/Debug/XR_overlay_ext.dll
```

Build `src/tests/hello_xr` from the OpenXR repository.

Run hello_xr.exe with the following environment variables:

```
XR_ENABLE_API_LAYERS=XR_EXT_overlay_api_layer
XR_API_LAYER_PATH=$WORKING_DIRECTORY
```

You will also need to set the environment variables for finding the runtime Manifest (JSON) if not in the Windows Registry (e.g. for Oculus).

These can be set, for example, in the Visual Studio Preferences for debugging `hello_xr` in the “Environment” section, or, as another example, at the command-line before running `hello_xr`.  Also provide the command-line option `-g D3D11`, which can be set in the “Command Arguments” section of Preferences or on the command line.

Run `hello_xr`.  Run `OverlayUser_Remote.exe`.  If successful, the console output from `OverlayUser_Remote.exe` will contain various messages and finally should output “First Overlay xrEndFrame was successful!  Continuing...”

## Nota Bene

* Only functions called by `overlate_remote.cpp` have been implemented in RPC.  Notably the following families of functions are not implemented and will send a warning message to the debugger and then return the error `XR_ERROR_RUNTIME_FAILURE`:
  * Action and ActionSet and other input
  * Direct3D 12, OpenGL, and Vulkan-related functions
  * Haptics 
  * Convenience functions for producing strings from enumerants
  * Debug Utils

* Exiting the Microsoft Developer Portal may cause a Direct3D exception in the host program.  The cause is currently unknown.

* Only a single Overlay XrSession is supported at this time.

## Troubleshooting

If the test program or the loader encounter a problem, please open a new issue and attach the contents of the Output pane in Visual Studio for both `OverlayUser_Remote.exe` and `hello_xr.exe` and the console output from both programs for review.

Additional useful information may be found in the loader output after setting the environment variable `XR_LOADER_DEBUG` to the value “all” before running `hello_xr.exe`.

