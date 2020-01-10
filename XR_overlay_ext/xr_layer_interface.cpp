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

#include <memory>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <queue>

#include <cassert>
#include <cstring>
#include <cstdio>

#include <d3d11_4.h>
#include <d3d12.h>

#include "xr_overlay_dll.h"
#include "xr_generated_dispatch_table.h"
#include "xr_linear.h"

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

static void CheckResultWithLastError(bool success, const char* what, const char *file, int line)
{
    if(!success) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        std::string str = fmt("%s at %s:%d failed with %d (%s)\n", what, file, line, lastError, messageBuf);
        OutputDebugStringA(str.data());
        DebugBreak();
        LocalFree(messageBuf);
    }
}

static void CheckResult(HRESULT result, const char* what, const char *file, int line)
{
    if(result != S_OK) {
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        std::string str = fmt("%s at %s:%d failed with %d (%s)\n", what, file, line, result, messageBuf);
        OutputDebugStringA(str.data());
        DebugBreak();
        LocalFree(messageBuf);
    }
}

static void CheckXrResult(XrResult a, const char* what, const char *file, int line)
{
    if(a != XR_SUCCESS) {
        std::string str = fmt("%s at %s:%d failed with %d\n", what, file, line, a);
        OutputDebugStringA(str.data());
        DebugBreak();
    }
}


// Use this macro to test if HANDLE or pointer functions succeeded that also update LastError
#define CHECK_NOT_NULL(a) CheckResultWithLastError(((a) != NULL), #a, __FILE__, __LINE__)

// Use this macro to test if functions succeeded that also update LastError
#define CHECK_LAST_ERROR(a) CheckResultWithLastError((a), #a, __FILE__, __LINE__)

// Use this macro to test Direct3D functions
#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

// Use this macro to test OpenXR functions
#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

// Supports only a single overlay / RPC session at this time

const char *kOverlayLayerName = "XR_EXT_overlay_api_layer";

DWORD gOverlayWorkerThreadId;
HANDLE gOverlayWorkerThread;

// Semaphore for blocking Overlay CreateSession until Main CreateSession has occurred
LPCSTR kOverlayCreateSessionSemaName = "XR_EXT_overlay_overlay_create_session_sema";
bool gMainSessionCreated = false;
HANDLE gOverlayCreateSessionSema;

// Semaphore for blocking Overlay WaitFrame while Main WaitFrame is occuring
LPCSTR kOverlayWaitFrameSemaName = "XR_EXT_overlay_overlay_wait_frame_sema";
HANDLE gOverlayWaitFrameSema;

// Main Session context that we hold on to for processing and interleaving
// Overlay Session commands
XrSession gSavedMainSession;
ID3D11Device *gSavedD3DDevice;
XrGraphicsRequirementsD3D11KHR gGraphicsRequirementsD3D11Saved; // XXX this is a struct and as such needs to be deep copied
bool gGetGraphicsRequirementsD3D11KHRWasCalled = false;
XrInstance gSavedInstance;
std::set<std::string> gSavedRequestedExtensions;
std::set<std::string> gSavedRequestedApiLayers;
XrFormFactor gSavedFormFactor;
XrSystemId gSavedSystemId;
unsigned int overlaySessionStandin;
XrSession kOverlayFakeSession = reinterpret_cast<XrSession>(&overlaySessionStandin);

bool gExitIPCLoop = false;
bool gSerializeEverything = true;

enum { MAX_OVERLAY_LAYER_COUNT = 2 };

const int OVERLAY_WAITFRAME_NORMAL_MILLIS = 32000;

// Compositor layers from Overlay Session to overlay on Main Session's layers
uint32_t gOverlayLayerCount = 0;
std::vector<const XrCompositionLayerBaseHeader *>gOverlayLayers; // XXX this is a struct and as such needs to be deep copied
std::set<XrSwapchain> gSwapchainsDestroyPending;
std::set<XrSpace> gSpacesDestroyPending;

// Events copied and queued from host XrPollEvent for later pickup by remote.
enum { MAX_REMOTE_QUEUE_EVENTS = 16 };
typedef std::unique_ptr<XrEventDataBuffer> EventDataBufferPtr;
std::queue<EventDataBufferPtr> gHostEventsSaved;

// Mutex synchronizing access to Main session and Overlay session commands
HANDLE gOverlayCallMutex = NULL;      // handle to sync object
LPCWSTR kOverlayMutexName = TEXT("XR_EXT_overlay_call_mutex");

static XrGeneratedDispatchTable *downchain = nullptr;

// Bookkeeping of SwapchainImages for copying remote SwapchainImages on ReleaseSwapchainImage
struct SwapchainCachedData
{
    XrSwapchain swapchain;
    std::vector<ID3D11Texture2D*> swapchainImages;
    std::set<HANDLE> remoteImagesAcquired;
    std::map<HANDLE, ID3D11Texture2D*> handleTextureMap;
    std::vector<uint32_t>   acquired;

    SwapchainCachedData(XrSwapchain swapchain_, const std::vector<ID3D11Texture2D*>& swapchainImages_) :
        swapchain(swapchain_),
        swapchainImages(swapchainImages_)
    {
        for(auto texture : swapchainImages) {
            texture->AddRef();
        }
    }

    ~SwapchainCachedData()
    {
        for(HANDLE acquired : remoteImagesAcquired) {
            IDXGIKeyedMutex* keyedMutex;
            {
                ID3D11Texture2D *sharedTexture = getSharedTexture(acquired);
                CHECK(sharedTexture->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex));
            }
            CHECK(keyedMutex->ReleaseSync(KEYED_MUTEX_IPC_REMOTE));
            keyedMutex->Release();
        }
        remoteImagesAcquired.clear();
        for(auto shared : handleTextureMap) {
            shared.second->Release();
            CloseHandle(shared.first);
        }
        for(auto texture : swapchainImages) {
            texture->Release();
        }
        handleTextureMap.clear();
    }

    ID3D11Texture2D* getSharedTexture(HANDLE sourceHandle)
    {
        ID3D11Texture2D *sharedTexture;

        ID3D11Device1 *device1;
        CHECK(gSavedD3DDevice->QueryInterface(__uuidof (ID3D11Device1), (void **)&device1));
        auto it = handleTextureMap.find(sourceHandle);
        if(it == handleTextureMap.end()) {
            CHECK(device1->OpenSharedResource1(sourceHandle, __uuidof(ID3D11Texture2D), (LPVOID*) &sharedTexture));
            handleTextureMap[sourceHandle] = sharedTexture;
        } else  {
            sharedTexture = it->second;
        }
        device1->Release();

        return sharedTexture;
    }
};

