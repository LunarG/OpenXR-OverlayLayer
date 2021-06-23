# OpenXR `XR_EXTX_overlay` Test Implementation

## Introduction 

Existing XR APIs support the notion of 2D composition layers “on top” of an existing application’s content.  Valve’s `IVROverlay` is an example.

Overlay applications can add a rich variety of content into other XR Applications:
* Chat (e.g. [Pluto VR](https://www.plutovr.com/))
* Desktop OS windows in-world
* In-game HUD
* Virtual keyboard

The `XR_EXTX_overlay` experimental extension to OpenXR is intended to prove the concept and uncover issues.  Some issues to address include:
* Establishing Security and access control requirements
* How to address input focus requirements

We hope OpenXR runtimes will incorporate a future version of the extension after any concerns are addressed and perhaps the extension is promoted to `EXT` or `KHR`.

## Implementation Specifics

The `XR_EXTX_overlay` test implementation for Windows and Direct3D 11 consists of an OpenXR API layer and a separate remote overlay test application.  The layer and application are implemented against the OpenXR 1.0.9 specification.  The layer is named `xr_extx_overlay.dll` and the overlay test is named `OverlaySample.exe`.  The API layer runs both in the main OpenXR app and the overlay app, and the instantiation of the API layer in the main app receives OpenXR commands over RPC (remote procedure calls using shared memory) from the instantiation of the API layer in the overlay app.

The test implementation demonstrates the remote overlay client application opening an Overlay session within “unaware” host OpenXR applications.  At commit time we have tested only the OpenXR-SDK-Source `hello_xr` as the host application.

The overlay test app can also be compiled as a standalone OpenXR program by changing the variable `createOverlaySession` in `main()` to false.  (Note that this may be visually disorienting in VR as there will be only the one layer and no other environmental cues.)

Development used CMake 3.14.6, although other versions may be compatible.

Building this test implementation used Visual Studio project files generated using the commands:

```
mkdir build
cd build
cmake -D OPENXR_SDK_SOURCE_ROOT=openxr-sdk-source-dir -D OPENXR_SDK_BUILD_SUBDIR=sdk-source-relative-build-dir -D OPENXR_LIB_DIR=built_openxr_loader_dir  -D FREEIMAGE_ROOT=freeimage-header-lib-dir -D FREEIMAGEPLUS_ROOT=freeimageplus-header-lib-dir -G visual-studio-generator-name -A x64 ..
```

As an example, if building using `bash` in `mintty` (msysgit "Git Bash"), you have downloaded the FreeImage dist into `$HOME/Downloads` and unpacked it there, and you're using Visual Studio 2017, your `cmake` command may look like this:

```
cmake -D OPENXR_SDK_SOURCE_ROOT=$HOME/trees/OpenXR-SDK-Source -D OPENXR_LIB_DIR=$HOME/trees/OpenXR-SDK-Source/build/win64/src/loader/Debug -D OPENXR_SDK_BUILD_SUBDIR=build/win64 -D FREEIMAGE_ROOT=$HOME/Downloads/FreeImage/dist/x64 -D FREEIMAGEPLUS_ROOT=$HOME/Downloads/FreeImage/Wrapper/FreeImagePlus/dist/x64 -G "Visual Studio 15 2017" -A x64 ..
```

The layer DLLs and executables were compiled with Visual Studio 2017, version 15.9.12 in “Debug” configuration.  Only the 64-bit target is supported at this time.

## Operation

The implementation has been tested on Microsoft Windows Mixed Reality OpenXR Developer runtime version 100.1910.1004 and on Oculus OpenXR developer channel runtime 1.52.0 with `hello_xr` from https://github.com/KhronosGroup/OpenXR-SDK-Source/tree/release-1.0.12 .

Replace references to `$OVERLAY_PROJECT` below with the top-level directory name containing the clone of the overlay project repository.  (E.g. if you performed `git clone https://github.com/LunarG/OpenXR-OverlayLayer.git` from your home directory, this would be `$HOME/OpenXR-OverlayLayer`.)  Replace references to `$WORKING_DIRECTORY` to a directory name which will contain runtime files relevant to the test implementation.  (Perhaps `$HOME/Overlay_OpenXR`.)

Copy to `$WORKING_DIRECTORY` the following files:

```
The openxr_loader.dll built from OpenXR-SDK-Source or other repository
$OVERLAY_PROJECT/build/overlay-sample/Debug/OverlaySample.exe
$OVERLAY_PROJECT/build/overlay-sample/*.png
$OVERLAY_PROJECT/api-layer/xr_extx_overlay.json
$OVERLAY_PROJECT/build/api-layer/Debug/xr_extx_overlay.dll
```

Build `src/tests/hello_xr` from the OpenXR repository.

Run hello_xr.exe with the following environment variables:

```
XR_ENABLE_API_LAYERS=xr_extx_overlay
XR_API_LAYER_PATH=$WORKING_DIRECTORY
```

You will also need to set the environment variables for finding the runtime Manifest (JSON) if not in the Windows Registry (e.g. for Oculus).

These can be set, for example, in the Visual Studio Preferences for debugging `hello_xr` in the “Environment” section, or, as another example, at the command-line before running `hello_xr`.  Also provide the command-line option `-g D3D11`, which can be set in the “Command Arguments” section of Preferences or on the command line.

Run `hello_xr`.  Run `OverlaySample.exe`.  If successful, the console output from `OverlaySample.exe` will contain various messages and finally should output “First Overlay xrEndFrame was successful!  Continuing...”

## Nota Bene

* The runtime’s `xrReleaseSwapchainImage` function may return `XR_ERROR_VALIDATION_FAILURE`, and OverlaySample.exe will break into the debugger if one is running. The reason is unknown.
* OverlaySample.exe does not suggest bindings for the Microsoft or Vive interaction profiles but instead suggests bindings for the “khr/simple_controller” profile. Probably OverlaySample.exe will need to have additional bindings added before being run on Microsoft or Vive runtimes. A workaround is to comment out the calls in openxr_program.cpp that suggest bindings for any profile but “khr/simple_controller”. This would not be an appropriate suggestion for a shipping application.

## Troubleshooting

If the test program or the loader encounter a problem, please open a new issue and attach the contents of the Output pane in Visual Studio for both `OverlaySample.exe` and `hello_xr.exe` and the console output from both programs for review.

Additional useful information may be found in the loader output after setting the environment variable `XR_LOADER_DEBUG` to the value “all” before running `hello_xr.exe`.

