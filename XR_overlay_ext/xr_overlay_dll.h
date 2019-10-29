#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <openxr/openxr.h>
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

typedef struct XrSessionCreateInfoOverlayEXT
{
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrBool32                    overlaySession;
    uint32_t                    sessionLayersPlacement;
} XrSessionCreateInfoOverlayEXT;

struct IPCXrCreateSessionIn {
    XrInstance instance;
    const XrSessionCreateInfo *createInfo;
};

template <typename T>
T unpack(unsigned char*& ptr)
{
    T tmp = *reinterpret_cast<T*>(ptr);
    ptr += sizeof(T);
	return tmp;
}

template <typename T>
bool pack(unsigned char*& ptr, const T& v)
{
    // XXX TODO - don't overflow the buffer - also pass and check size
    *reinterpret_cast<T*>(ptr) = v;
    ptr += sizeof(T);
    return true;
}

const uint64_t IPC_REQUEST_HANDOFF = 1;
const uint64_t IPC_XR_CREATE_SESSION = 2;

#ifdef __cplusplus    // If used by C++ code, 
extern "C" {          // we need to export the C interface
#endif

// Shared memory entry points
XR_OVERLAY_EXT_API bool MapSharedMemory(UINT32 size);
XR_OVERLAY_EXT_API bool UnmapSharedMemory();
XR_OVERLAY_EXT_API void SetSharedMem(LPCWSTR lpszBuf);
XR_OVERLAY_EXT_API void GetSharedMem(LPWSTR lpszBuf, DWORD cchSize);
XR_OVERLAY_EXT_API void* IPCGetSharedMemory();
XR_OVERLAY_EXT_API bool IPCWaitForGuestRequest();
XR_OVERLAY_EXT_API void IPCFinishGuestRequest();
XR_OVERLAY_EXT_API bool IPCWaitForHostResponse();
XR_OVERLAY_EXT_API void IPCFinishHostResponse();

enum {
    // XXX need to do this with an enum generated from ext as part of build
    XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT = 1000099999,
};


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


// Function used to negotiate an interface betewen the loader and a layer.
XR_OVERLAY_EXT_API XrResult Overlay_xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *li, const char *ln, XrNegotiateApiLayerRequest *lr);

     
#ifdef __cplusplus
}
#endif