typedef std::unique_ptr<SwapchainCachedData> SwapchainCachedDataPtr;
std::map<XrSwapchain, SwapchainCachedDataPtr> gSwapchainMap;

void ClearOverlayLayers()
{
    for(auto* l: gOverlayLayers) {
        FreeXrStructChainWithFree(l);
    }
    gOverlayLayers.clear();
}

const XrCompositionLayerBaseHeader* FindLayerReferencingSwapchain(XrSwapchain swapchain)
{
    for(const auto* l : gOverlayLayers) {
        while(l != nullptr) {
            switch(l->type) {
                case XR_TYPE_COMPOSITION_LAYER_QUAD: {
                    auto p2 = reinterpret_cast<const XrCompositionLayerQuad*>(l);
                    if(p2->subImage.swapchain == swapchain) {
                        return l;
                    }
                    break;
                }
                case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
                    auto p2 = reinterpret_cast<const XrCompositionLayerProjection*>(l);
                    for(uint32_t j = 0; j < p2->viewCount; j++) {
                        if(p2->views[j].subImage.swapchain == swapchain) {
                            return l;
                        }
                    }
                    break;
                }
                case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR: {
                    auto p2 = reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(l);
                    if(p2->subImage.swapchain == swapchain) {
                        return l;
                    }
                    break;
                }
                default: {
                    OutputDebugStringA(fmt("**OVERLAY** Warning: FindLayerReferencingSwapchain skipping compositor layer of unknown type %d\n", l->type).c_str());
                }
            }
            l = reinterpret_cast<const XrCompositionLayerBaseHeader*>(l->next);
        }
    }
    return nullptr;
}

const XrCompositionLayerBaseHeader* FindLayerReferencingSpace(XrSpace space)
{
    for(const auto* l : gOverlayLayers) {
        while(l != nullptr) {
            switch(l->type) {
                case XR_TYPE_COMPOSITION_LAYER_QUAD: {
                    auto p2 = reinterpret_cast<const XrCompositionLayerQuad*>(l);
                    if(p2->space == space) {
                        return l;
                    }
                    break;
                }
                case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
                    auto p2 = reinterpret_cast<const XrCompositionLayerProjection*>(l);
                    if(p2->space == space) {
                        return l;
                    }
                    break;
                }
                case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR: {
                    // Nothing - DepthInfoKHR Is chained from Projection, which has the space
                    break;
                }
                default: {
                    OutputDebugStringA(fmt("**OVERLAY** Warning: FindLayerReferencingSwapchain skipping compositor layer of unknown type %d\n", l->type).c_str());
                }
            }
            l = reinterpret_cast<const XrCompositionLayerBaseHeader*>(l->next);
        }
    }
    return nullptr;
}

typedef std::pair<bool, XrSessionState> OptionalSessionStateChange;

struct OverlayAppSession
{
    enum OpenXRCommand {
        BEGIN_SESSION,
        WAIT_FRAME,
        END_SESSION,
        REQUEST_EXIT_SESSION,
    };
    enum SessionLossState {
        NOT_LOST,
        LOSS_PENDING,
        LOST,
    } sessionLossState;
    XrSessionState currentOverlayXrSessionState;
    XrSessionState currentMainXrSessionState;
    XrTime currentMainXrSessionTime;

    bool isOverlaySessionRunning;
    bool exitRequested;
    bool isMainSessionRunning;

    bool hasMainSessionCalledWaitFrame;
    XrFrameState savedFrameState; // XXX this is a struct and as such needs to be deep copied

    OverlayAppSession() :
        currentOverlayXrSessionState(XR_SESSION_STATE_IDLE),
        isOverlaySessionRunning(false),
        sessionLossState(NOT_LOST),
        isMainSessionRunning(false),
        hasMainSessionCalledWaitFrame(false)
    {}

    void DoMainSessionStateChange(XrSessionState state, XrTime when)
    {
        currentMainXrSessionState = state;
        currentMainXrSessionTime = when;
        OutputDebugStringA(fmt("**OVERLAY** main session is now %d\n", state).c_str());
    }

    void DoOverlaySessionCommand(OpenXRCommand command)
    {
        if(command == BEGIN_SESSION) {
            isOverlaySessionRunning = true;
        } else if (command == END_SESSION) {
            isOverlaySessionRunning = false;
        } else if (command == REQUEST_EXIT_SESSION) {
            exitRequested = true;
        }
    }

    void DoMainSessionCommand(OpenXRCommand command)
    {
        if(command == BEGIN_SESSION) {
            isMainSessionRunning = true;
            hasMainSessionCalledWaitFrame = true;
        } else if (command == END_SESSION) {
            isMainSessionRunning = false;
        } else if (command == WAIT_FRAME) {
            // XXX saved predicted times updated separately
        }
    }

    void DoMainSessionLost()
    {
        sessionLossState = LOST;
    }
    
    OptionalSessionStateChange GetAndDoPendingStateChange();
    SessionLossState GetSessionLossState()
    {
        return sessionLossState;
    }

    void IncrementPredictedDisplayTime()
    {
        savedFrameState.predictedDisplayTime += 1; // XXX This is legal, but unclear that it won't cause a FP exception...
    }
} overlayAppSession;

