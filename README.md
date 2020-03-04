# OpenXR-OverlayLayer
Implementation of the OpenXR Overlay extension as a layer

Development used
CMake 3.12.4, although other versions may be compatible.

Building this test implementation used Visual Studio project files generated using the commands:

* mkdir build
* cd build
* cmake -D FREEIMAGE_ROOT=freeimage-header-lib-dir -D FREEIMAGEPLUS_ROOT=freeimageplus-header-lib-dir -G "Visual Studio 15 2017" -A x64 ..

As an example, if building using `bash` in `mintty` (msysgit "Git Bash") and you have downloaded the FreeImage dist into `$HOME/Downloads` and unpacked it there, your `cmake` command will look like this:

* cmake -D FREEIMAGE_ROOT=C:/Users/grantham/Downloads/FreeImage/dist/x64 -D FREEIMAGEPLUS_ROOT=$HOME/Downloads/FreeImage/Wrapper/FreeImagePlus/dist/x64 -G "Visual Studio 15 2017" -A x64 ..

The layer DLLs and executables were compiled with Visual Studio 2017, version 15.9.12 in 64-bit “Debug” configuration and have not been tested as a 32-bit or Release build.

