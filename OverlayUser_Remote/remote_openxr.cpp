#define NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <chrono>
#include <thread>

#define XR_USE_GRAPHICS_API_D3D11 1

#include "../XR_overlay_ext/xr_overlay_dll.h"
#include <openxr/openxr_platform.h>

#include <dxgi1_2.h>
#include <d3d11_1.h>

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

#define CHECK_NOT_NULL(a) CheckResultWithLastError(((a) != NULL), #a, __FILE__, __LINE__)

#define CHECK_LAST_ERROR(a) CheckResultWithLastError((a), #a, __FILE__, __LINE__)

#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

struct LocalSession
{
    XrSession       session;
    ID3D11Device*   d3d11;
    LocalSession(XrSession session_, ID3D11Device *d3d11_) :
        session(session_),
        d3d11(d3d11_)
    {}
    LocalSession(const LocalSession& l) :
        session(l.session),
        d3d11(l.d3d11)
    {}
};

typedef std::unique_ptr<LocalSession> LocalSessionPtr;

std::map<XrSession, LocalSessionPtr> gLocalSessionMap;

DWORD gHostProcessId;

struct LocalSwapchain
{
    XrSwapchain             swapchain;
    std::vector<ID3D11Texture2D*> swapchainTextures;
    std::vector<HANDLE>          swapchainHandles;
    std::vector<uint32_t>   acquired;
    bool                    waited;

    LocalSwapchain(XrSwapchain sc, size_t count, ID3D11Device* d3d11, const XrSwapchainCreateInfo* createInfo) :
        swapchain(sc),
        swapchainTextures(count),
        swapchainHandles(count),
        waited(false)
    {
        for(int i = 0; i < count; i++) {
            D3D11_TEXTURE2D_DESC desc;
            desc.Width = createInfo->width;
            desc.Height = createInfo->height;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // static_cast<DXGI_FORMAT>(createInfo->format);
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

            CHECK(d3d11->CreateTexture2D(&desc, NULL, &swapchainTextures[i]));

            {
                IDXGIResource1* sharedResource = NULL;
                CHECK(swapchainTextures[i]->QueryInterface(__uuidof(IDXGIResource1), (LPVOID*) &sharedResource));

                HANDLE thisProcessHandle;
                CHECK_NOT_NULL(thisProcessHandle = GetCurrentProcess());
                HANDLE hostProcessHandle;
                CHECK_NOT_NULL(hostProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, gHostProcessId));

                HANDLE handle;

                CHECK(sharedResource->CreateSharedHandle(NULL,
                    DXGI_SHARED_RESOURCE_READ, // GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                    NULL, &handle));
                
                CHECK_LAST_ERROR(DuplicateHandle(thisProcessHandle, handle, hostProcessHandle, &swapchainHandles[i], 0, TRUE, DUPLICATE_SAME_ACCESS));

                // IDXGIKeyedMutex* keyedMutex;
                // CHECK(swapchainTextures[i]->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex));
                // CHECK(keyedMutex->AcquireSync(KEYED_MUTEX_IPC_REMOTE, INFINITE));
                // CHECK(keyedMutex->ReleaseSync(KEYED_MUTEX_IPC_REMOTE));
            }
        }
    }

    ~LocalSwapchain()
    {
        // Need to acquire back from remote side?
    }
};

typedef std::unique_ptr<LocalSwapchain> LocalSwapchainPtr;

std::map<XrSwapchain, LocalSwapchainPtr> gLocalSwapchainMap;

// MUST BE DEFAULT ONLY FOR OBJECTS WITH NO POINTERS IN THEM
template <typename T>
T* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const T* p)
{
    if(!p)
        return nullptr;

    T* t = new(ipcbuf) T;
    if(!t)
        return nullptr;

    *t = *p;
    return t;
}

// MUST BE DEFAULT ONLY FOR OBJECTS WITH NO POINTERS IN THEM
template <typename T>
T* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const T* p, size_t count)
{
    if(!p)
        return nullptr;

    T* t = new(ipcbuf) T[count];
    if(!t)
        return nullptr;

    for(size_t i = 0; i < count; i++)
        t[i] = p[i];

    return t;
}

// MUST BE DEFAULT ONLY FOR OBJECTS WITH NO POINTERS IN THEM
template <typename T>
T* IPCSerializeNoCopy(IPCBuffer& ipcbuf, IPCXrHeader* header, const T* p)
{
    if(!p)
        return nullptr;

    T* t = new(ipcbuf) T;
    if(!t)
        return nullptr;

    return t;
}