OptionalSessionStateChange OverlayAppSession::GetAndDoPendingStateChange()
{
    if((currentOverlayXrSessionState != XR_SESSION_STATE_LOSS_PENDING) &&
        ((GetSessionLossState() == LOST) ||
        (GetSessionLossState() == LOSS_PENDING))) {

        return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_LOSS_PENDING };
    }

    switch(currentOverlayXrSessionState) {
        case XR_SESSION_STATE_IDLE:
            if(exitRequested || (currentMainXrSessionState == XR_SESSION_STATE_EXITING)) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_EXITING };
            } else if(isMainSessionRunning && hasMainSessionCalledWaitFrame) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_READY };
            }
            break;

        case XR_SESSION_STATE_READY:
            if(isOverlaySessionRunning) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } 
            break;

        case XR_SESSION_STATE_SYNCHRONIZED:
            if(exitRequested || !isMainSessionRunning || (currentMainXrSessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_STOPPING };
            } else if((currentMainXrSessionState == XR_SESSION_STATE_VISIBLE) || (currentMainXrSessionState == XR_SESSION_STATE_FOCUSED)) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_VISIBLE };
            } 
            break;

        case XR_SESSION_STATE_VISIBLE:
            if(exitRequested || !isMainSessionRunning || (currentMainXrSessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } else if(currentMainXrSessionState == XR_SESSION_STATE_SYNCHRONIZED) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } else if(currentMainXrSessionState == XR_SESSION_STATE_FOCUSED) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_FOCUSED };
            }
            break;

        case XR_SESSION_STATE_FOCUSED:
            if(exitRequested || !isMainSessionRunning || (currentMainXrSessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_VISIBLE };
            } else if((currentMainXrSessionState == XR_SESSION_STATE_VISIBLE) || (currentMainXrSessionState == XR_SESSION_STATE_SYNCHRONIZED)) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_VISIBLE };
            }
            break;

        case XR_SESSION_STATE_STOPPING:
            if(!isOverlaySessionRunning) {
                return OptionalSessionStateChange { true, currentOverlayXrSessionState = XR_SESSION_STATE_IDLE };
            }
            break;

        default:
            // No other combination of states requires an Overlay SessionStateChange
            break;
    }

    return OptionalSessionStateChange { false, XR_SESSION_STATE_UNKNOWN };
}

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
    void *shmem = IPCGetSharedMemory();
    bool connectionIsActive = false;
    HANDLE remoteProcessHandle = 0;

    do {
        // XXX Super Awkward, should probably have a separate Handshake loop
        IPCWaitResult result;
        if(connectionIsActive) {
            result = IPCWaitForGuestRequestOrTermination(remoteProcessHandle);
        } else {
            result = IPCWaitForGuestRequest();
        }

        if(result == IPC_REMOTE_PROCESS_TERMINATED) {

            DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
            if(waitresult == WAIT_TIMEOUT) {
                OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
                DebugBreak();
            }
            gSwapchainMap.clear();
            ClearOverlayLayers();
            ReleaseMutex(gOverlayCallMutex);
            connectionIsActive = false;
            continue;

        } else if(result == IPC_WAIT_ERROR) {

            DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
            if(waitresult == WAIT_TIMEOUT) {
                OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
                DebugBreak();
            }
            gSwapchainMap.clear();
            ClearOverlayLayers();
            ReleaseMutex(gOverlayCallMutex);
            connectionIsActive = false;
            OutputDebugStringA("IPC Wait Error\n");
            break;
        }

        IPCBuffer ipcbuf = IPCGetBuffer();
        IPCXrHeader *hdr;
        hdr = ipcbuf.getAndAdvance<IPCXrHeader>();

        hdr->makePointersAbsolute(ipcbuf.base);

        switch(hdr->requestType) {

            // XXX make all these into functions

            case IPC_XR_ENUMERATE_INSTANCE_EXTENSION_PROPERTIES: {
                auto args = ipcbuf.getAndAdvance<IPCXrEnumerateInstanceExtensionProperties>();
                hdr->result = downchain->EnumerateInstanceExtensionProperties(args->layerName, args->propertyCapacityInput, args->propertyCountOutput, args->properties);
                break;
            }

            case IPC_XR_CREATE_INSTANCE: {
                // Establish IPC parameters and make initial handshake
                auto args = ipcbuf.getAndAdvance<IPCXrCreateInstance>();

                // Wait on main session
                if(!gMainSessionCreated) {
                    DWORD waitresult = WaitForSingleObject(gOverlayCreateSessionSema, INFINITE);
                    if(waitresult == WAIT_TIMEOUT) {
                        OutputDebugStringA("**OVERLAY** create session timeout\n");
                        DebugBreak();
                    }
                }

                bool extensionsSupported = true;
                for(unsigned int i = 0; extensionsSupported && (i < args->createInfo->enabledExtensionCount); i++) {
                    extensionsSupported = gSavedRequestedExtensions.find(args->createInfo->enabledExtensionNames[i]) != gSavedRequestedExtensions.end();
                }

                bool apiLayersSupported = true;
                for(unsigned int i = 0; apiLayersSupported && (i < args->createInfo->enabledApiLayerCount); i++) {
                    apiLayersSupported = gSavedRequestedApiLayers.find(args->createInfo->enabledApiLayerNames[i]) != gSavedRequestedApiLayers.end();
                }

                if(!apiLayersSupported) {
                    hdr->result = XR_ERROR_API_LAYER_NOT_PRESENT;
                } else if(!extensionsSupported) {
                    hdr->result = XR_ERROR_EXTENSION_NOT_PRESENT;
                } else {
                    hdr->result = XR_SUCCESS;
                    connectionIsActive = true;
                    DWORD remote = args->remoteProcessId;
                    CHECK_NOT_NULL(remoteProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, args->remoteProcessId));

                    *(args->instance) = gSavedInstance;
                    *(args->hostProcessId) = GetCurrentProcessId();
                }

                break;
            }

            case IPC_XR_CREATE_SESSION: {
                auto args = ipcbuf.getAndAdvance<IPCXrCreateSession>();
                hdr->result = Overlay_xrCreateSession(args->instance, args->createInfo, args->session);
                break;
            }

            case IPC_XR_GET_SYSTEM: {
                auto args = ipcbuf.getAndAdvance<IPCXrGetSystem>();
                if(args->getInfo->formFactor == gSavedFormFactor) {
                    *args->systemId = gSavedSystemId;
                    hdr->result = XR_SUCCESS;
                } else {
                    *args->systemId = XR_NULL_SYSTEM_ID;
                    hdr->result = XR_ERROR_FORM_FACTOR_UNAVAILABLE;
                }
                break;
            }

            case IPC_XR_CREATE_REFERENCE_SPACE: {
                auto args = ipcbuf.getAndAdvance<IPCXrCreateReferenceSpace>();
                hdr->result = Overlay_xrCreateReferenceSpace(args->session, args->createInfo, args->space);
                break;
            }

            case IPC_XR_GET_INSTANCE_PROPERTIES: {
                auto args = ipcbuf.getAndAdvance<IPCXrGetInstanceProperties>();
                hdr->result = downchain->GetInstanceProperties(args->instance, args->properties);
                break;
            }

            case IPC_XR_GET_SYSTEM_PROPERTIES: {
                auto args = ipcbuf.getAndAdvance<IPCXrGetSystemProperties>();
                hdr->result = downchain->GetSystemProperties(args->instance, args->system, args->properties);
                // Remote only gets the overlay layers
                args->properties->graphicsProperties.maxLayerCount = MAX_OVERLAY_LAYER_COUNT;
                break;
            }

            case IPC_XR_LOCATE_SPACE: {
                auto args = ipcbuf.getAndAdvance<IPCXrLocateSpace>();
                hdr->result = Overlay_xrLocateSpace(args->space, args->baseSpace, args->time, args->spaceLocation);
                break;
            }

            case IPC_XR_ENUMERATE_SWAPCHAIN_FORMATS: { 
                auto args = ipcbuf.getAndAdvance<IPCXrEnumerateSwapchainFormats>();
                hdr->result = Overlay_xrEnumerateSwapchainFormats(args->session, args->formatCapacityInput, args->formatCountOutput, args->formats);
                break;
            }

            case IPC_XR_CREATE_SWAPCHAIN: {
                auto args = ipcbuf.getAndAdvance<IPCXrCreateSwapchain>();
                hdr->result = Overlay_xrCreateSwapchain(args->session, args->createInfo, args->swapchain);
                if(hdr->result == XR_SUCCESS) {

                    uint32_t count;
                    CHECK_XR(downchain->EnumerateSwapchainImages(*args->swapchain, 0, &count, nullptr));

                    std::vector<XrSwapchainImageD3D11KHR> swapchainImages(count);
                    std::vector<ID3D11Texture2D*> swapchainTextures(count);
                    for(uint32_t i = 0; i < count; i++) {
                        swapchainImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
                        swapchainImages[i].next = nullptr;
                    }
                    CHECK_XR(downchain->EnumerateSwapchainImages(*args->swapchain, count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data())));

                    for(uint32_t i = 0; i < count; i++) {
                        swapchainTextures[i] = swapchainImages[i].texture;
                    }
                  
                    gSwapchainMap[*args->swapchain] = SwapchainCachedDataPtr(new SwapchainCachedData(*args->swapchain, swapchainTextures));
                    *args->swapchainCount = count;
                }
                break;
            }

            case IPC_XR_BEGIN_FRAME: {
                auto args = ipcbuf.getAndAdvance<IPCXrBeginFrame>();
                hdr->result = Overlay_xrBeginFrame(args->session, args->frameBeginInfo);
                break;
            }

            case IPC_XR_WAIT_FRAME: {
                auto args = ipcbuf.getAndAdvance<IPCXrWaitFrame>();
                hdr->result = Overlay_xrWaitFrame(args->session, args->frameWaitInfo, args->frameState);
                break;
            }

            case IPC_XR_END_FRAME: {
                auto args = ipcbuf.getAndAdvance<IPCXrEndFrame>();
                hdr->result = Overlay_xrEndFrame(args->session, args->frameEndInfo);
                break;
            }

            case IPC_XR_ACQUIRE_SWAPCHAIN_IMAGE: {
                auto args = ipcbuf.getAndAdvance<IPCXrAcquireSwapchainImage>();
                hdr->result = Overlay_xrAcquireSwapchainImage(args->swapchain, args->acquireInfo, args->index);
                if(hdr->result == XR_SUCCESS) {
                    auto& cache = gSwapchainMap[args->swapchain];
                    cache->acquired.push_back(*args->index);
                }
                break;
            }

            case IPC_XR_WAIT_SWAPCHAIN_IMAGE: {
                auto args = ipcbuf.getAndAdvance<IPCXrWaitSwapchainImage>();
                hdr->result = Overlay_xrWaitSwapchainImage(args->swapchain, args->waitInfo);
                auto& cache = gSwapchainMap[args->swapchain];
                if(cache->remoteImagesAcquired.find(args->sourceImage) != cache->remoteImagesAcquired.end()) {
                    IDXGIKeyedMutex* keyedMutex;
                    {
                        ID3D11Texture2D *sharedTexture = cache->getSharedTexture(args->sourceImage);
                        CHECK(sharedTexture->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex));
                    }
                    cache->remoteImagesAcquired.erase(args->sourceImage);
                    CHECK(keyedMutex->ReleaseSync(KEYED_MUTEX_IPC_REMOTE));
                    keyedMutex->Release();
                }
                break;
            }

            case IPC_XR_RELEASE_SWAPCHAIN_IMAGE: {
                auto args = ipcbuf.getAndAdvance<IPCXrReleaseSwapchainImage>();
                auto& cache = gSwapchainMap[args->swapchain];

                ID3D11Texture2D *sharedTexture = cache->getSharedTexture(args->sourceImage);

                {
                    IDXGIKeyedMutex* keyedMutex;
                    CHECK(sharedTexture->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex));
                    CHECK(keyedMutex->AcquireSync(KEYED_MUTEX_IPC_HOST, INFINITE));
                    keyedMutex->Release();
                }

                cache->remoteImagesAcquired.insert(args->sourceImage);
                int which = cache->acquired[0];
                cache->acquired.erase(cache->acquired.begin());

                ID3D11DeviceContext* d3dContext;
                gSavedD3DDevice->GetImmediateContext(&d3dContext);
                d3dContext->CopyResource(cache->swapchainImages[which], sharedTexture);
                hdr->result = Overlay_xrReleaseSwapchainImage(args->swapchain, args->releaseInfo);
                break;
            }

            case IPC_XR_DESTROY_SESSION: { 
                auto args = ipcbuf.getAndAdvance<IPCXrDestroySession>();
                hdr->result = Overlay_xrDestroySession(args->session);
                DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
                if(waitresult == WAIT_TIMEOUT) {
                    OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
                    DebugBreak();
                }
                gSwapchainMap.clear();
                ReleaseMutex(gOverlayCallMutex);
                break;
            }

            case IPC_XR_ENUMERATE_VIEW_CONFIGURATIONS: {
                auto args = ipcbuf.getAndAdvance<IPCXrEnumerateViewConfigurations>();
                hdr->result = downchain->EnumerateViewConfigurations(args->instance, args->systemId, args->viewConfigurationTypeCapacityInput, args->viewConfigurationTypeCountOutput, args->viewConfigurationTypes);
                break;
            }

            case IPC_XR_ENUMERATE_VIEW_CONFIGURATION_VIEWS: {
                auto args = ipcbuf.getAndAdvance<IPCXrEnumerateViewConfigurationViews>();
                hdr->result = downchain->EnumerateViewConfigurationViews(args->instance, args->systemId, args->viewConfigurationType, args->viewCapacityInput, args->viewCountOutput, args->views);
                break;
            }

            case IPC_XR_GET_VIEW_CONFIGURATION_PROPERTIES: {
                auto args = ipcbuf.getAndAdvance<IPCXrGetViewConfigurationProperties>();
                hdr->result = downchain->GetViewConfigurationProperties(args->instance, args->systemId, args->viewConfigurationType, args->configurationProperties);
                break;
            }

            case IPC_XR_DESTROY_SWAPCHAIN: {
                auto args = ipcbuf.getAndAdvance<IPCXrDestroySwapchain>();
                hdr->result = Overlay_xrDestroySwapchain(args->swapchain);
                gSwapchainMap.erase(args->swapchain);
                break;
            }

            case IPC_XR_DESTROY_SPACE: {
                auto args = ipcbuf.getAndAdvance<IPCXrDestroySpace>();
                hdr->result = Overlay_xrDestroySpace(args->space);
                break;
            }

            case IPC_XR_BEGIN_SESSION: {
                auto args = ipcbuf.getAndAdvance<IPCXrBeginSession>();
                hdr->result = Overlay_xrBeginSession(args->session, args->beginInfo);
                break;
            }

            case IPC_XR_REQUEST_EXIT_SESSION: {
                auto args = ipcbuf.getAndAdvance<IPCXrRequestExitSession>();
                hdr->result = Overlay_xrRequestExitSession(args->session);
                break;
            }

            case IPC_XR_END_SESSION: {
                auto args = ipcbuf.getAndAdvance<IPCXrEndSession>();
                hdr->result = Overlay_xrEndSession(args->session);
                break;
            }

            case IPC_XR_GET_D3D11_GRAPHICS_REQUIREMENTS_KHR: {
                auto args = ipcbuf.getAndAdvance<IPCXrGetD3D11GraphicsRequirementsKHR>();
                hdr->result = Overlay_xrGetD3D11GraphicsRequirementsKHR(args->instance, args->systemId, args->graphicsRequirements);
                break;
            }

            case IPC_XR_POLL_EVENT: {
                auto args = ipcbuf.getAndAdvance<IPCXrPollEvent>();
                DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
                if(waitresult == WAIT_TIMEOUT) {
                    OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
                    DebugBreak();
                }

                OptionalSessionStateChange pendingStateChange;
                pendingStateChange = overlayAppSession.GetAndDoPendingStateChange();

                if(pendingStateChange.first) {

                    EventDataBufferPtr event(new XrEventDataBuffer);
                    auto* ssc = reinterpret_cast<XrEventDataSessionStateChanged*>(event.get());
                    ssc->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
                    ssc->next = nullptr;
                    ssc->session = kOverlayFakeSession;
                    ssc->state = pendingStateChange.second;
                    XrTime calculatedTime = 0; // XXX Must not be 0 - should calculate from system time but cannot guarantee conversion routines will be available
                    ssc->time = calculatedTime;
                    OutputDebugStringA(fmt("**OVERLAY** synthesizing a session changed Event to %d\n", ssc->state).c_str());
                    CopyEventChainIntoBuffer(reinterpret_cast<XrEventDataBaseHeader *>(event.get()), args->event);
                    hdr->result = XR_SUCCESS;

                } else if(gHostEventsSaved.size() == 0) {

                    hdr->result = XR_EVENT_UNAVAILABLE;

                } else {

                    EventDataBufferPtr event(std::move(gHostEventsSaved.front()));
                    gHostEventsSaved.pop();
                    OutputDebugStringA(fmt("**OVERLAY** dequeued a %d Event, length %d\n", event->type, gHostEventsSaved.size()).c_str());
                    CopyEventChainIntoBuffer(reinterpret_cast<XrEventDataBaseHeader*>(event.get()), args->event);

                    // Find all XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED.  If the session is our Main,
                    // change the session to be our overlay session stand-in.
                    const void *next = args->event;
                    while(next != nullptr) {
                        const auto* e = reinterpret_cast<const XrEventDataBaseHeader*>(next);
                        if(e->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                            const auto* ssc = reinterpret_cast<const XrEventDataSessionStateChanged*>(e);
                            if(ssc->session == gSavedMainSession) {
                                auto* ssc2 = const_cast<XrEventDataSessionStateChanged*>(ssc);
                                ssc2->session = kOverlayFakeSession;
                            }
                        }
                        next = e->next;
                    }

                    // args->event is already registered as a pointer
                    // that has to be updated as it passes over IPC.
                    // Add all newly created *subsequent* next pointers in
                    // the chain, too.
                    const void** tofix = &(args->event->next);
                    while (*tofix != nullptr) {
                        hdr->addOffsetToPointer(ipcbuf.base, tofix);
                        const XrEventDataBaseHeader* qq = reinterpret_cast<const XrEventDataBaseHeader*>(*tofix);
                        tofix = const_cast<const void **>(&(qq->next));
                    }
                    // add next pointers to the pointer list

                    hdr->result = XR_SUCCESS;
                    // event goes out of scope and is deleted
                }
                ReleaseMutex(gOverlayCallMutex);

                break;
            }

            default:
                OutputDebugStringA("unknown request type in IPC");
                abort();
                break;
        }

        hdr->makePointersRelative(ipcbuf.base);
        IPCFinishHostResponse();

    } while(!gExitIPCLoop);

    return 0;
}

