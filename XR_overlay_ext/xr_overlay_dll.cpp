//
// Copyright 2019 LunarG Inc. and PlutoVR Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Author: Brad Grantham <brad@lunarg.com>
// Author: Dave Houlton <daveh@lunarg.com>
//

// xr_overlay_dll.cpp : Defines the exported functions for the DLL application.
//

#include "xr_overlay_dll.h"
#include <string>
#include <memory>

// The DLL code
#include <memory.h> 
 
static size_t  mem_size = 256;   // TBD HACK! - can't default, need to pass via shared_mem meta
static LPVOID shared_mem = NULL;        // pointer to shared memory
static HANDLE shared_mem_handle = NULL; // handle to file mapping
static HANDLE mutex_handle = NULL;      // handle to sync object

LPCWSTR kSharedMemName = TEXT("XR_EXT_overlay_shared_mem");     // Shared Memory known name
LPCWSTR kSharedMutexName = TEXT("XR_EXT_overlay_mutex");        // Shared Memory sync mutex known name

// Semaphore for releasing Host when a Remote RPC has been assembled
LPCWSTR kGuestRequestSemaName = TEXT("LUNARG_XR_IPC_guest_request_sema");
HANDLE gGuestRequestSema;

// Semaphore for releasing Remote when a Host RPC response has been assembled
LPCWSTR kHostResponseSemaName = TEXT("LUNARG_XR_IPC_host_response_sema");
HANDLE gHostResponseSema;

static const DWORD GUEST_REQUEST_WAIT_MILLIS = 100;
static const DWORD HOST_RESPONSE_WAIT_MILLIS = 100000;

// Get the shared memory wrapped in a convenient structure
XR_OVERLAY_EXT_API IPCBuffer IPCGetBuffer()
{
    return IPCBuffer(shared_mem, mem_size);
}

static std::string fmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int size = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if(size >= 0) {
        int provided = size + 1;
        std::unique_ptr<char[]> buf(new char[provided]);

        va_start(args, fmt);
        int size = vsnprintf(buf.get(), provided, fmt, args);
        va_end(args);

        return std::string(buf.get());
    }
    return "(fmt() failed, vsnprintf returned -1)";
}

const void* CopyEventChainIntoBuffer(const XrEventDataBaseHeader* eventData, unsigned char* buffer, size_t remaining);

template <class XR_TYPE>
const void* CopyXrTypeIntoBuffer(const XR_TYPE* eventData, XR_TYPE* buffer, size_t remaining)
{
    if (remaining < sizeof(XR_TYPE)) {
        OutputDebugStringA(fmt("**OVERLAY** out of buffer space in CopyXrTypeIntoBuffer\n").c_str());
        DebugBreak();
    }
    auto* dest = reinterpret_cast<XR_TYPE*>(buffer);
    *dest = *reinterpret_cast<const XR_TYPE*>(eventData);
    unsigned char *next = reinterpret_cast<unsigned char*>(buffer) + sizeof(XR_TYPE);
    dest->next = CopyEventChainIntoBuffer(reinterpret_cast<const XrEventDataBaseHeader*>(eventData->next), next, remaining - sizeof(XR_TYPE));
    return buffer;
}

const void* CopyEventChainIntoBuffer(const XrEventDataBaseHeader* eventData, unsigned char* buffer, size_t remaining)
{
    if(eventData == nullptr) {
        return nullptr;
    }

    switch(eventData->type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            return CopyXrTypeIntoBuffer(reinterpret_cast<const XrEventDataInstanceLossPending*>(eventData), reinterpret_cast<XrEventDataInstanceLossPending*>(buffer), remaining);
            break;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            return CopyXrTypeIntoBuffer(reinterpret_cast<const XrEventDataSessionStateChanged*>(eventData), reinterpret_cast<XrEventDataSessionStateChanged*>(buffer), remaining);
            break;
        }
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            return CopyXrTypeIntoBuffer(reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(eventData), reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(buffer), remaining);
            break;
        }
        case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
            return CopyXrTypeIntoBuffer(reinterpret_cast<const XrEventDataEventsLost*>(eventData), reinterpret_cast<XrEventDataEventsLost*>(buffer), remaining);
            break;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
            return CopyXrTypeIntoBuffer(reinterpret_cast<const XrEventDataInteractionProfileChanged*>(eventData), reinterpret_cast<XrEventDataInteractionProfileChanged*>(buffer), remaining);
            break;
        }
        case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: { 
            return CopyXrTypeIntoBuffer(reinterpret_cast<const XrEventDataPerfSettingsEXT*>(eventData), reinterpret_cast<XrEventDataPerfSettingsEXT*>(buffer), remaining);
            break;
        }
        case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR: {
            return CopyXrTypeIntoBuffer(reinterpret_cast<const XrEventDataVisibilityMaskChangedKHR*>(eventData), reinterpret_cast<XrEventDataVisibilityMaskChangedKHR*>(buffer), remaining);
            break;
        }
        default: {
            OutputDebugStringA(fmt("**OVERLAY** skipped type %d in CopyEventChainIntoBuffer\n", eventData->type).c_str());
            DebugBreak();
            return CopyEventChainIntoBuffer(reinterpret_cast<const XrEventDataBaseHeader*>(eventData->next), buffer, remaining);
            break;
        }
    }
    return buffer;
}

