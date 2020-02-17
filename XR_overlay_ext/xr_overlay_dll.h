//
// Copyright 2019-2020 LunarG Inc. and PlutoVR Inc.
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

#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <new>
#include <cstdlib>
#include <functional>

#include <d3d11_4.h>
#include <d3d12.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <xr_dependencies.h>
#include <loader_interfaces.h>


//#if defined(__GNUC__) && __GNUC__ >= 4
//#define XR_OVERLAY_EXT_API __attribute__((visibility("default")))
//#elif defined(__clang__)
//#define XR_OVERLAY_EXT_API __attribute__((visibility("default")))
//#else // Windows
//#define blah
//#endif

// Just VisStudio, for now
#ifdef XR_OVERLAY_DLL_EXPORTS
#define XR_OVERLAY_EXT_API __declspec(dllexport)
#else
#define XR_OVERLAY_EXT_API __declspec(dllimport)
#endif

enum {
    KEYED_MUTEX_IPC_REMOTE = 0,
    KEYED_MUTEX_IPC_HOST = 1,
};

// Stand in while Overlay is not defined in openxr.h

XR_DEFINE_ATOM(XrPermissionIdEXT)

typedef struct XrPermissionPropertiesEXT {
    XrStructureType type;
    const void* XR_MAY_ALIAS next;
    char permissionName[128];
    char permissionDescription[256];
    XrPermissionIdEXT permissionId;
} XrPermissionPropertiesEXT;

typedef struct XrPermissionRequestEXT {
    XrStructureType type;
    const void* XR_MAY_ALIAS next;
    XrPermissionIdEXT permissionId;
    XrBool32 optional;
} XrPermissionRequestEXT;

typedef struct XrSessionCreateInfoPermissionsEXT {
    XrStructureType type;
    const void* XR_MAY_ALIAS next;
    uint32_t requestedPermissionsCount;
    const XrPermissionRequestEXT* requestedPermissions;
} XrSessionCreateInfoPermissionsEXT;

XrResult xrEnumerateInstancePermissionsEXT(
    XrInstance instance,
    uint32_t propertyCapacityInput,
    uint32_t* propertyCountOutput,
    XrPermissionPropertiesEXT* properties);

typedef XrResult (XRAPI_PTR *PFN_xrEnumerateInstancePermissionsEXT)(XrInstance instance, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrPermissionPropertiesEXT* properties);

enum {
    // XXX need to do this with an enum generated from ext as part of build

    // XR_EXT_permission_support
    XR_TYPE_PERMISSION_PROPERTIES_EXT,
    XR_TYPE_PERMISSION_REQUEST_EXT,
    XR_TYPE_SESSION_CREATE_INFO_PERMISSIONS_EXT,

    // New Success Codes
    XR_OPTIONAL_PERMISSION_UNAVAILABLE_EXT,

    // New Error codes
    XR_ERROR_PERMISSION_INVALID_EXT,
    XR_ERROR_REQUIRED_PERMISSION_UNAVAILABLE_EXT,
};

// Structs containing RPC arguments -----------------------------------------

struct IPCXrCreateInstance {
    const XrInstanceCreateInfo*                 createInfo;
    XrInstance *instance;
    DWORD remoteProcessId; 
    DWORD *hostProcessId;
};

struct IPCXrGetSystem {
	XrInstance                                  instance;
	const XrSystemGetInfo*                      getInfo;
    XrSystemId*                                 systemId;
};

struct IPCXrCreateSession {
    XrInstance instance;
    const XrSessionCreateInfo *createInfo;
    XrSession* session;
};

struct IPCXrCreateReferenceSpace {
    XrSession session;
    const XrReferenceSpaceCreateInfo *createInfo;
    XrSpace* space;
};

struct IPCXrEnumerateSwapchainFormats {
    XrSession                                   session;
    uint32_t                                    formatCapacityInput;
    uint32_t*                                   formatCountOutput;
    int64_t*                                    formats;
};

struct IPCXrCreateSwapchain {
    XrSession                                   session;
    const XrSwapchainCreateInfo*                createInfo;
    XrSwapchain*                                swapchain;
    int32_t*                                    swapchainCount;    
};

struct IPCXrWaitFrame {
    XrSession                                   session;
    const XrFrameWaitInfo*                      frameWaitInfo;
    XrFrameState*                               frameState;
};

struct IPCXrBeginFrame {
    XrSession                                   session;
    const XrFrameBeginInfo*                     frameBeginInfo;
};

struct IPCXrEndFrame {
    XrSession                                   session;
    const XrFrameEndInfo*                       frameEndInfo;
};

struct IPCXrAcquireSwapchainImage {
    XrSwapchain                                 swapchain;
    const XrSwapchainImageAcquireInfo*          acquireInfo;
    uint32_t*                                   index;
};

