// xr_overlay_dll.cpp : Defines the exported functions for the DLL application.
//

#include "xr_overlay_dll.h"

// The DLL code
#include <memory.h> 
 
static size_t  mem_size = 256;   // TBD HACK! - can't default, need to pass via shared_mem meta
static LPVOID shared_mem = NULL;        // pointer to shared memory
static HANDLE shared_mem_handle = NULL; // handle to file mapping
static HANDLE mutex_handle = NULL;      // handle to sync object

LPCWSTR kSharedMemName = TEXT("XR_EXT_overlay_shared_mem");
LPCWSTR kSharedMutexName = TEXT("XR_EXT_overlay_mutex");
LPCWSTR kHost = TEXT("XR_EXT_overlay_mutex");
LPCWSTR kGuestRequestSemaName = TEXT("LUNARG_XR_IPC_guest_request_sema");
LPCWSTR kHostResponseSemaName = TEXT("LUNARG_XR_IPC_host_response_sema");
HANDLE gGuestRequestSema;
HANDLE gHostResponseSema;

static const DWORD GUEST_REQUEST_WAIT_MILLIS = 100000; // 1000 // XXX debugging
static const DWORD HOST_RESPONSE_WAIT_MILLIS = 100000; // 1000 // XXX debugging

XR_OVERLAY_EXT_API IPCBuffer IPCGetBuffer()
{
    return IPCBuffer(shared_mem, mem_size);
}

#ifdef __cplusplus    // If used by C++ code, 
extern "C" {          // we need to export the C interface
#endif

bool CreateIPCSemaphores()
{
    gGuestRequestSema = CreateSemaphore(nullptr, 0, 1, kGuestRequestSemaName);
    if(gGuestRequestSema == NULL) {
        OutputDebugStringA("Creation of gGuestRequestSema failed");
        DebugBreak();
        return false;
    }
    gHostResponseSema = CreateSemaphore(nullptr, 0, 1, kHostResponseSemaName);
    if(gHostResponseSema == NULL) {
        OutputDebugStringA("Creation of gHostResponseSema failed");
        DebugBreak();
        return false;
    }
	return true;
}

void* IPCGetSharedMemory()
{
    return shared_mem;
}

// Call from Guest when request in shmem is complete
void IPCFinishGuestRequest()
{
    ReleaseSemaphore(gGuestRequestSema, 1, nullptr);
}

// Call from Host to get complete request in shmem
bool IPCWaitForGuestRequest()
{
    WaitForSingleObject(gGuestRequestSema, GUEST_REQUEST_WAIT_MILLIS);
    // XXX TODO something sane on very long timeout
	return true;
}

// Call from Host when response in shmem is complete
void IPCFinishHostResponse()
{
    ReleaseSemaphore(gHostResponseSema, 1, nullptr);
}

// Call from Guest to get complete request in shmem
bool IPCWaitForHostResponse()
{
    WaitForSingleObject(gHostResponseSema, HOST_RESPONSE_WAIT_MILLIS);
    // XXX TODO something sane on very long timeout
	return true;
}


// Set up shared memory using a named file-mapping object. 
bool MapSharedMemory(UINT32 req_memsize)
{ 
    mutex_handle = CreateMutex(NULL, TRUE, kSharedMutexName);
    if (NULL == mutex_handle) return false;
    bool first = (GetLastError() != ERROR_ALREADY_EXISTS); 

    shared_mem_handle = CreateFileMapping( 
        INVALID_HANDLE_VALUE,   // use sys paging file instead of an existing file
        NULL,                   // default security attributes
        PAGE_READWRITE,         // read/write access
        0,                      // size: high 32-bits
        req_memsize,            // size: low 32-bits
        kSharedMemName);        // name of map object

    if (NULL == shared_mem_handle)
    {
        if (first) ReleaseMutex(mutex_handle); 
        CloseHandle(mutex_handle);
        return false; 
    }

    // Get a pointer to the file-mapped shared memory, read/write
    shared_mem = MapViewOfFile(shared_mem_handle, FILE_MAP_WRITE, 0, 0, 0);
    if (NULL == shared_mem) 
    {
        if (first) ReleaseMutex(mutex_handle); 
        CloseHandle(mutex_handle);
        return false; 
    }

    MEMORY_BASIC_INFORMATION mbi = { 0 };
    VirtualQueryEx(GetCurrentProcess(), shared_mem, &mbi, sizeof(mbi));
    mem_size = mbi.RegionSize;

    // First will initialize memory
    if (first)
    {
        memset(shared_mem, '\0', mem_size); 
        ReleaseMutex(mutex_handle);
    }
    
    return true;
}

// Unmap the shared memory and release handle
//
bool UnmapSharedMemory()
{
    // Close handle to mutex
    CloseHandle(mutex_handle);

    // Unmap shared memory from the process's address space
    bool err = UnmapViewOfFile(shared_mem); 
 
    // Close the process's handle to the file-mapping object
    if (!err) err = CloseHandle(shared_mem_handle);

    return err;
} 


// SetSharedMem sets the contents of the shared memory 
// 
void SetSharedMem(LPCWSTR lpszBuf) 
{ 
    WaitForSingleObject(mutex_handle, INFINITE);
    LPWSTR lpszTmp; 
    DWORD dwCount = 1;  // reserve for null terminator
 
    // Get the address of the shared memory block
    lpszTmp = (LPWSTR) shared_mem; 
 
    // Copy the null-terminated string into shared memory
    while (*lpszBuf && (dwCount < mem_size)) 
    {
        *lpszTmp++ = *lpszBuf++; 
        dwCount++;
    }
    *lpszTmp = '\0'; 

    ReleaseMutex(mutex_handle);
} 
 
// GetSharedMem gets the contents of the shared memory
// 
void GetSharedMem(LPWSTR lpszBuf, size_t cchSize) 
{ 
    WaitForSingleObject(mutex_handle, INFINITE);

    LPWSTR lpszTmp; 
 
    if (cchSize >= mem_size) cchSize = mem_size - 1;

    // Get the address of the shared memory block
    lpszTmp = (LPWSTR) shared_mem; 
 
    // Copy from shared memory into the caller's buffer
    while (*lpszTmp && --cchSize) 
        *lpszBuf++ = *lpszTmp++; 
    *lpszBuf = '\0'; 

    ReleaseMutex(mutex_handle);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        BOOL success = CreateIPCSemaphores();
        if(success) {
            MapSharedMemory(32768);
        }
        break;
    }

    case DLL_PROCESS_DETACH:
        UnmapSharedMemory();
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}

#ifdef __cplusplus
}
#endif