// MUST BE DEFAULT ONLY FOR OBJECTS WITH NO POINTERS IN THEM
template <typename T>
T* IPCSerializeNoCopy(IPCBuffer& ipcbuf, IPCXrHeader* header, const T* p, size_t count)
{
    if(!p)
        return nullptr;

    T* t = new(ipcbuf) T[count];
    if(!t)
        return nullptr;

    return t;
}

// MUST BE DEFAULT ONLY FOR OBJECTS WITH NO POINTERS IN THEM
template <typename T>
void IPCCopyOut(T* dst, const T* src)
{
    if(!src)
        return;

    *dst = *src;
}

// MUST BE DEFAULT ONLY FOR OBJECTS WITH NO POINTERS IN THEM
template <typename T>
void IPCCopyOut(T* dst, const T* src, size_t count)
{
    if(!src)
        return;

    for(size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

template <>
XrBaseInStructure* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const XrBaseInStructure* srcbase)
{
    XrBaseInStructure *dstbase = nullptr;
    bool skipped;

    do {
        skipped = false;

        if(!srcbase) {
            return nullptr;
        }

        switch(srcbase->type) {

            case XR_TYPE_SWAPCHAIN_CREATE_INFO: {
                auto src = reinterpret_cast<const XrSwapchainCreateInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src;
                break;
            }

            case XR_TYPE_FRAME_WAIT_INFO: {
                auto src = reinterpret_cast<const XrFrameWaitInfo*>(srcbase);
                auto dst = new(ipcbuf) XrFrameWaitInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src;
                break;
            }

            case XR_TYPE_FRAME_BEGIN_INFO: {
                auto src = reinterpret_cast<const XrFrameBeginInfo*>(srcbase);
                auto dst = new(ipcbuf) XrFrameBeginInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src;
                break;
            }

            case XR_TYPE_COMPOSITION_LAYER_QUAD: {
                auto src = reinterpret_cast<const XrCompositionLayerQuad*>(srcbase);
                auto dst = new(ipcbuf) XrCompositionLayerQuad;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src;
                break;
            }

            case XR_TYPE_FRAME_END_INFO: {
                auto src = reinterpret_cast<const XrFrameEndInfo*>(srcbase);
                auto dst = new(ipcbuf) XrFrameEndInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                dst->type = src->type;
                dst->displayTime = src->displayTime;
                dst->environmentBlendMode = src->environmentBlendMode;
                dst->layerCount = src->layerCount;

                // Lay down layers...
                // const XrCompositionLayerBaseHeader* const* layers;
                auto layers = new(ipcbuf) XrCompositionLayerBaseHeader*[src->layerCount];
                dst->layers = layers;
                header->addOffsetToPointer(ipcbuf.base, &dst->layers);
                for(uint32_t i = 0; i < dst->layerCount; i++) {
                    layers[i] = reinterpret_cast<XrCompositionLayerBaseHeader*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->layers[i])));
                    header->addOffsetToPointer(ipcbuf.base, &layers[i]);
                }
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO: {
                auto src = reinterpret_cast<const XrSwapchainImageAcquireInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainImageAcquireInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src;
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO: {
                auto src = reinterpret_cast<const XrSwapchainImageWaitInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainImageWaitInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src;
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO: {
                auto src = reinterpret_cast<const XrSwapchainImageReleaseInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainImageReleaseInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src;
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO: {
                auto src = reinterpret_cast<const XrSessionCreateInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSessionCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src; // sloppy, should copy just non-pointers
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT: {
                auto src = reinterpret_cast<const XrSessionCreateInfoOverlayEXT*>(srcbase);
                auto dst = new(ipcbuf) XrSessionCreateInfoOverlayEXT;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src; // sloppy, should copy just non-pointers
                break;
            }

            case XR_TYPE_REFERENCE_SPACE_CREATE_INFO: {
                auto src = reinterpret_cast<const XrReferenceSpaceCreateInfo*>(srcbase);
                auto dst = new(ipcbuf) XrReferenceSpaceCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src; // sloppy, should copy just non-pointers
                break;
            }

            case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
                // We know what this is but do not send it through because we process it locally.
                srcbase = srcbase->next;
                skipped = true;
                break;
            }

            default: {
                // I don't know what this is, skip it and try the next one
                std::string str = fmt("IPCSerialize called on %p of unknown type %d - dropped from \"next\" chain.\n", srcbase, srcbase->type);
                OutputDebugStringA(str.data());
                srcbase = srcbase->next;
                skipped = true;
                break;
            }
        }
    } while(skipped);

    dstbase->next = reinterpret_cast<XrBaseInStructure*>(IPCSerialize(ipcbuf, header, srcbase->next));
    header->addOffsetToPointer(ipcbuf.base, &dstbase->next);

    return dstbase;
}

template <>
void IPCCopyOut(XrBaseOutStructure* dstbase, const XrBaseOutStructure* srcbase)
{
    bool skipped = true;

    while (skipped) {

        if(!srcbase) {
            return;
        }

        switch(dstbase->type) {

#if 0 // SessionCreateInfo is only ever "in" but here for reference.
            case XR_TYPE_SESSION_CREATE_INFO: {
                // XrSessionCreateInfo* src = reinterpret_cast<const XrSessionCreateInfo*>(srcbase);
                // XrSessionCreateInfo* dst = reinterpret_cast<const XrSessionCreateInfo*>(dstbase);
                // Nothing to copy out, proceed to next
                skipped = false;
                break;
            }
#endif

            default: {
                // I don't know what this is, drop it and keep going
                std::string str = fmt("IPCCopyOut called to copy out to %p of unknown type %d - skipped.\n", dstbase, dstbase->type);
                OutputDebugStringA(str.data());

                dstbase = dstbase->next;

                // Don't increment srcbase.  Unknown structs were
                // dropped during serialization, so keep going until we
                // see a type we know and then we'll have caught up with
                // what was serialized.
                //
                break;
            }
        }
    }

    IPCCopyOut(dstbase->next, srcbase->next);
}

template <>
IPCXrEnumerateSwapchainFormats* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrEnumerateSwapchainFormats* src)
{
    auto dst = new(ipcbuf) IPCXrEnumerateSwapchainFormats;

    dst->session = src->session;
    dst->formatCapacityInput = src->formatCapacityInput;
    dst->formatCountOutput = IPCSerializeNoCopy(ipcbuf, header, src->formatCountOutput);
    header->addOffsetToPointer(ipcbuf.base, &dst->formatCountOutput);
    dst->formats = IPCSerializeNoCopy(ipcbuf, header, src->formats, src->formatCapacityInput);
    header->addOffsetToPointer(ipcbuf.base, &dst->formats);

    return dst;
}

template <>
IPCXrCreateSwapchain* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrCreateSwapchain* src)
{
    auto dst = new(ipcbuf) IPCXrCreateSwapchain;

    dst->session = src->session;

    dst->createInfo = reinterpret_cast<const XrSwapchainCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->createInfo);

    dst->swapchain = IPCSerializeNoCopy(ipcbuf, header, src->swapchain);
    header->addOffsetToPointer(ipcbuf.base, &dst->swapchain);

    dst->swapchainCount = IPCSerializeNoCopy(ipcbuf, header, src->swapchainCount);
    header->addOffsetToPointer(ipcbuf.base, &dst->swapchainCount);

    return dst;
}

