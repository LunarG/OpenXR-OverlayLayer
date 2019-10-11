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

#ifdef __cplusplus    // If used by C++ code, 
extern "C" {          // we need to export the C interface
#endif

// Shared memory entry points
XR_OVERLAY_EXT_API bool MapSharedMemory(UINT32 size);
XR_OVERLAY_EXT_API bool UnmapSharedMemory();
XR_OVERLAY_EXT_API void SetSharedMem(LPCWSTR lpszBuf);
XR_OVERLAY_EXT_API void GetSharedMem(LPWSTR lpszBuf, DWORD cchSize);


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
XrResult Overlay_xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);


// Function used to negotiate an interface betewen the loader and a layer.
XR_OVERLAY_EXT_API XrResult Overlay_xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *li, const char *ln, XrNegotiateApiLayerRequest *lr);

     
#ifdef __cplusplus
}
#endif
