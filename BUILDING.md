1) Create VS solution/project files with cmake:

```
mkdir build
cd build
cmake -G "your toolchain here" ..
```

2) Open solution file

3) To debug, all the executables and DLLs need to be co-located

```
Select Properties of XR_overlay_ext project
Edit Build Events -> Post-Build Event to "COPY /Y $(OutDir)\$(TargetFileName) $(SolutionDir)\OverlayUser_Host\$(Configuration)"
Select Properties of OverlayUser_Remote
Edit Build Events -> Post-Build Event to "COPY /Y $(OutDir)\$(TargetFileName) $(SolutionDir)\OverlayUser_Host\$(Configuration)"

Re-build and select OverlayUser_Host as the Startup Project
```

If your cmake-fu is strong, replace step 3 with cmake magic
