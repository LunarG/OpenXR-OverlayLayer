#include "xr_overlay_dll.h"
#include "xr_generated_dispatch_table.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

const char *kOverlayLayerName = "XR_EXT_overlay_api_layer";
DWORD gOverlayWorkerThreadId;
HANDLE gOverlayWorkerThread;
LPCSTR kOverlayCreateSessionSemaName = "XR_EXT_overlay_overlay_create_session_sema";
HANDLE gOverlayCreateSessionSema;
LPCSTR kOverlayWaitFrameSemaName = "XR_EXT_overlay_overlay_wait_frame_sema";
HANDLE gOverlayWaitFrameSema;
LPCSTR kMainDestroySessionSemaName = "XR_EXT_overlay_main_destroy_session_sema";
HANDLE gMainDestroySessionSema;

XrInstance gSavedInstance;
XrSession kOverlayFakeSession = (XrSession)0xCAFEFEED;
XrSession gMainSessionSaved;
XrSession gOverlaySession;

enum {
    XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT = 1000099999,
};

struct xrinfoBase
{
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
};

typedef struct XrSessionCreateInfoOverlayEXT
{
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrBool32                    overlaySession;
    uint32_t                    sessionLayersPlacement;
} XrSessionCreateInfoOverlayEXT;

static XrGeneratedDispatchTable *downchain = nullptr;

#ifdef __cplusplus    // If used by C++ code, 
extern "C" {          // we need to export the C interface
#endif


// Negotiate an interface with the loader 
XR_OVERLAY_EXT_API XrResult Overlay_xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *loaderInfo, 
                                                    const char *layerName,
                                                    XrNegotiateApiLayerRequest *layerRequest) 
{
    if (nullptr != layerName)
    {
        if (0 != strncmp(kOverlayLayerName, layerName, strnlen_s(kOverlayLayerName, XR_MAX_API_LAYER_NAME_SIZE)))
        {
            return XR_ERROR_INITIALIZATION_FAILED;
        }
    }

    if (nullptr == loaderInfo || 
        nullptr == layerRequest || 
        loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || 
        loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        layerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        layerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        layerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->minApiVersion < XR_MAKE_VERSION(0, 9, 0) || 
        loaderInfo->minApiVersion >= XR_MAKE_VERSION(1, 1, 0))
    {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    layerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    layerRequest->layerApiVersion = XR_MAKE_VERSION(1, 0, 0);
    layerRequest->getInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(Overlay_xrGetInstanceProcAddr);
    layerRequest->createApiLayerInstance = reinterpret_cast<PFN_xrCreateApiLayerInstance>(Overlay_xrCreateApiLayerInstance);

    return XR_SUCCESS;
}

XrResult Overlay_xrCreateInstance(const XrInstanceCreateInfo *info, XrInstance *instance) 
{ 
    // Layer initialization here
    return XR_SUCCESS; 
}

XrResult Overlay_xrDestroyInstance(XrInstance instance) 
{ 
    // Layer cleanup here
    return XR_SUCCESS; 
}

DWORD WINAPI ThreadBody(LPVOID)
{
    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    XrSessionCreateInfoOverlayEXT createInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
    createInfo.next = &createInfoOverlay;
    createInfoOverlay.overlaySession = XR_TRUE;
    createInfoOverlay.sessionLayersPlacement = 1;
    XrResult result = Overlay_xrCreateSession(gSavedInstance, &createInfo, &gOverlaySession);
    if(result != XR_SUCCESS) {
        OutputDebugStringA("**BRAD** failed to call xrCreateSession in thread\n");
        DebugBreak();
        return 1;
    }
    OutputDebugStringA("**BRAD** success in thread creating overlay session\n");

    return 0;
}

char CHK_buf[512];
#define CHK(a) \
    if((a) == NULL) { \
        sprintf_s(CHK_buf, "operation at %s:%d failed with %d\n", __FILE__, __LINE__, GetLastError()); \
        OutputDebugStringA(CHK_buf); \
        DebugBreak(); \
    }

void CreateOverlaySessionThread()
{
    CHK(gOverlayCreateSessionSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayCreateSessionSemaName));
    CHK(gOverlayWaitFrameSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayWaitFrameSemaName));
    CHK(gMainDestroySessionSema =
        CreateSemaphoreA(nullptr, 0, 1, kMainDestroySessionSemaName));

    CHK(gOverlayWorkerThread =
        CreateThread(nullptr, 0, ThreadBody, nullptr, 0, &gOverlayWorkerThreadId));
    OutputDebugStringA("**BRAD** success\n");
}

