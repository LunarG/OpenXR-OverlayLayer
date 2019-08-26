// Remote.cpp 

#include <windows.h>
#include <tchar.h>
#include <iostream>

#include "../XR_overlay_ext/xr_overlay_dll.h"

int main( void )
{
    DebugBreak();

    WCHAR cBuf[MAX_PATH];

    GetSharedMem(cBuf, MAX_PATH);
 
    printf("Child process read from shared memory: %S\n", cBuf);
    
    return 0;
}