struct IPCXrWaitSwapchainImage {
    XrSwapchain                                 swapchain;
    const XrSwapchainImageWaitInfo*             waitInfo;
    HANDLE                                      sourceImage;
};

struct IPCXrReleaseSwapchainImage {
    XrSwapchain                                 swapchain;
    const XrSwapchainImageReleaseInfo*          releaseInfo;
    HANDLE                                      sourceImage;
};

struct IPCXrDestroySession {
    XrSession                                   session;
};

struct IPCXrEnumerateInstanceExtensionProperties {
    const char*                                 layerName;
    uint32_t                                    propertyCapacityInput;
    uint32_t*                                   propertyCountOutput;
    XrExtensionProperties*                      properties;
};

struct IPCXrEnumerateViewConfigurations {
    XrInstance                                  instance;
    XrSystemId                                  systemId;
    uint32_t                                    viewConfigurationTypeCapacityInput;
    uint32_t*                                   viewConfigurationTypeCountOutput;
    XrViewConfigurationType*                    viewConfigurationTypes;
};

struct IPCXrEnumerateViewConfigurationViews {
    XrInstance                                  instance;
    XrSystemId                                  systemId;
    XrViewConfigurationType                     viewConfigurationType;
    uint32_t                                    viewCapacityInput;
    uint32_t*                                   viewCountOutput;
    XrViewConfigurationView*                    views;
};

struct IPCXrGetViewConfigurationProperties {
    XrInstance                                  instance;
    XrSystemId                                  systemId;
    XrViewConfigurationType                     viewConfigurationType;
    XrViewConfigurationProperties*              configurationProperties;
};

struct IPCXrDestroySwapchain {
    XrSwapchain                                 swapchain;
};

struct IPCXrDestroySpace {
    XrSpace                                 space;
};

struct IPCXrGetInstanceProperties {
    XrInstance                                  instance;
    XrInstanceProperties*                       properties;
};

struct IPCXrLocateSpace {
    XrSpace space;
  XrSpace baseSpace;
  XrTime time;
  XrSpaceLocation* spaceLocation;
};

struct IPCXrGetSystemProperties {
    XrInstance                                  instance;
    XrSystemId                                  system;
    XrSystemProperties*                       properties;
};

struct IPCXrGetD3D11GraphicsRequirementsKHR {
    XrInstance	instance;
    XrSystemId	systemId;
    XrGraphicsRequirementsD3D11KHR*             graphicsRequirements;
};

struct IPCXrBeginSession {
    XrSession                                   session;
    const XrSessionBeginInfo*                   beginInfo;
};

struct IPCXrRequestExitSession {
    XrSession                                   session;
};

struct IPCXrEndSession {
    XrSession                                   session;
};

struct IPCXrPollEvent {
    XrInstance                                  instance;
    XrEventDataBuffer*                          event;
};

// no RPC struct for EnumerateSwapchainImages

#define MAX_POINTER_FIXUP_COUNT 128

// Header laid into the shared memory tracking the RPC type, the result,
// and all pointers inside the shared memory which have to be fixed up
// passing from Remote to Host and then back
struct IPCXrHeader
{
    uint64_t requestType;
    XrResult result;

    int pointerFixupCount;
    size_t pointerOffsets[MAX_POINTER_FIXUP_COUNT];

    IPCXrHeader(uint64_t requestType) :
        requestType(requestType),
        pointerFixupCount(0)
    {}

    bool addOffsetToPointer(void* vbase, void* vp)
    {
        if(pointerFixupCount >= MAX_POINTER_FIXUP_COUNT)
            return false;

        unsigned char* base = reinterpret_cast<unsigned char *>(vbase);
        unsigned char* p = reinterpret_cast<unsigned char *>(vp);
        pointerOffsets[pointerFixupCount++] = p - base;
        return true;
    }

    void makePointersRelative(void* vbase)
    {
        unsigned char* base = reinterpret_cast<unsigned char *>(vbase);
        for(int i = 0; i < pointerFixupCount; i++) {
            unsigned char* pointerToByte = base + pointerOffsets[i];
            unsigned char** pointerToPointer = reinterpret_cast<unsigned char **>(pointerToByte);
            unsigned char*& pointer = *pointerToPointer;
            if(pointer) { // nullptr remains nulltpr
                pointer = pointer - (base - reinterpret_cast<unsigned char *>(0));
            }
        }
    }

    void makePointersAbsolute(void* vbase)
    {
        unsigned char* base = reinterpret_cast<unsigned char *>(vbase);
        for(int i = 0; i < pointerFixupCount; i++) {
            unsigned char* pointerToByte = base + pointerOffsets[i];
            unsigned char** pointerToPointer = reinterpret_cast<unsigned char **>(pointerToByte);
            unsigned char*& pointer = *pointerToPointer;
            if(pointer) { // nullptr remains nulltpr
                pointer = pointer + (base - reinterpret_cast<unsigned char *>(0));
            }
        }
    }
};

