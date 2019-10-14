#define XR_USE_GRAPHICS_API_D3D11 1

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
XrSession gSavedMainSession;
XrSession gOverlaySession;
bool gExitOverlay = false;
bool gSerializeEverything = true;

const uint64_t ONE_SECOND_IN_NANOSECONDS = 1000000000;

XrFrameState gSavedWaitFrameState;

HANDLE gOverlayCallMutex = NULL;      // handle to sync object
LPCWSTR kOverlayMutexName = TEXT("XR_EXT_overlay_call_mutex");

char CHECK_buf[512];

#define CHECK(a) { a; CheckWinResult(#a, __FILE__, __LINE__); }

void CheckWinResult(const char* what, const char *file, int line)
{
    DWORD lastError = GetLastError();
    if(lastError != 0) {
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, NULL);
        sprintf_s(CHECK_buf, "%s at %s:%d failed with %d (%s)\n", what, file, line, lastError, messageBuf);
        OutputDebugStringA(CHECK_buf);
        DebugBreak();
        LocalFree(messageBuf);
    }
}

#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

void CheckXrResult(XrResult a, const char* what, const char *file, int line)
{
    if(a != XR_SUCCESS) {
        sprintf_s(CHECK_buf, "%s at %s:%d failed with %d\n", what, file, line, a);
        OutputDebugStringA(CHECK_buf);
        DebugBreak();
    }
}

enum {
    // XXX need to do this with an enum generated from ext as part of build
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


// XXX TODO - this should call XR functions normally, probably from
// another compilation unit, and probably after we *know* CreateInstance
// has completed and we have a working dispatch table.
DWORD WINAPI ThreadBody(LPVOID)
{
    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
    sessionCreateInfoOverlay.next = nullptr;
    sessionCreateInfo.next = &sessionCreateInfoOverlay;
    sessionCreateInfoOverlay.overlaySession = XR_TRUE;
    sessionCreateInfoOverlay.sessionLayersPlacement = 1;
    XrResult result = Overlay_xrCreateSession(gSavedInstance, &sessionCreateInfo, &gOverlaySession);
    if(result != XR_SUCCESS) {
        OutputDebugStringA("**OVERLAY** failed to call xrCreateSession in thread\n");
        DebugBreak();
        return 1;
    }
    OutputDebugStringA("**OVERLAY** success in thread creating overlay session\n");

    XrSwapchain swapchains[2];
    XrSwapchainImageD3D11KHR *swapchainImages[2];
    for(int eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = 28; // XXX!!! m_colorSwapchainFormat;
        swapchainCreateInfo.width = 512;
        swapchainCreateInfo.height = 512;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = 1;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.next = NULL;
        result = Overlay_xrCreateSwapchain(gOverlaySession, &swapchainCreateInfo, &swapchains[eye]);
        if(result != XR_SUCCESS) {
            OutputDebugStringA("**OVERLAY** failed to call xrCreateSwapChain in thread\n");
            DebugBreak();
            return 1;
        }
        uint32_t count;
        Overlay_xrEnumerateSwapchainImages(swapchains[eye], 0, &count, nullptr);
        swapchainImages[eye] = new XrSwapchainImageD3D11KHR[count];
        for(uint32_t i = 0; i < count; i++) {
            swapchainImages[eye][i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            swapchainImages[eye][i].next = NULL;
        }
        CHECK_XR(Overlay_xrEnumerateSwapchainImages(swapchains[eye], count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages[eye])));
    }
    OutputDebugStringA("**OVERLAY** success in thread creating swapchain\n");

    int whichImage = 0;
    int whichSwapchain = 0;
    while(!gExitOverlay) {
        // ...
        XrFrameState state;
        Overlay_xrWaitFrame(gOverlaySession, NULL, &state);
        OutputDebugStringA("**OVERLAY** exited overlay session xrWaitFrame\n");
        Overlay_xrBeginFrame(gOverlaySession, NULL);
        for(int eye = 0; eye < 2; eye++) {
            uint32_t index;
            XrSwapchain sc = reinterpret_cast<XrSwapchain>(swapchains[eye]);
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            acquireInfo.next = nullptr;
            // TODO - these should be layered and wrapped with a mutex
            CHECK_XR(downchain->AcquireSwapchainImage(sc, &acquireInfo, &index));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.next = nullptr;
            waitInfo.timeout = ONE_SECOND_IN_NANOSECONDS;
            CHECK_XR(downchain->WaitSwapchainImage(sc, &waitInfo));

            // TODO render into swapchain image here 

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            releaseInfo.next = nullptr;
            CHECK_XR(downchain->ReleaseSwapchainImage(sc, &releaseInfo));
        }
        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.next = NULL;
        Overlay_xrEndFrame(gOverlaySession, &frameEndInfo);
        whichSwapchain = (whichSwapchain + 1) % 2;
    }

    result = Overlay_xrDestroySession(gOverlaySession);
    if(result != XR_SUCCESS) {
        OutputDebugStringA("**OVERLAY** failed to call xrDestroySession in thread\n");
        DebugBreak();
        return 1;
    }

    OutputDebugStringA("**OVERLAY** destroyed session, exiting\n");
    return 0;
}

void CreateOverlaySessionThread()
{
    CHECK(gOverlayCreateSessionSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayCreateSessionSemaName));
    CHECK(gOverlayWaitFrameSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayWaitFrameSemaName));
    CHECK(gMainDestroySessionSema =
        CreateSemaphoreA(nullptr, 0, 1, kMainDestroySessionSemaName));
    CHECK(gOverlayCallMutex = CreateMutex(NULL, TRUE, kOverlayMutexName));
    ReleaseMutex(gOverlayCallMutex);

    CHECK(gOverlayWorkerThread =
        CreateThread(nullptr, 0, ThreadBody, nullptr, 0, &gOverlayWorkerThreadId));
    OutputDebugStringA("**OVERLAY** success\n");
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

