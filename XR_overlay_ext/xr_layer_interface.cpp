#include <memory>
#include <chrono>
#include <string>

#include <cassert>
#include <cstring>
#include <cstdio>

#define XR_USE_GRAPHICS_API_D3D11 1

#include "xr_overlay_dll.h"
#include "xr_generated_dispatch_table.h"
#include "xr_linear.h"

#include <d3d11_4.h>

const char *kOverlayLayerName = "XR_EXT_overlay_api_layer";
DWORD gOverlayWorkerThreadId;
HANDLE gOverlayWorkerThread;
LPCSTR kOverlayCreateSessionSemaName = "XR_EXT_overlay_overlay_create_session_sema";
HANDLE gOverlayCreateSessionSema;
LPCSTR kOverlayWaitFrameSemaName = "XR_EXT_overlay_overlay_wait_frame_sema";
HANDLE gOverlayWaitFrameSema;
LPCSTR kMainDestroySessionSemaName = "XR_EXT_overlay_main_destroy_session_sema";
HANDLE gMainDestroySessionSema;

ID3D11Device *gSavedD3DDevice;
XrInstance gSavedInstance;
unsigned int overlaySessionStandin;
XrSession kOverlayFakeSession = reinterpret_cast<XrSession>(&overlaySessionStandin);
XrSession gSavedMainSession;
XrSession gOverlaySession;
bool gExitOverlay = false;
bool gSerializeEverything = true;

const uint64_t ONE_SECOND_IN_NANOSECONDS = 1000000000;

enum { MAX_OVERLAY_LAYER_COUNT = 2 };

XrFrameState gSavedWaitFrameState;

HANDLE gOverlayCallMutex = NULL;      // handle to sync object
LPCWSTR kOverlayMutexName = TEXT("XR_EXT_overlay_call_mutex");

namespace Math {
namespace Pose {
XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

XrPosef Translation(const XrVector3f& translation) {
    XrPosef t = Identity();
    t.position = translation;
    return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
    XrPosef t = Identity();
    t.orientation.x = 0.f;
    t.orientation.y = std::sin(radians * 0.5f);
    t.orientation.z = 0.f;
    t.orientation.w = std::cos(radians * 0.5f);
    t.position = translation;
    return t;
}
}  // namespace Pose
}  // namespace Math

static XrGeneratedDispatchTable *downchain = nullptr;

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
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        std::string str = fmt("%s at %s:%d failed with %d (%s)\n", what, file, line, lastError, messageBuf);
        OutputDebugStringA(str.data());
        DebugBreak();
        LocalFree(messageBuf);
    }
}

#define CHECK_NOT_NULL(a) CheckResultWithLastError(((a) != NULL), #a, __FILE__, __LINE__)

#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

static void CheckXrResult(XrResult a, const char* what, const char *file, int line)
{
    if(a != XR_SUCCESS) {
        std::string str = fmt("%s at %s:%d failed with %d\n", what, file, line, a);
        OutputDebugStringA(str.data());
        DebugBreak();
    }
}

const void* unpackXrStruct(unsigned char*& ptr);

template <>
XrSessionCreateInfo* unpack<XrSessionCreateInfo*>(unsigned char*& ptr)
{
	// type already unpacked
	// XXX MUSTFIX memory leak - add allocator to unpacker or put directly in shmem
	XrSessionCreateInfo *tmp = new XrSessionCreateInfo;
	tmp->type = XR_TYPE_SESSION_CREATE_INFO;
	tmp->next = unpackXrStruct(ptr);
	tmp->createFlags = unpack<XrSessionCreateFlags>(ptr);
	tmp->systemId = unpack<XrSystemId>(ptr);
	return tmp;
}