void CreateOverlaySessionThread()
{
    CHECK_NOT_NULL(gOverlayCreateSessionSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayCreateSessionSemaName));
    CHECK_NOT_NULL(gOverlayWaitFrameSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayWaitFrameSemaName));
    CHECK_NOT_NULL(gOverlayCallMutex = CreateMutex(nullptr, TRUE, kOverlayMutexName));
    ReleaseMutex(gOverlayCallMutex);

    CHECK_NOT_NULL(gOverlayWorkerThread =
        CreateThread(nullptr, 0, ThreadBody, nullptr, 0, &gOverlayWorkerThreadId));
    OutputDebugStringA("**OVERLAY** success creating IPC thread\n");
}

XrResult Overlay_xrCreateApiLayerInstance(const XrInstanceCreateInfo *info, const struct XrApiLayerCreateInfo *apiLayerInfo, XrInstance *instance) 
{
    assert(0 == strncmp(kOverlayLayerName, apiLayerInfo->nextInfo->layerName, strnlen_s(kOverlayLayerName, XR_MAX_API_LAYER_NAME_SIZE)));
    assert(nullptr != apiLayerInfo->nextInfo);

    gSavedRequestedExtensions.clear();
    gSavedRequestedExtensions.insert("XR_EXT_overlay");
    gSavedRequestedExtensions.insert(info->enabledExtensionNames, info->enabledExtensionNames + info->enabledExtensionCount);

    gSavedRequestedApiLayers.clear();
    // Is there a way to query which layers are only downstream?
    // We can't get to the functionality of layers upstream (closer to
    // the app), so we can't claim all these layers are enabled (the remote
    // app can't use these layers)
    // gSavedRequestedApiLayers.insert(info->enabledApiLayerNames, info->enabledApiLayerNames + info->enabledApiLayerCount);

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

XrResult Overlay_xrGetSystemProperties(
    XrInstance instance,
    XrSystemId systemId,
    XrSystemProperties* properties)
{
    if(gSerializeEverything) {
        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
            DebugBreak();
        }
    }

    XrResult result;

    result = downchain->GetSystemProperties(instance, systemId, properties);

    if(result == XR_SUCCESS) {

        // Reserve one for overlay
        // TODO : should remove for main session, should return only max overlay layers for overlay session
        properties->graphicsProperties.maxLayerCount =
            properties->graphicsProperties.maxLayerCount - MAX_OVERLAY_LAYER_COUNT;
    }

    if(gSerializeEverything) {
        ReleaseMutex(gOverlayCallMutex);
    }

    return result;
}