template <>
IPCXrWaitFrame* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrWaitFrame* src)
{
    auto dst = new(ipcbuf) IPCXrWaitFrame;

    dst->session = src->session;

    dst->frameWaitInfo = reinterpret_cast<const XrFrameWaitInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->frameWaitInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->frameWaitInfo);

    dst->frameState = IPCSerializeNoCopy(ipcbuf, header, src->frameState);
    header->addOffsetToPointer(ipcbuf.base, &dst->frameState);

    return dst;
}

template <>
IPCXrBeginFrame* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrBeginFrame* src)
{
    auto dst = new(ipcbuf) IPCXrBeginFrame;

    dst->session = src->session;

    dst->frameBeginInfo = reinterpret_cast<const XrFrameBeginInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->frameBeginInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->frameBeginInfo);

    return dst;
}

template <>
IPCXrEndFrame* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrEndFrame* src)
{
    auto dst = new(ipcbuf) IPCXrEndFrame;

    dst->session = src->session;

    dst->frameEndInfo = reinterpret_cast<const XrFrameEndInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->frameEndInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->frameEndInfo);

    return dst;
}

template <>
IPCXrAcquireSwapchainImage* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrAcquireSwapchainImage* src)
{
    auto dst = new(ipcbuf) IPCXrAcquireSwapchainImage;

    dst->swapchain = src->swapchain;

    dst->acquireInfo = reinterpret_cast<const XrSwapchainImageAcquireInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->acquireInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->acquireInfo);

    dst->index = IPCSerializeNoCopy(ipcbuf, header, src->index);
    header->addOffsetToPointer(ipcbuf.base, &dst->index);

    return dst;
}