template <>
XrSessionCreateInfoOverlayEXT* unpack<XrSessionCreateInfoOverlayEXT*>(unsigned char*& ptr)
{
	// type already unpacked
	// XXX MUSTFIX memory leak - add allocator to unpacker or put directly in shmem
	XrSessionCreateInfoOverlayEXT *tmp = new XrSessionCreateInfoOverlayEXT;
	tmp->type = (XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT;
	tmp->next = unpackXrStruct(ptr);
	tmp->overlaySession = unpack<XrBool32>(ptr);
	tmp->sessionLayersPlacement = unpack<uint32_t>(ptr);
	return tmp;
}

const void* unpackXrStruct(unsigned char*& ptr)
{
	bool notnull = unpack<bool>(ptr);
	if (!notnull)
		return nullptr;

	XrStructureType type = unpack<XrStructureType>(ptr);
	const void* p;
	switch (type) {
	case XR_TYPE_SESSION_CREATE_INFO:
		p = unpack<XrSessionCreateInfo*>(ptr);
		break;
	case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT:
		p = unpack<XrSessionCreateInfoOverlayEXT*>(ptr);
		break;
	default:
		// I don't know what this is
		std::string str = fmt("packXrStruct called to pack unknown type %d - dropped from \"next\" chain.\n", type);
		OutputDebugStringA(str.data());
		abort();
		break;
	}
	return p;
}

template <>
IPCXrCreateSessionIn unpack<IPCXrCreateSessionIn>(unsigned char*& ptr)
{
	IPCXrCreateSessionIn tmp;
	tmp.instance = unpack<XrInstance>(ptr);
	tmp.createInfo = reinterpret_cast<const XrSessionCreateInfo *>(unpackXrStruct(ptr));
	return tmp;
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

extern "C" {
extern int Image2Width;
extern int Image2Height;
extern unsigned char Image2Bytes[];
extern int Image1Width;
extern int Image1Height;
extern unsigned char Image1Bytes[];
};


DWORD WINAPI ThreadBody(LPVOID)
{
    void *shmem = IPCGetSharedMemory();

    bool continueIPC = true;
    do {
        IPCWaitForGuestRequest(); // XXX TODO check response
        OutputDebugStringA("**OVERLAY** success in waiting for guest request\n");

        unsigned char* unpackPtr = reinterpret_cast<unsigned char*>(shmem);
        uint64_t requestType = unpack<uint64_t>(unpackPtr);

        unsigned char* packPtr = reinterpret_cast<unsigned char*>(shmem);

        switch(requestType) {

            case IPC_XR_CREATE_SESSION: {
                IPCXrCreateSessionIn args = unpack<IPCXrCreateSessionIn>(unpackPtr);
                XrSession session;
                XrResult result = Overlay_xrCreateSession(args.instance, args.createInfo, &session);
                pack(packPtr, (XrResult)result);
                pack(packPtr, session);
                IPCFinishHostResponse();
                continueIPC = false; // XXX testing initial handoff, normally will remain in this loop until remote terminates
                break;
            }

            case IPC_REQUEST_HANDOFF:
                // Establish IPC parameters and make initial handoff
                pack(packPtr, (XrResult)XR_SUCCESS);
                pack(packPtr, gSavedInstance);
                IPCFinishHostResponse();
                break;

            default:
                OutputDebugStringA("unknown request type in IPC");
                abort();
                break;
        }

    } while(continueIPC);
    OutputDebugStringA("**OVERLAY** exited IPC loop\n");

    // gOverlaySession was saved off when we proxied the IPC call to CreateSession

    // Don't have gSavedD3DDevice until after CreateSession

    // XXX TODO use multiple Devices to avoid having to synchronize
    ID3D11Multithread* d3dMultithread;
    CHECK(gSavedD3DDevice->QueryInterface(__uuidof(ID3D11Multithread), reinterpret_cast<void**>(&d3dMultithread)));
    d3dMultithread->SetMultithreadProtected(TRUE);

    ID3D11DeviceContext* d3dContext;
    gSavedD3DDevice->GetImmediateContext(&d3dContext);

    ID3D11Texture2D* sourceImages[2];
    for(int i = 0; i < 2; i++) {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = 512;
        desc.Height = 512;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN ;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        CHECK(gSavedD3DDevice->CreateTexture2D(&desc, NULL, &sourceImages[i]));

        D3D11_MAPPED_SUBRESOURCE mapped;
        CHECK(d3dContext->Map(sourceImages[i], 0, D3D11_MAP_WRITE, 0, &mapped));
        if(i == 0)
            memcpy(mapped.pData, Image2Bytes, 512 * 512 * 4);
        else
            memcpy(mapped.pData, Image1Bytes, 512 * 512 * 4);
        d3dContext->Unmap(sourceImages[i], 0);
    }

    XrSpace viewSpace;
    XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    createSpaceInfo.next = nullptr;
    createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    // Render head-locked 1.5m in front of device.
    createSpaceInfo.poseInReferenceSpace = Math::Pose::Translation({-1.0f, 0.5f, -1.5f});
    CHECK_XR(Overlay_xrCreateReferenceSpace(gOverlaySession, &createSpaceInfo, &viewSpace));

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
        swapchainCreateInfo.next = nullptr;
        CHECK_XR(Overlay_xrCreateSwapchain(gOverlaySession, &swapchainCreateInfo, &swapchains[eye]));

        uint32_t count;
        CHECK_XR(Overlay_xrEnumerateSwapchainImages(swapchains[eye], 0, &count, nullptr));
        swapchainImages[eye] = new XrSwapchainImageD3D11KHR[count];
        for(uint32_t i = 0; i < count; i++) {
            swapchainImages[eye][i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            swapchainImages[eye][i].next = nullptr;
        }
        CHECK_XR(Overlay_xrEnumerateSwapchainImages(swapchains[eye], count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages[eye])));
    }
    OutputDebugStringA("**OVERLAY** success in thread creating swapchain\n");

    int whichImage = 0;
    auto then = std::chrono::steady_clock::now();
    while(!gExitOverlay) {
        auto now = std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count() > 1000) {
            whichImage = (whichImage + 1) % 2;
            then = std::chrono::steady_clock::now();
        }

        XrFrameState state;
        Overlay_xrWaitFrame(gOverlaySession, nullptr, &state);
        OutputDebugStringA("**OVERLAY** exited overlay session xrWaitFrame\n");
        Overlay_xrBeginFrame(gOverlaySession, nullptr);
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

            d3dContext->CopyResource(swapchainImages[eye][index].texture, sourceImages[whichImage]);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            releaseInfo.next = nullptr;
            CHECK_XR(downchain->ReleaseSwapchainImage(sc, &releaseInfo));
        }
        XrCompositionLayerQuad layers[2];
        XrCompositionLayerBaseHeader* layerPointers[2];
        for(uint32_t eye = 0; eye < 2; eye++) {
            XrSwapchainSubImage fullImage = {swapchains[eye], {{0, 0}, {512, 512}}, 0};
            layerPointers[eye] = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layers[eye]);
            layers[eye].type = XR_TYPE_COMPOSITION_LAYER_QUAD;
            layers[eye].next = nullptr;
            layers[eye].layerFlags = 0;
            layers[eye].space = viewSpace;
            layers[eye].eyeVisibility = (eye == 0) ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            layers[eye].subImage = fullImage;
            layers[eye].pose = Math::Pose::Identity();
            layers[eye].size = {0.33f, 0.33f};
        }
        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.next = nullptr;
        frameEndInfo.layers = layerPointers;
        frameEndInfo.layerCount = 2;
        frameEndInfo.displayTime = gSavedWaitFrameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE; // XXX ignored
        Overlay_xrEndFrame(gOverlaySession, &frameEndInfo);
    }

    CHECK_XR(Overlay_xrDestroySession(gOverlaySession));

    OutputDebugStringA("**OVERLAY** destroyed session, exiting\n");
    return 0;
}