static const int memberAlignment = 8;

static size_t pad(size_t s)
{
    return (s + memberAlignment - 1) / memberAlignment * memberAlignment;
}

// Convenience object representing the shared memory buffer after the
// header, allowing apps to allocate bytes and then fill them or to read
// bytes and step over them
struct IPCBuffer
{
    unsigned char *base;
    size_t size;
    unsigned char *current;

    static const int memberAlignment = 8;

    IPCBuffer(void *base_, size_t size_) :
        base(reinterpret_cast<unsigned char*>(base_)),
        size(size_)
    {
        reset();
    }

    void reset(void)
    {
        current = base;
    }

    void advance(size_t s)
    {
        current += pad(s);
    }

    bool write(const void* p, size_t s)
    {
        if((current - base + s) > size)
            return false;
        memcpy(current, p, s);
        advance(s);
        return true;
    }

    void read(void *p, size_t s)
    {
        if((current - base + s) > size)
            abort();
        memcpy(p, current, s);
        advance(s);
    }

    template <typename T>
    bool write(const T* p)
    {
        if((current - base + s) > size)
            return false;
        memcpy(current, p, s);
        advance(sizeof(T));
        return true;
    }
    
    template <typename T>
    bool read(T* p)
    {
        if((current - base + s) > size)
            return false;
        memcpy(p, current, s);
        advance(sizeof(T));
        return true;
    }

    template <typename T>
    T* getAndAdvance()
    {
        if(current - base + sizeof(T) > size)
            return nullptr;
        T *p = reinterpret_cast<T*>(current);
        advance(sizeof(T));
        return p;
    }

    void *allocate (std::size_t s)
    {
        if((current - base + s) > size)
            return nullptr;
        void *p = current;
        advance(s);
        return p;
    }
    void deallocate (void *) {}
};

// New and delete for the buffer above
inline void* operator new (std::size_t size, IPCBuffer& buffer)
{
    return buffer.allocate(size);
}

inline void operator delete(void* p, IPCBuffer& buffer)
{
    buffer.deallocate(p);
}

enum CopyType {
    COPY_EVERYTHING,       // XR command will consume (aka input)
    COPY_ONLY_TYPE_NEXT,   // XR command will fill (aka output)
};

typedef std::function<void* (size_t size)> AllocateFunc;
typedef std::function<void (const void* p)> FreeFunc;
XR_OVERLAY_EXT_API XrBaseInStructure *CopyXrStructChain(const XrBaseInStructure* srcbase, CopyType copyType, AllocateFunc alloc, std::function<void (void* pointerToPointer)> addOffsetToPointer);
XR_OVERLAY_EXT_API void FreeXrStructChain(const XrBaseInStructure* p, FreeFunc free);
XR_OVERLAY_EXT_API XrBaseInStructure* CopyEventChainIntoBuffer(const XrEventDataBaseHeader* eventData, XrEventDataBuffer* buffer);
XR_OVERLAY_EXT_API XrBaseInStructure* CopyXrStructChainWithMalloc(const void* xrstruct);
XR_OVERLAY_EXT_API void FreeXrStructChainWithFree(const void* xrstruct);

// RPC types implemented
enum {
    IPC_XR_CREATE_INSTANCE = 1,
    IPC_XR_CREATE_SESSION,
    IPC_XR_CREATE_REFERENCE_SPACE,
    IPC_XR_ENUMERATE_SWAPCHAIN_FORMATS,
    IPC_XR_CREATE_SWAPCHAIN,
    IPC_XR_BEGIN_FRAME,
    IPC_XR_WAIT_FRAME,
    IPC_XR_END_FRAME,
    IPC_XR_ACQUIRE_SWAPCHAIN_IMAGE,
    IPC_XR_WAIT_SWAPCHAIN_IMAGE,
    IPC_XR_RELEASE_SWAPCHAIN_IMAGE,
    IPC_XR_DESTROY_SESSION,
    IPC_XR_ENUMERATE_VIEW_CONFIGURATIONS,
    IPC_XR_ENUMERATE_VIEW_CONFIGURATION_VIEWS,
    IPC_XR_GET_VIEW_CONFIGURATION_PROPERTIES,
    IPC_XR_DESTROY_SWAPCHAIN,
    IPC_XR_DESTROY_SPACE,
    IPC_XR_BEGIN_SESSION,
    IPC_XR_REQUEST_EXIT_SESSION,
    IPC_XR_END_SESSION,
    IPC_XR_GET_INSTANCE_PROPERTIES,
    IPC_XR_GET_SYSTEM_PROPERTIES,
    IPC_XR_LOCATE_SPACE,
    IPC_XR_GET_D3D11_GRAPHICS_REQUIREMENTS_KHR,
    IPC_XR_POLL_EVENT,
    IPC_XR_GET_SYSTEM,
    IPC_XR_ENUMERATE_INSTANCE_EXTENSION_PROPERTIES,
};

