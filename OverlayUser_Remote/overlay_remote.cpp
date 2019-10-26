// Remote.cpp 

#include <windows.h>
#include <tchar.h>
#include <iostream>

#include "../XR_overlay_ext/xr_overlay_dll.h"

int main( void )
{
    //DebugBreak();

    // WCHAR cBuf[MAX_PATH];
    // GetSharedMem(cBuf, MAX_PATH);
    // printf("Child process read from shared memory: %S\n", cBuf);


    void *shmem = IPCGetSharedMemory();
    unsigned char* packPtr = reinterpret_cast<unsigned char*>(shmem);

    uint64_t requestType = IPC_REQUEST_HANDOFF;
    size_t requestSize = 0;
    pack(packPtr, requestType);
    pack(packPtr, requestSize);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    unsigned char* unpackPtr = packPtr + requestSize;
    XrInstance instance = unpack<XrInstance>(unpackPtr);
    printf("instance is %p\n", instance);
    
    return 0;
}