XrResult Overlay_xrCreateApiLayerInstance(const XrInstanceCreateInfo *info, const struct XrApiLayerCreateInfo *apiLayerInfo, XrInstance *instance) 
{
    assert(0 == strncmp(kOverlayLayerName, apiLayerInfo->nextInfo->layerName, strnlen_s(kOverlayLayerName, XR_MAX_API_LAYER_NAME_SIZE)));
    assert(nullptr != apiLayerInfo->nextInfo);

    // Copy the contents of the layer info struct, but then move the next info up by
    // one slot so that the next layer gets information.
    XrApiLayerCreateInfo local_api_layer_info = {};
    memcpy(&local_api_layer_info, apiLayerInfo, sizeof(XrApiLayerCreateInfo));
    local_api_layer_info.nextInfo = apiLayerInfo->nextInfo->next;

    // Get the function pointers we need
    PFN_xrGetInstanceProcAddr       pfn_next_gipa = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;
    PFN_xrCreateApiLayerInstance    pfn_next_cali = apiLayerInfo->nextInfo->nextCreateApiLayerInstance;

    // Create the instance
    XrInstance returned_instance = *instance;
    XrResult result = pfn_next_cali(info, &local_api_layer_info, &returned_instance);
    *instance = returned_instance;
    gSavedInstance = returned_instance;

    // Create the dispatch table to the next levels
    downchain = new XrGeneratedDispatchTable();
    GeneratedXrPopulateDispatchTable(downchain, returned_instance, pfn_next_gipa);

    // TBD where should the layer's dispatch table live? File global for now...

    //std::unique_lock<std::mutex> mlock(g_instance_dispatch_mutex);
    //g_instance_dispatch_map[returned_instance] = next_dispatch;

    CreateOverlaySessionThread();

    return result;
}

/*

Final
    add "external synchronization" around all funcs needing external synch
    add special conditional "always synchronize" in case runtime misbehaves
    catch and pass all functions
    run with validation layer?
*/

// TODO catch BeginSession

// TODO catch EndSession

// TODO catch CreateSession
// if not cached session
//     create primary session and hold
// if not overlay
//     kick off thread to create overlay session
//     hand back existing CreateSession
// if overlay
//     hand back placeholder for session for overlays

// TODO for everything that receives a session
// if it's the overlay session
//   replace with the main session
//   behave as overlay

// TODO catch DestroySession
// if not overlay
//   if overlay exists, mark session destroyed and wait for thread to join
//   else destroy session
// if overlay
//   mark overlay session destroyed
// actually destroy on last of overlay or 

// TODO catch PollEvent
// if not overlay
//     normal
// else if overlay
//     transmit STOPPING

// TODO WaitFrame
// if main
//     mark have waitframed since beginsession
//     chain waitframe and save off results
// if overlay
//     wait on main waitframe if not waitframed since beginsession
//     use results saved from main

// TODO BeginFrame
// chain BeginFrame
// mark that overlay or main session called BeginFrame
// if main
//     clear have waitframed since last beginframe
// overlay
//     mark that we have begun a frame

// TODO EndFrame
// if main
//     add in overlay frame data if exists
// if overlay
//     store EndFrame data