XrResult Overlay_xrBeginSession(
    XrSession session,
    const XrSessionBeginInfo*                   beginInfo)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {
        if(overlayAppSession.currentOverlayXrSessionState != XR_SESSION_STATE_READY) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_NOT_READY;
        }
        overlayAppSession.DoOverlaySessionCommand(OverlayAppSession::BEGIN_SESSION);

        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            // simulate the operation if possible
            ReleaseMutex(gOverlayCallMutex);
            return XR_SESSION_LOSS_PENDING;
        }
        result = XR_SUCCESS;

    } else {

        result = downchain->BeginSession(session, beginInfo);
        overlayAppSession.DoMainSessionCommand(OverlayAppSession::BEGIN_SESSION);

    }

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrRequestExitSession(
    XrSession session)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {

        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            // simulate the operation if possible
            ReleaseMutex(gOverlayCallMutex);
            return XR_SESSION_LOSS_PENDING;
        }
        overlayAppSession.DoOverlaySessionCommand(OverlayAppSession::REQUEST_EXIT_SESSION);
        result = downchain->RequestExitSession(gSavedMainSession);

    } else {

        result = downchain->RequestExitSession(session);

    }

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrEndSession(
    XrSession session)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {

        if(overlayAppSession.currentOverlayXrSessionState != XR_SESSION_STATE_STOPPING) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_NOT_STOPPING;
        }
        overlayAppSession.DoOverlaySessionCommand(OverlayAppSession::END_SESSION);

        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            ReleaseMutex(gOverlayCallMutex);
            // simulate the operation if possible
            return XR_SESSION_LOSS_PENDING;
        }
        result = XR_SUCCESS;

    } else {

        result = downchain->EndSession(session);
        overlayAppSession.DoMainSessionCommand(OverlayAppSession::END_SESSION);

    }

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrDestroySession(
    XrSession session)
{
    XrResult result;

    if(session == kOverlayFakeSession) {
        // overlay session

        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if (waitresult == WAIT_TIMEOUT) {
                OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
                DebugBreak();
        }

        ClearOverlayLayers();
        result = XR_SUCCESS;

        ReleaseMutex(gOverlayCallMutex);

    } else {
        // main session

        gExitIPCLoop = true;

        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if (waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
            DebugBreak();
        }

        result = downchain->DestroySession(session);
        overlayAppSession.DoMainSessionLost();

        ReleaseMutex(gOverlayCallMutex);
    }

    return result;
}