template <>
IPCXrWaitSwapchainImage* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrWaitSwapchainImage* src)
{
    auto dst = new(ipcbuf) IPCXrWaitSwapchainImage;

    dst->swapchain = src->swapchain;

    dst->waitInfo = reinterpret_cast<const XrSwapchainImageWaitInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->waitInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->waitInfo);

    dst->sourceImage = src->sourceImage;

    return dst;
}

template <>
IPCXrReleaseSwapchainImage* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrReleaseSwapchainImage* src)
{
    auto dst = new(ipcbuf) IPCXrReleaseSwapchainImage;

    dst->swapchain = src->swapchain;

    dst->releaseInfo = reinterpret_cast<const XrSwapchainImageReleaseInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->releaseInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->releaseInfo);

    dst->sourceImage = src->sourceImage;

    return dst;
}

template <>
IPCXrDestroySession* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrDestroySession* src)
{
    auto dst = new(ipcbuf) IPCXrDestroySession;

    dst->session = src->session;

    return dst;
}

template <>
void IPCCopyOut(IPCXrEnumerateSwapchainFormats* dst, const IPCXrEnumerateSwapchainFormats* src)
{
    IPCCopyOut(dst->formatCountOutput, src->formatCountOutput);
    if (src->formats) {
        IPCCopyOut(dst->formats, src->formats, *src->formatCountOutput);
    }
}

template <>
IPCXrCreateSession* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrCreateSession* src)
{
    auto dst = new(ipcbuf) IPCXrCreateSession;

    dst->instance = src->instance;

    dst->createInfo = reinterpret_cast<const XrSessionCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->createInfo);

    dst->session = IPCSerializeNoCopy(ipcbuf, header, src->session);
    header->addOffsetToPointer(ipcbuf.base, &dst->session);

    return dst;
}

template <>
void IPCCopyOut(IPCXrCreateSession* dst, const IPCXrCreateSession* src)
{
    IPCCopyOut(dst->session, src->session);
}


template <>
IPCXrCreateReferenceSpace* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrCreateReferenceSpace* src)
{
    auto dst = new(ipcbuf) IPCXrCreateReferenceSpace;

    dst->session = src->session;

    dst->createInfo = reinterpret_cast<const XrReferenceSpaceCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->createInfo);

    dst->space = IPCSerializeNoCopy(ipcbuf, header, src->space);
    header->addOffsetToPointer(ipcbuf.base, &dst->space);

    return dst;
}

template <>
void IPCCopyOut(IPCXrCreateReferenceSpace* dst, const IPCXrCreateReferenceSpace* src)
{
    IPCCopyOut(dst->space, src->space);
}

template <>
IPCXrHandshake* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrHandshake* src)
{
    IPCXrHandshake *dst = new(ipcbuf) IPCXrHandshake;

    dst->remoteProcessId = src->remoteProcessId;
    // TODO don't bother copying instance in here because out only
    dst->instance = IPCSerializeNoCopy(ipcbuf, header, src->instance);
    header->addOffsetToPointer(ipcbuf.base, &dst->instance);
    dst->systemId = IPCSerializeNoCopy(ipcbuf, header, src->systemId);
    header->addOffsetToPointer(ipcbuf.base, &dst->systemId);
    dst->adapterLUID = IPCSerializeNoCopy(ipcbuf, header, src->adapterLUID);
    header->addOffsetToPointer(ipcbuf.base, &dst->adapterLUID);
    dst->hostProcessId = IPCSerializeNoCopy(ipcbuf, header, src->hostProcessId);
    header->addOffsetToPointer(ipcbuf.base, &dst->hostProcessId);

    return dst;
}

template <>
void IPCCopyOut(IPCXrHandshake* dst, const IPCXrHandshake* src)
{
    IPCCopyOut(dst->instance, src->instance);
    IPCCopyOut(dst->systemId, src->systemId);
    IPCCopyOut(dst->adapterLUID, src->adapterLUID);
    IPCCopyOut(dst->hostProcessId, src->hostProcessId);
}