template <class XR_STRUCT>
XrBaseInStructure* AllocateAndCopy(const XR_STRUCT* srcbase, CopyType copyType, std::function<void* (size_t size)> alloc)
{
    auto src = reinterpret_cast<const XR_STRUCT*>(srcbase);
    auto dst = reinterpret_cast<XR_STRUCT*>(alloc(sizeof(XR_STRUCT)));
    if(copyType == COPY_EVERYTHING) {
        *dst = *src;
    } else {
        dst->type = src->type;
    }
    return reinterpret_cast<XrBaseInStructure*>(dst);
}

XR_OVERLAY_EXT_API XrBaseInStructure *CopyXrStructChain(const XrBaseInStructure* srcbase, CopyType copyType, AllocateFunc alloc, std::function<void (void* pointerToPointer)> addOffsetToPointer)
{
    XrBaseInStructure *dstbase = nullptr;
    bool skipped;

    do {
        skipped = false;

        if(!srcbase) {
            return nullptr;
        }

        switch(srcbase->type) {

            // Should copy only non-pointers instead of "*dst = *src"
            case XR_TYPE_INSTANCE_CREATE_INFO: {
                auto src = reinterpret_cast<const XrInstanceCreateInfo*>(srcbase);
                auto dst = reinterpret_cast<XrInstanceCreateInfo*>(alloc(sizeof(XrInstanceCreateInfo)));
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                dst->type = src->type;

                dst->createFlags = src->createFlags;
                dst->applicationInfo = src->applicationInfo;

                // Lay down API layer names...
                auto enabledApiLayerNames = (char **)alloc(sizeof(char *) * src->enabledApiLayerCount);
                dst->enabledApiLayerNames = enabledApiLayerNames;
                dst->enabledApiLayerCount = src->enabledApiLayerCount;
                addOffsetToPointer(&dst->enabledApiLayerNames);
                for(uint32_t i = 0; i < dst->enabledApiLayerCount; i++) {
                    enabledApiLayerNames[i] = (char *)alloc(strlen(src->enabledApiLayerNames[i]) + 1);
                    strncpy_s(enabledApiLayerNames[i], strlen(src->enabledApiLayerNames[i]) + 1, src->enabledApiLayerNames[i], strlen(src->enabledApiLayerNames[i]) + 1);
                    addOffsetToPointer(&enabledApiLayerNames[i]);
                }

                // Lay down extension layer names...
                auto enabledExtensionNames = (char **)alloc(sizeof(char *) * src->enabledExtensionCount);
                dst->enabledExtensionNames = enabledExtensionNames;
                dst->enabledExtensionCount = src->enabledExtensionCount;
                addOffsetToPointer(&dst->enabledExtensionNames);
                for(uint32_t i = 0; i < dst->enabledExtensionCount; i++) {
                    enabledExtensionNames[i] = (char *)alloc(strlen(src->enabledExtensionNames[i]) + 1);
                    strncpy_s(enabledExtensionNames[i], strlen(src->enabledExtensionNames[i]) + 1, src->enabledExtensionNames[i], strlen(src->enabledExtensionNames[i]) + 1);
                    addOffsetToPointer(&enabledExtensionNames[i]);
                }
                break;
            }

            case XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrGraphicsRequirementsD3D11KHR*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_FRAME_STATE: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrFrameState*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SYSTEM_PROPERTIES: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSystemProperties*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_INSTANCE_PROPERTIES: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrInstanceProperties*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_VIEW: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrViewConfigurationView*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_PROPERTIES: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrViewConfigurationProperties*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SESSION_BEGIN_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSessionBeginInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SYSTEM_GET_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSystemGetInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SWAPCHAIN_CREATE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainCreateInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_FRAME_WAIT_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrFrameWaitInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_FRAME_BEGIN_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrFrameBeginInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_COMPOSITION_LAYER_QUAD: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrCompositionLayerQuad*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_EVENT_DATA_BUFFER: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataBuffer*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_FRAME_END_INFO: {
                auto src = reinterpret_cast<const XrFrameEndInfo*>(srcbase);
                auto dst = reinterpret_cast<XrFrameEndInfo*>(alloc(sizeof(XrFrameEndInfo)));
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                dst->type = src->type;
                dst->displayTime = src->displayTime;
                dst->environmentBlendMode = src->environmentBlendMode;
                dst->layerCount = src->layerCount;

                // Lay down layers...
                // const XrCompositionLayerBaseHeader* const* layers;
                auto layers = (XrCompositionLayerBaseHeader**)alloc(sizeof(XrCompositionLayerBaseHeader*) * src->layerCount);
                dst->layers = layers;
                addOffsetToPointer(&dst->layers);
                for(uint32_t i = 0; i < dst->layerCount; i++) {
                    layers[i] = reinterpret_cast<XrCompositionLayerBaseHeader*>(CopyXrStructChain(reinterpret_cast<const XrBaseInStructure*>(src->layers[i]), copyType, alloc, addOffsetToPointer));
                    addOffsetToPointer(&layers[i]);
                }
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainImageAcquireInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainImageWaitInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainImageReleaseInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSessionCreateInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSessionCreateInfoOverlayEXT*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_REFERENCE_SPACE_CREATE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrReferenceSpaceCreateInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
                // We know what this is but do not send it through because we process it locally.
                srcbase = srcbase->next;
                skipped = true;
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataInstanceLossPending*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataSessionStateChanged*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataEventsLost*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataInteractionProfileChanged*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: { 
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataPerfSettingsEXT*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataVisibilityMaskChangedKHR*>(srcbase), copyType, alloc);
                break;
            }

            default: {
                // I don't know what this is, skip it and try the next one
                std::string str = fmt("CopyXrStructChain called on %p of unknown type %d - dropped from \"next\" chain.\n", srcbase, srcbase->type);
                OutputDebugStringA(str.data());
                srcbase = srcbase->next;
                skipped = true;
                break;
            }
        }
    } while(skipped);

    dstbase->next = reinterpret_cast<XrBaseInStructure*>(CopyXrStructChain(srcbase->next, copyType, alloc, addOffsetToPointer));
    addOffsetToPointer(&dstbase->next);

    return dstbase;
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
IPCWaitResult IPCWaitForGuestRequest()
{
    DWORD result;
    do {
        result = WaitForSingleObject(gGuestRequestSema, GUEST_REQUEST_WAIT_MILLIS);
    } while(result == WAIT_TIMEOUT);

    if(result == WAIT_OBJECT_0) {
        return IPC_GUEST_REQUEST_READY;
    }
    return IPC_WAIT_ERROR;
}

// Call from Host to get complete request in shmem
IPCWaitResult IPCWaitForGuestRequestOrTermination(HANDLE remoteProcessHandle)
{
    HANDLE handles[2];

    handles[0] = gGuestRequestSema;
    handles[1] = remoteProcessHandle;

    DWORD result;

    do {
        result = WaitForMultipleObjects(2, handles, FALSE, GUEST_REQUEST_WAIT_MILLIS);
    } while(result == WAIT_TIMEOUT);

    if(result == WAIT_OBJECT_0 + 0) {
        return IPC_GUEST_REQUEST_READY;
    }

    if(result == WAIT_OBJECT_0 + 1) {
        return IPC_REMOTE_PROCESS_TERMINATED;
    }

    return IPC_WAIT_ERROR;
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

XR_OVERLAY_EXT_API void CopyEventChainIntoBuffer(const XrEventDataBaseHeader* eventData, XrEventDataBuffer* buffer)
{
    CopyEventChainIntoBuffer(eventData, reinterpret_cast<unsigned char*>(buffer), sizeof(*buffer));
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