XrResult Overlay_xrDestroySwapchain(XrSwapchain swapchain)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    bool isSubmitted = (FindLayerReferencingSwapchain(swapchain) == nullptr);

    if(isSubmitted) {
        gSwapchainsDestroyPending.insert(swapchain);
        result = XR_SUCCESS;
    } else {
        result = downchain->DestroySwapchain(swapchain);
    }

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrDestroySpace(XrSpace space)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    bool isSubmitted = (FindLayerReferencingSpace(space) == nullptr);

    if(isSubmitted) {
        gSpacesDestroyPending.insert(space);
        result = XR_SUCCESS;
    } else {
        result = downchain->DestroySpace(space);
    }

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session)
{
    XrResult result;

    const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
    const XrSessionCreateInfoOverlayEXT* cio = nullptr;
    const XrGraphicsBindingD3D11KHR* d3dbinding = nullptr;
    while(p != nullptr) {
        if(p->type == XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT) {
            cio = reinterpret_cast<const XrSessionCreateInfoOverlayEXT*>(p);
        }
        if( (p->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) ||
            (p->type == XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR) ||
            (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR) ||
            (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR) ||
            (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR) ||
            (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR) ||
            (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR) ||
            (p->type == XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR)) {
            return XR_ERROR_GRAPHICS_DEVICE_INVALID;
        }
        if(p->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
            d3dbinding = reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(p);
        }
        p = reinterpret_cast<const XrBaseInStructure*>(p->next);
    }

    // TODO handle the case where Main session passes the
    // overlaycreateinfo but overlaySession = FALSE
    if(cio == nullptr) {

        // Main session

        // TODO : remake chain without InfoOverlayEXT

        result = downchain->CreateSession(instance, createInfo, session);
        if(result != XR_SUCCESS)
            return result;

        gSavedSystemId = createInfo->systemId;
        gSavedMainSession = *session;
        gSavedD3DDevice = d3dbinding->device;
        ID3D11Multithread* d3dMultithread;
        CHECK(gSavedD3DDevice->QueryInterface(__uuidof(ID3D11Multithread), reinterpret_cast<void**>(&d3dMultithread)));
        d3dMultithread->SetMultithreadProtected(TRUE);
        d3dMultithread->Release();

        // Let overlay session continue
        gMainSessionCreated = true;
        ReleaseSemaphore(gOverlayCreateSessionSema, 1, nullptr);
		 
    } else {

        // TODO should store any kind of failure in main XrCreateSession and then fall through here
        *session = kOverlayFakeSession;
        result = XR_SUCCESS;
    }

    return result;
}

XrResult Overlay_xrEnumerateSwapchainFormats(
    XrSession                                   session,
    uint32_t                                    formatCapacityInput,
    uint32_t*                                   formatCountOutput,
    int64_t*                                    formats)
{ 
    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            // simulate the operation if possible
            *formatCountOutput = 0;
            ReleaseMutex(gOverlayCallMutex);
            return XR_SESSION_LOSS_PENDING;
        }
        session = gSavedMainSession;
    }

    XrResult result = downchain->EnumerateSwapchainFormats(session, formatCapacityInput, formatCountOutput, formats);

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images)
{ 
    if(gSerializeEverything) {
        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
            DebugBreak();
        }
    }

    XrResult result = downchain->EnumerateSwapchainImages(swapchain, imageCapacityInput, imageCountOutput, images);

    if(gSerializeEverything) {
        ReleaseMutex(gOverlayCallMutex);
    }

    return result;
}