XrResult ipcxrHandshake(
    XrInstance *instance,
    XrSystemId *systemId,
    LUID *luid,
    DWORD *hostProcessId)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    auto header = new(ipcbuf) IPCXrHeader{IPC_HANDSHAKE};

    IPCXrHandshake args {GetCurrentProcessId(), instance, systemId, luid, hostProcessId};
    IPCXrHandshake *argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    gHostProcessId = *hostProcessId;

    return header->result;
}

XrResult xrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session)
{
    const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
    const XrGraphicsBindingD3D11KHR* d3dbinding = nullptr;
    while(p != nullptr) {
        if(p->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
            d3dbinding = reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(p);
        }
        p = reinterpret_cast<const XrBaseInStructure*>(p->next);
    }

    if(!d3dbinding) {
        return XR_ERROR_GRAPHICS_DEVICE_INVALID; // ?
    }

    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_CREATE_SESSION};

    IPCXrCreateSession args {instance, createInfo, session};

    IPCXrCreateSession* argsSerialized = IPCSerialize(ipcbuf, header, &args);
    header->makePointersRelative(ipcbuf.base);

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    if(header->result == XR_SUCCESS) {
        gLocalSessionMap[*session] = LocalSessionPtr(new LocalSession(*session, d3dbinding->device));
    }

    return header->result;
}

XrResult xrCreateReferenceSpace(
    XrSession                                   session,
    const XrReferenceSpaceCreateInfo*           createInfo,
    XrSpace*                                    space)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_CREATE_REFERENCE_SPACE};

    IPCXrCreateReferenceSpace args {session, createInfo, space};

    IPCXrCreateReferenceSpace* argsSerialized = IPCSerialize(ipcbuf, header, &args);
    header->makePointersRelative(ipcbuf.base);

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

XrResult xrEnumerateSwapchainFormats(
    XrSession                                   session,
    uint32_t                                    formatCapacityInput,
    uint32_t*                                   formatCountOutput,
    int64_t*                                    formats)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_ENUMERATE_SWAPCHAIN_FORMATS};

    IPCXrEnumerateSwapchainFormats args {session, formatCapacityInput, formatCountOutput, formats};

    IPCXrEnumerateSwapchainFormats* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    // TODO intersect with {RGB,RGBA,BGRA,BGR}8

    return header->result;
}

template <>
void IPCCopyOut(IPCXrCreateSwapchain* dst, const IPCXrCreateSwapchain* src)
{
    IPCCopyOut(dst->swapchain, src->swapchain);
    IPCCopyOut(dst->swapchainCount, src->swapchainCount);
}

template <>
void IPCCopyOut(IPCXrAcquireSwapchainImage* dst, const IPCXrAcquireSwapchainImage* src)
{
    IPCCopyOut(dst->index, src->index);
}

template <>
void IPCCopyOut(IPCXrWaitFrame* dst, const IPCXrWaitFrame* src)
{
    IPCCopyOut(dst->frameState, src->frameState);
}


XrResult xrCreateSwapchain(
    XrSession                                   session,
    const XrSwapchainCreateInfo*                createInfo,
    XrSwapchain*                                swapchain)
{
    if(createInfo->sampleCount != 1) {
        return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
    }
    if(createInfo->mipCount != 1) {
        return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED; 
    }
    if(createInfo->arraySize != 1) {
        return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED; 
    }
    if((createInfo->usageFlags & ~(XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT)) != 0) {
        return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED; 
    }
    if(createInfo->createFlags != 0) {
        return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED; 
    }

    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_CREATE_SWAPCHAIN};

    int32_t swapchainCount;    
    IPCXrCreateSwapchain args {session, createInfo, swapchain, &swapchainCount};

    IPCXrCreateSwapchain* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    if(header->result == XR_SUCCESS) {

        auto& localSession = gLocalSessionMap[session];

        gLocalSwapchainMap[*swapchain] = LocalSwapchainPtr(new LocalSwapchain(*swapchain, swapchainCount, localSession->d3d11, createInfo));
    }

    return header->result;
}

XrResult xrBeginFrame(
    XrSession                                   session,
    const XrFrameBeginInfo*                     frameBeginInfo)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_BEGIN_FRAME};

    IPCXrBeginFrame args {session, frameBeginInfo};

    IPCXrBeginFrame* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized);

    return header->result;
}