XrResult Overlay_xrDestroySession(
    XrSession session)
{
    XrResult result;

    if(session == kOverlayFakeSession) {
        // overlay session

        ReleaseSemaphore(gMainDestroySessionSema, 1, nullptr);
        result = XR_SUCCESS;

    } else {
        // main session

        gExitOverlay = true;
        ReleaseSemaphore(gOverlayWaitFrameSema, 1, nullptr);
        DWORD waitresult = WaitForSingleObject(gMainDestroySessionSema, 1000000);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** main destroy session timeout\n");
            DebugBreak();
        }
        OutputDebugStringA("**OVERLAY** main destroy session okay\n");
        result = downchain->DestroySession(session);
    }

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

        gSavedMainSession = *session;

        // Let overlay session continue
        ReleaseSemaphore(gOverlayCreateSessionSema, 1, nullptr);
		 
    } else {

        // Wait on main session
        DWORD waitresult = WaitForSingleObject(gOverlayCreateSessionSema, 10000);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** overlay create session timeout\n");
            DebugBreak();
        }
        // TODO should store any kind of failure in main XrCreateSession and then fall through here
        *session = kOverlayFakeSession;
        result = XR_SUCCESS;
    }

    return result;
}

XrResult Overlay_xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images)
{ 
    if(gSerializeEverything) {
        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** timeout waiting in EnumerateSwapchainImages on gOverlayCallMutex\n");
            DebugBreak();
        }
    }

    XrResult result = downchain->EnumerateSwapchainImages(swapchain, imageCapacityInput, imageCountOutput, images);

    if(gSerializeEverything) {
        ReleaseMutex(gOverlayCallMutex);
    }

    return result;
}

XrResult Overlay_xrCreateSwapchain(XrSession session, const  XrSwapchainCreateInfo *createInfo, XrSwapchain *swapchain) 
{ 
    if(gSerializeEverything) {
        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** timeout waiting in CreateSwapchain on gOverlayCallMutex\n");
            DebugBreak();
        }
    }

    if(session == kOverlayFakeSession) {
        session = gSavedMainSession;
    }

    XrResult result = downchain->CreateSwapchain(session, createInfo, swapchain);
    if(gSerializeEverything) {
        ReleaseMutex(gOverlayCallMutex);
    }

    return result;
}

XrResult Overlay_xrWaitFrame(XrSession session, const XrFrameWaitInfo *info, XrFrameState *state) 
{ 
    XrResult result;

    if(session == kOverlayFakeSession) {

        // Wait on main session
        // TODO - make first wait be long and subsequent waits be short,
        // since it looks like WaitFrame may wait a long time on runtime.
        DWORD waitresult = WaitForSingleObject(gOverlayWaitFrameSema, 10000);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** overlay session wait frame timeout\n");
            DebugBreak();
        }

        waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** timeout waiting in WaitFrame in main session on gOverlayCallMutex\n");
            DebugBreak();
        }

        // TODO pass back any failure recorded by main session waitframe
        *state = gSavedWaitFrameState;

        ReleaseMutex(gOverlayCallMutex);

        result = XR_SUCCESS;

    } else {

        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** timeout waiting in WaitFrame in main session on gOverlayCallMutex\n");
            DebugBreak();
        }

        result = downchain->WaitFrame(session, info, state);

        ReleaseMutex(gOverlayCallMutex);

        OutputDebugStringA("**OVERLAY** main session wait frame returned\n");

        gSavedWaitFrameState = *state;
        ReleaseSemaphore(gOverlayWaitFrameSema, 1, nullptr);
    }

    return result;
}

XrResult Overlay_xrBeginFrame(XrSession session, const XrFrameBeginInfo *info) 
{ 
    XrResult result;

    if(gSerializeEverything) {
        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** timeout waiting in CreateSwapchain on gOverlayCallMutex\n");
            DebugBreak();
        }
    }

    if(session == kOverlayFakeSession) {

        // Do nothing in overlay session
        result = XR_SUCCESS;

    } else {

        result = downchain->BeginFrame(session, info);

    }

    if(gSerializeEverything) {
        ReleaseMutex(gOverlayCallMutex);
    }
    return result;
}

XrResult Overlay_xrEndFrame(XrSession session, const XrFrameEndInfo *info) 
{ 
    XrResult result;

    if(gSerializeEverything) {
        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** timeout waiting in CreateSwapchain on gOverlayCallMutex\n");
            DebugBreak();
        }
    }

    if(session == kOverlayFakeSession) {

        // TODO copy data from overlay EndFrame to the side
		result = XR_SUCCESS;

    } else {

        // TODO copy info and insert overlay before it
        result = downchain->EndFrame(session, info);

    }

    if(gSerializeEverything) {
        ReleaseMutex(gOverlayCallMutex);
    }
    return result;
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
  } else if (0 == strcmp(name, "xrDestroySession")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrDestroySession);
  } else if (0 == strcmp(name, "xrEnumerateSwapchainImages")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrEnumerateSwapchainImages);
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