XrResult Overlay_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
{ 
    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            // simulate the operation if possible
            ReleaseMutex(gOverlayCallMutex);
            return XR_SESSION_LOSS_PENDING;
        }
        session = gSavedMainSession;
    }

    XrResult result = downchain->CreateReferenceSpace(session, createInfo, space);

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrCreateSwapchain(XrSession session, const  XrSwapchainCreateInfo *createInfo, XrSwapchain *swapchain) 
{ 
    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            ReleaseMutex(gOverlayCallMutex);
            // simulate the operation if possible
            return XR_SESSION_LOSS_PENDING;
        }
        session = gSavedMainSession;
    }

    XrResult result = downchain->CreateSwapchain(session, createInfo, swapchain);
    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
{ 
    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

	XrResult result = downchain->LocateSpace(space, baseSpace, time, location);

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrWaitFrame(XrSession session, const XrFrameWaitInfo *info, XrFrameState *state) 
{
    XrResult result;
    DWORD waitresult;

    if(session == kOverlayFakeSession) {
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            // Give up at WaitFrame and just return error
            overlayAppSession.DoMainSessionLost();
            return XR_ERROR_SESSION_LOST;
        }

        waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE); // XXX should timeout gracefully
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
            DebugBreak();
        }

        // XXX Main may even change states between this and the WaitForSingleObject.
        bool waitOnHostWaitFrame = overlayAppSession.isMainSessionRunning && (overlayAppSession.currentMainXrSessionState != XR_SESSION_STATE_STOPPING);

        ReleaseMutex(gOverlayCallMutex);

        // Wait on main session
        if(waitOnHostWaitFrame) {
            waitresult = WaitForSingleObject(gOverlayWaitFrameSema, OVERLAY_WAITFRAME_NORMAL_MILLIS); // Try to sync up with Main WaitFrame
            if(waitresult == WAIT_TIMEOUT) {
                // Overlay can't hang - Main may have transitioned to STOPPING or out of "running".
                OutputDebugStringA("**OVERLAY** overlay session WaitFrame timeout\n");
                // DebugBreak();
            } else {
                if(waitresult == WAIT_OBJECT_0){
                    // This is what we want, no action
                }
            } // XXX handle WAIT_ABANDONED also
        }

        // TODO pass back any failure recorded by main session waitframe
        result = XR_SUCCESS;
        *state = overlayAppSession.savedFrameState;
        overlayAppSession.IncrementPredictedDisplayTime();

    } else {

        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
            DebugBreak();
        }

        result = downchain->WaitFrame(session, info, state);
        overlayAppSession.savedFrameState = *state;
        overlayAppSession.DoMainSessionCommand(OverlayAppSession::WAIT_FRAME);

        ReleaseMutex(gOverlayCallMutex);

        ReleaseSemaphore(gOverlayWaitFrameSema, 1, nullptr);
    }

    return result;
}

XrResult Overlay_xrBeginFrame(XrSession session, const XrFrameBeginInfo *info) 
{ 
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {

        // Do nothing in overlay session
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            // simulate the operation if possible
            ReleaseMutex(gOverlayCallMutex);
            return XR_SESSION_LOSS_PENDING;
        }
        result = XR_SUCCESS;

    } else {

        result = downchain->BeginFrame(session, info);

    }

    ReleaseMutex(gOverlayCallMutex);
    return result;
}

XrResult Overlay_xrEndFrame(XrSession session, const XrFrameEndInfo *info) 
{
    XrResult result = XR_SUCCESS;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {

        // TODO: validate blend mode matches main session
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOST) {
            ReleaseMutex(gOverlayCallMutex);
            return XR_ERROR_SESSION_LOST;
        }
        if(overlayAppSession.GetSessionLossState() == OverlayAppSession::LOSS_PENDING) {
            // simulate the operation if possible
            ReleaseMutex(gOverlayCallMutex);
            return XR_SESSION_LOSS_PENDING;
        }

        ClearOverlayLayers();
        if(info->layerCount > MAX_OVERLAY_LAYER_COUNT) {
            result = XR_ERROR_LAYER_LIMIT_EXCEEDED;
        } else {
            for(uint32_t i = 0; (result == XR_SUCCESS) && (i < info->layerCount); i++) {
                const XrBaseInStructure *copy = CopyXrStructChainWithMalloc(info->layers[i]);
                if(copy == nullptr) {
                    result = XR_ERROR_OUT_OF_MEMORY;
                    ClearOverlayLayers();
                } else {
                    gOverlayLayers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(copy));
                }
            }
        }

    } else {

        XrFrameEndInfo info2 = *info;
        std::unique_ptr<const XrCompositionLayerBaseHeader*> layers2(new const XrCompositionLayerBaseHeader*[info->layerCount + gOverlayLayers.size()]);
        memcpy(layers2.get(), info->layers, sizeof(const XrCompositionLayerBaseHeader*) * info->layerCount);
        for(uint32_t i = 0; i < gOverlayLayers.size(); i++) {
            layers2.get()[info->layerCount + i] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(gOverlayLayers[i]);
	}

        info2.layerCount = info->layerCount + static_cast<uint32_t>(gOverlayLayers.size());
        info2.layers = layers2.get();

        result = downchain->EndFrame(session, &info2);

        // XXX there's probably an elegant C++ find with lambda that would do this:
        auto copyOfPendingDestroySwapchains = gSwapchainsDestroyPending;
        for(auto swapchain : copyOfPendingDestroySwapchains) {
            bool isSubmitted = (FindLayerReferencingSwapchain(swapchain) == nullptr);

            if(!isSubmitted) {
                result = downchain->DestroySwapchain(swapchain);
                if(result != XR_SUCCESS) {
                    ReleaseMutex(gOverlayCallMutex);
                    return result;
                }
                gSwapchainsDestroyPending.erase(swapchain);
            }
        }
        auto copyOfPendingDestroySpaces = gSpacesDestroyPending;
        for(auto space : copyOfPendingDestroySpaces) {
            bool isSubmitted = (FindLayerReferencingSpace(space) == nullptr);

            if(!isSubmitted) {
                result = downchain->DestroySpace(space);
                if(result != XR_SUCCESS) {
                    ReleaseMutex(gOverlayCallMutex);
                    return result;
                }
                gSpacesDestroyPending.erase(space);
            }
        }
    }

    ReleaseMutex(gOverlayCallMutex);
    return result;
}