void CreateOverlaySessionThread()
{
    CHECK_NOT_NULL(gOverlayCreateSessionSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayCreateSessionSemaName));
    CHECK_NOT_NULL(gOverlayWaitFrameSema =
        CreateSemaphoreA(nullptr, 0, 1, kOverlayWaitFrameSemaName));
    CHECK_NOT_NULL(gMainDestroySessionSema =
        CreateSemaphoreA(nullptr, 0, 1, kMainDestroySessionSemaName));
    CHECK_NOT_NULL(gOverlayCallMutex = CreateMutex(nullptr, TRUE, kOverlayMutexName));
    ReleaseMutex(gOverlayCallMutex);

    CHECK_NOT_NULL(gOverlayWorkerThread =
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
    // TODO : should remove for main session, should return only max overlay layers for overlay session
    properties->graphicsProperties.maxLayerCount =
        properties->graphicsProperties.maxLayerCount - MAX_OVERLAY_LAYER_COUNT;

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

    const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
    const XrSessionCreateInfoOverlayEXT* cio = nullptr;
    const XrGraphicsBindingD3D11KHR* d3dbinding = nullptr;
    while(p != nullptr) {
        if(p->type == XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT) {
            cio = reinterpret_cast<const XrSessionCreateInfoOverlayEXT*>(p);
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

        gSavedMainSession = *session;
        gSavedD3DDevice = d3dbinding->device;

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
        gOverlaySession = *session; // XXX as loop is transferred to IPC
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

XrResult Overlay_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
{ 
    if(gSerializeEverything) {
        DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
        if(waitresult == WAIT_TIMEOUT) {
            OutputDebugStringA("**OVERLAY** timeout waiting in Overlay_xrCreateReferenceSpace on gOverlayCallMutex\n");
            DebugBreak();
        }
    }

    if(session == kOverlayFakeSession) {
        session = gSavedMainSession;
    }

    XrResult result = downchain->CreateReferenceSpace(session, createInfo, space);

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

uint32_t gOverlayQuadLayerCount = 0;
XrCompositionLayerQuad gOverlayQuadLayers[MAX_OVERLAY_LAYER_COUNT];

XrResult Overlay_xrEndFrame(XrSession session, const XrFrameEndInfo *info) 
{ 
    XrResult result;

    DWORD waitresult = WaitForSingleObject(gOverlayCallMutex, INFINITE);
    if(waitresult == WAIT_TIMEOUT) {
        OutputDebugStringA("**OVERLAY** timeout waiting in CreateSwapchain on gOverlayCallMutex\n");
        DebugBreak();
    }

    if(session == kOverlayFakeSession) {

        // TODO: validate blend mode matches main session

        if(info->layerCount > MAX_OVERLAY_LAYER_COUNT) {
            gOverlayQuadLayerCount = 0;
            result = XR_ERROR_LAYER_LIMIT_EXCEEDED;
        } else {
            bool valid = true;
            for(uint32_t i = 0; i < info->layerCount; i++) {
                if(info->layers[0]->type != XR_TYPE_COMPOSITION_LAYER_QUAD) {
                    result = XR_ERROR_LAYER_INVALID;
                    valid = false;
                    break;
                }
            }
            if(valid) {
                gOverlayQuadLayerCount = info->layerCount;
                for(uint32_t i = 0; i < info->layerCount; i++) {
                    gOverlayQuadLayers[i] = *reinterpret_cast<const XrCompositionLayerQuad*>(info->layers[i]);
                }
                result = XR_SUCCESS;
            } else {
                gOverlayQuadLayerCount = 0;
            }
        }

    } else {

        XrFrameEndInfo info2 = *info;
        std::unique_ptr<const XrCompositionLayerBaseHeader*> layers2(new const XrCompositionLayerBaseHeader*[info->layerCount + gOverlayQuadLayerCount]);
        memcpy(layers2.get(), info->layers, sizeof(const XrCompositionLayerBaseHeader*) * info->layerCount);
        for(uint32_t i = 0; i < gOverlayQuadLayerCount; i++)
            layers2.get()[info->layerCount + i] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&gOverlayQuadLayers);

        info2.layerCount = info->layerCount + gOverlayQuadLayerCount;
        info2.layers = layers2.get();
        result = downchain->EndFrame(session, &info2);
    }

    ReleaseMutex(gOverlayCallMutex);
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
  } else if (0 == strcmp(name, "xrCreateReferenceSpace")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(Overlay_xrCreateReferenceSpace);
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