XrResult Overlay_xrGetSystemProperties(
    XrInstance instance,
    XrSystemId systemId,
    XrSystemProperties* properties)
{
    XrResult result;

    result = downchain->GetSystemProperties(instance, systemId, properties);
    if(result != XR_SUCCESS)
	return result;

    // Reserve one for overlay
    properties->graphicsProperties.maxLayerCount =
        properties->graphicsProperties.maxLayerCount - 1;

    return result;
}

XrResult Overlay_xrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session)
{
    XrResult result;

    xrinfoBase* p = reinterpret_cast<xrinfoBase*>(const_cast<void*>(createInfo->next));
    while(p != nullptr && p->type != XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT) {
        p = reinterpret_cast<xrinfoBase*>(const_cast<void*>(p->next));
    }
    XrSessionCreateInfoOverlayEXT* cio = reinterpret_cast<XrSessionCreateInfoOverlayEXT*>(p);

    // TODO handle the case where Main session passes the
    // overlaycreateinfo but overlaySession = FALSE
    if(cio == nullptr) {
        // Main session

        // TODO : remake chain without InfoOverlayEXT

        result = downchain->CreateSession(instance, createInfo, session);
        if(result != XR_SUCCESS)
            return result;

        gMainSessionSaved = *session;

        // Let overlay session continue
        ReleaseSemaphore(gOverlayCreateSessionSema, 1, nullptr);

    } else {
        // Wait on main session
        DWORD waitresult = WaitForSingleObject(gOverlayCreateSessionSema, 1000);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**BRAD** overlay create session timeout\n");
            DebugBreak();
        }
        // TODO should store any kind of failure in main XrCreateSession and then fall through here
        *session = kOverlayFakeSession;
        result = XR_SUCCESS;
    }

    return result;
}

XrResult Overlay_xrCreateSwapchain(XrSession session, const  XrSwapchainCreateInfo *createInfo, XrSwapchain *swapchain) 
{ 
    // return XR_ERROR_INITIALIZATION_FAILED;   // TBD
    return downchain->CreateSwapchain(session, createInfo, swapchain);
}

XrResult Overlay_xrBeginFrame(XrSession session, const XrFrameBeginInfo *info) 
{ 
    // return XR_ERROR_INITIALIZATION_FAILED;   // TBD
    return downchain->BeginFrame(session, info);
}

XrResult Overlay_xrEndFrame(XrSession session, const XrFrameEndInfo *info) 
{ 
    // return XR_ERROR_INITIALIZATION_FAILED;   // TBD
    return downchain->EndFrame(session, info);
}

XrResult Overlay_xrWaitFrame(XrSession session, const XrFrameWaitInfo *info, XrFrameState *state) 
{ 
    // return XR_ERROR_INITIALIZATION_FAILED;   // TBD
    return downchain->WaitFrame(session, info, state);
}

XrResult Overlay_xrGetInstanceProcAddr(XrInstance instance, const char *name, PFN_xrVoidFunction *function) {
  if (0 == strcmp(name, "xrGetInstanceProcAddr")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrGetInstanceProcAddr);
  } else if (0 == strcmp(name, "xrCreateInstance")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrCreateInstance);
  } else if (0 == strcmp(name, "xrDestroyInstance")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrDestroyInstance);
  } else if (0 == strcmp(name, "xrCreateSwapchain")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrCreateSwapchain);
  } else if (0 == strcmp(name, "xrBeginFrame")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrBeginFrame);
  } else if (0 == strcmp(name, "xrEndFrame")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrEndFrame);
  } else if (0 == strcmp(name, "xrGetSystemProperties")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrGetSystemProperties);
  } else if (0 == strcmp(name, "xrWaitFrame")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrWaitFrame);
  } else if (0 == strcmp(name, "xrCreateSession")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrCreateSession);
  } else {
    *function = nullptr;
  }

      // If we setup the function, just return
    if (*function != nullptr) {
        return XR_SUCCESS;
    }

    if(downchain == nullptr) {
        return XR_ERROR_HANDLE_INVALID;
    }

    return downchain->GetInstanceProcAddr(instance, name, function);
}

#ifdef __cplusplus
}   // extern "C"
#endif