XrResult XRAPI_CALL Overlay_xrAcquireSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageAcquireInfo*          acquireInfo,
    uint32_t*                                   index)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    result = downchain->AcquireSwapchainImage(swapchain, acquireInfo, index);

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrWaitSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageWaitInfo*             waitInfo)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    result = downchain->WaitSwapchainImage(swapchain, waitInfo);

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrReleaseSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageReleaseInfo*          releaseInfo)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }

    result = downchain->ReleaseSwapchainImage(swapchain, releaseInfo);

    ReleaseMutex(gOverlayCallMutex);

    return result;
}

XrResult Overlay_xrPollEvent(
        XrInstance                                  instance,
        XrEventDataBuffer*                          eventData)
{
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA(fmt("**OVERLAY** timeout waiting at %s:%d on gOverlayCallMutex\n", __FILE__, __LINE__).c_str());
        DebugBreak();
    }
    result = downchain->PollEvent(instance, eventData);

    if(result == XR_SUCCESS) {
        if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {

            // XXX ignoring all chained event data
            const auto* ssc = reinterpret_cast<const XrEventDataSessionStateChanged*>(eventData);
            overlayAppSession.DoMainSessionStateChange(ssc->state, ssc->time);
            if(ssc->next != nullptr) {
                OutputDebugStringA(fmt("**OVERLAY** ignoring a struct chained from a SESSION_STATE_CHANGED Event\n").c_str());
            }

        } else {

            bool queueFull = (gHostEventsSaved.size() == MAX_REMOTE_QUEUE_EVENTS);
            bool queueOneShortOfFull = (gHostEventsSaved.size() == MAX_REMOTE_QUEUE_EVENTS - 1);
            bool backIsEventsLostEvent = (gHostEventsSaved.size() > 0) && (gHostEventsSaved.back()->type == XR_TYPE_EVENT_DATA_EVENTS_LOST);

            bool alreadyLostSomeEvents = queueFull || (queueOneShortOfFull && backIsEventsLostEvent);

            EventDataBufferPtr newEvent(new XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER});
            CopyEventChainIntoBuffer(const_cast<const XrEventDataBaseHeader*>(reinterpret_cast<XrEventDataBaseHeader*>(eventData)), newEvent.get());

            if(newEvent.get()->type != XR_TYPE_EVENT_DATA_BUFFER) {
                // We were able to find some known events in the event pointer chain

                if(alreadyLostSomeEvents) {

                    auto* lost = reinterpret_cast<XrEventDataEventsLost*>(gHostEventsSaved.back().get());
                    lost->lostEventCount ++;

                } else if(queueOneShortOfFull) {

                    EventDataBufferPtr newEvent(new XrEventDataBuffer);
                    XrEventDataEventsLost* lost = reinterpret_cast<XrEventDataEventsLost*>(newEvent.get());
                    lost->type = XR_TYPE_EVENT_DATA_EVENTS_LOST;
                    lost->next = nullptr;
                    lost->lostEventCount = 1;
                    gHostEventsSaved.emplace(std::move(newEvent));
                    OutputDebugStringA(fmt("**OVERLAY** enqueued a Lost Event, length %d\n", gHostEventsSaved.size()).c_str());

                } else {

                    OutputDebugStringA(fmt("**OVERLAY** will enqueue a %d Event\n", newEvent->type).c_str());
                    gHostEventsSaved.emplace(std::move(newEvent));
                    OutputDebugStringA(fmt("**OVERLAY** queue is now size %d\n", gHostEventsSaved.size()).c_str());
                    
                }
            }
        }
    }

    ReleaseMutex(gOverlayCallMutex);
    return result;
}


XrResult Overlay_xrGetSystem(
    XrInstance                                  instance,
    const XrSystemGetInfo*                      getInfo,
    XrSystemId*                                 systemId)
{
    XrResult result = downchain->GetSystem(instance, getInfo, systemId);
    gSavedFormFactor = getInfo->formFactor;
    gSavedSystemId = *systemId;
    return result;
}

XrResult Overlay_xrGetD3D11GraphicsRequirementsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrGraphicsRequirementsD3D11KHR*             graphicsRequirements)
{
   	XrResult result;
    if(!gGetGraphicsRequirementsD3D11KHRWasCalled) {
        auto foo = downchain->GetD3D11GraphicsRequirementsKHR;
        result = foo /* downchain->GetD3D11GraphicsRequirementsKHR */(instance, systemId, graphicsRequirements);
        if (result == XR_SUCCESS) {
            gGraphicsRequirementsD3D11Saved = *graphicsRequirements; // XXX Need deep copy for next chains...
            gGraphicsRequirementsD3D11Saved.next = nullptr;
            gGetGraphicsRequirementsD3D11KHRWasCalled = true;
        }
    } else {
        *graphicsRequirements = gGraphicsRequirementsD3D11Saved; // XXX Need deep copy for next chains...
        result = XR_SUCCESS;
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
  } else if (0 == strcmp(name, "xrBeginSession")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrBeginSession);
  } else if (0 == strcmp(name, "xrRequestExitSession")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrRequestExitSession);
  } else if (0 == strcmp(name, "xrEndSession")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrEndSession);
  } else if (0 == strcmp(name, "xrDestroySession")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrDestroySession);
  } else if (0 == strcmp(name, "xrCreateReferenceSpace")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrCreateReferenceSpace);
  } else if (0 == strcmp(name, "xrEnumerateSwapchainFormats")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrEnumerateSwapchainFormats);
  } else if (0 == strcmp(name, "xrEnumerateSwapchainImages")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrEnumerateSwapchainImages);
  } else if (0 == strcmp(name, "xrGetD3D11GraphicsRequirementsKHR")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrGetD3D11GraphicsRequirementsKHR);
  } else if (0 == strcmp(name, "xrPollEvent")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrPollEvent);
  } else if (0 == strcmp(name, "xrGetSystem")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrGetSystem);
  } else if (0 == strcmp(name, "xrAcquireSwapchainImage")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrAcquireSwapchainImage);
  } else if (0 == strcmp(name, "xrWaitSwapchainImage")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrWaitSwapchainImage);
  } else if (0 == strcmp(name, "xrReleaseSwapchainImage")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrReleaseSwapchainImage);
  } else if (0 == strcmp(name, "xrLocateSpace")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrLocateSpace);
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