XrResult xrWaitFrame(
    XrSession                                   session,
    const XrFrameWaitInfo*                      frameWaitInfo,
    XrFrameState*                               frameState)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_WAIT_FRAME};

    IPCXrWaitFrame args {session, frameWaitInfo, frameState};

    IPCXrWaitFrame* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

XrResult xrEndFrame(
    XrSession                                  session,
    const XrFrameEndInfo*                      frameEndInfo)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_END_FRAME};

    IPCXrEndFrame args {session, frameEndInfo};

    IPCXrEndFrame* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized);

    return header->result;
}

XrResult xrAcquireSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageAcquireInfo*          acquireInfo,
    uint32_t*                                   index)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_ACQUIRE_SWAPCHAIN_IMAGE};

    IPCXrAcquireSwapchainImage args {swapchain, acquireInfo, index};

    IPCXrAcquireSwapchainImage* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    gLocalSwapchainMap[swapchain]->acquired.push_back(*index);

    return header->result;
}

XrResult xrWaitSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageWaitInfo*             waitInfo)
{
    auto& localSwapchain = gLocalSwapchainMap[swapchain];
    if(localSwapchain->waited) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_WAIT_SWAPCHAIN_IMAGE};

    uint32_t wasWaited = localSwapchain->acquired[0];
    HANDLE sharedResourceHandle = localSwapchain->swapchainHandles[wasWaited];
    IPCXrWaitSwapchainImage args {swapchain, waitInfo, sharedResourceHandle};

    IPCXrWaitSwapchainImage* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized);

    localSwapchain->waited = true;
    IDXGIKeyedMutex* keyedMutex;
    CHECK(localSwapchain->swapchainTextures[wasWaited]->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex));
    CHECK(keyedMutex->AcquireSync(KEYED_MUTEX_IPC_REMOTE, INFINITE));
    // keyedMutex->Release();

    return header->result;
}

XrResult xrReleaseSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageReleaseInfo*             waitInfo)
{
    if(!gLocalSwapchainMap[swapchain]->waited)
        return XR_ERROR_CALL_ORDER_INVALID;

    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_RELEASE_SWAPCHAIN_IMAGE};

    uint32_t beingReleased = gLocalSwapchainMap[swapchain]->acquired[0];

    auto& localSwapchain = gLocalSwapchainMap[swapchain];

    localSwapchain->acquired.erase(localSwapchain->acquired.begin());

    IDXGIKeyedMutex* keyedMutex;
    CHECK(localSwapchain->swapchainTextures[beingReleased]->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex));
    CHECK(keyedMutex->ReleaseSync(KEYED_MUTEX_IPC_HOST));

    HANDLE sharedResourceHandle = localSwapchain->swapchainHandles[beingReleased];
    IPCXrReleaseSwapchainImage args {swapchain, waitInfo, sharedResourceHandle};

    IPCXrReleaseSwapchainImage* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized);

    gLocalSwapchainMap[swapchain]->waited = false;

    return header->result;
}

XrResult xrEnumerateSwapchainImages(
        XrSwapchain swapchain,
        uint32_t imageCapacityInput,
        uint32_t* imageCountOutput,
        XrSwapchainImageBaseHeader* images)
{
    auto& localSwapchain = gLocalSwapchainMap[swapchain];

    if(imageCapacityInput == 0) {
        *imageCountOutput = (uint32_t)localSwapchain->swapchainTextures.size();
        return XR_SUCCESS;
    }

    auto sci = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
    uint32_t toWrite = std::min(imageCapacityInput, (uint32_t)localSwapchain->swapchainTextures.size());
    for(uint32_t i = 0; i < toWrite; i++) {
        if(sci[i].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
            // XXX TODO something smart here - validation failure only?
            DebugBreak();
        } else {
            sci[i].texture = localSwapchain->swapchainTextures[i];
            // ignore next since we don't understand anything else
        }
    }

    *imageCountOutput = toWrite;

    return XR_SUCCESS;
}

XrResult xrDestroySession(
    XrSession                                 session)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_DESTROY_SESSION};

    IPCXrDestroySession args {session};

    IPCXrDestroySession* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    if(header->result == XR_SUCCESS) {
        gLocalSessionMap.erase(session);
    }

    return header->result;
}