enum IPCWaitResult {
    IPC_REMOTE_REQUEST_READY,
    IPC_HOST_RESPONSE_READY,
    IPC_REMOTE_PROCESS_TERMINATED,
    IPC_WAIT_ERROR,
};

enum IPCConnectResult {
    IPC_CONNECT_SUCCESS,
    IPC_CONNECT_TIMEOUT,
    IPC_CONNECT_ERROR,
};

XR_OVERLAY_EXT_API IPCBuffer IPCGetBuffer();

#ifdef __cplusplus    // If used by C++ code, 
extern "C" {          // we need to export the C interface
#endif

// Shared memory entry points
XR_OVERLAY_EXT_API bool MapSharedMemory(UINT32 size);
XR_OVERLAY_EXT_API bool UnmapSharedMemory();

XR_OVERLAY_EXT_API void* IPCGetSharedMemory();
XR_OVERLAY_EXT_API IPCConnectResult IPCXrConnectToHost();
XR_OVERLAY_EXT_API IPCConnectResult IPCWaitForRemoteConnection();
XR_OVERLAY_EXT_API IPCWaitResult IPCWaitForRemoteRequestOrTermination();
XR_OVERLAY_EXT_API void IPCFinishRemoteRequest();
XR_OVERLAY_EXT_API IPCWaitResult IPCWaitForHostResponse();
XR_OVERLAY_EXT_API void IPCFinishHostResponse();
XR_OVERLAY_EXT_API void IPCSetupForRemoteConnection();

// Exported entry points
XR_OVERLAY_EXT_API XrResult Overlay_xrCreateInstance(const XrInstanceCreateInfo*, XrInstance*);
XR_OVERLAY_EXT_API XrResult Overlay_xrDestroyInstance(XrInstance);
XR_OVERLAY_EXT_API XrResult Overlay_xrGetInstanceProcAddr(XrInstance, const char*, PFN_xrVoidFunction*);
XR_OVERLAY_EXT_API XrResult Overlay_xrCreateSwapchain(XrSession, const  XrSwapchainCreateInfo*, XrSwapchain*);
XR_OVERLAY_EXT_API XrResult Overlay_xrBeginFrame(XrSession, const XrFrameBeginInfo*);
XR_OVERLAY_EXT_API XrResult Overlay_xrEndFrame(XrSession, const XrFrameEndInfo*);
XR_OVERLAY_EXT_API XrResult Overlay_xrWaitFrame(XrSession, const XrFrameWaitInfo*, XrFrameState*);
XR_OVERLAY_EXT_API XrResult Overlay_xrCreateApiLayerInstance(const XrInstanceCreateInfo *info, const struct XrApiLayerCreateInfo *apiLayerInfo, XrInstance *instance);
XR_OVERLAY_EXT_API XrResult Overlay_xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);
XR_OVERLAY_EXT_API XrResult Overlay_xrDestroySession(XrSession session);
XR_OVERLAY_EXT_API XrResult Overlay_xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);
XR_OVERLAY_EXT_API XrResult Overlay_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);
XR_OVERLAY_EXT_API XrResult Overlay_xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats);
XR_OVERLAY_EXT_API XrResult Overlay_xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo);
XR_OVERLAY_EXT_API XrResult Overlay_xrRequestExitSession(XrSession session);
XR_OVERLAY_EXT_API XrResult Overlay_xrEndSession(XrSession session);
XR_OVERLAY_EXT_API XrResult Overlay_xrDestroySpace(XrSpace space);
XR_OVERLAY_EXT_API XrResult Overlay_xrDestroySwapchain(XrSwapchain swapchain);
XR_OVERLAY_EXT_API XrResult Overlay_xrGetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* graphicsRequirements);
XR_OVERLAY_EXT_API XrResult Overlay_xrPollEvent(XrInstance instance, XrEventDataBuffer* event);
XR_OVERLAY_EXT_API XrResult Overlay_xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId);
XR_OVERLAY_EXT_API XrResult Overlay_xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* spaceLocation);
XR_OVERLAY_EXT_API XrResult Overlay_xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index);
XR_OVERLAY_EXT_API XrResult Overlay_xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);
XR_OVERLAY_EXT_API XrResult Overlay_xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);



// Function used to negotiate an interface betewen the loader and a layer.
XR_OVERLAY_EXT_API XrResult Overlay_xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *li, const char *ln, XrNegotiateApiLayerRequest *lr);

     
#ifdef __cplusplus
}
#endif
