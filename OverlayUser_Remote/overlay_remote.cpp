// Remote.cpp 

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

extern "C" {
	extern int Image2Width;
	extern int Image2Height;
	extern unsigned char Image2Bytes[];
	extern int Image1Width;
	extern int Image1Height;
	extern unsigned char Image1Bytes[];
};


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

static void CheckXrResult(XrResult a, const char* what, const char *file, int line)
{
    if(a != XR_SUCCESS) {
        std::string str = fmt("%s at %s:%d failed with %d\n", what, file, line, a);
        OutputDebugStringA(str.data());
        DebugBreak();
    }
}

#define CHECK_NOT_NULL(a) CheckResultWithLastError(((a) != NULL), #a, __FILE__, __LINE__)

#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

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

struct LocalSwapchain
{
    XrSwapchain             swapchain;
    size_t                  swapchainTextureCount;
    int                     nextSwapchain; 
    std::vector<ID3D11Texture2D*> swapchainTextures;
    std::vector<HANDLE>          swapchainHandles;
    std::vector<IDXGIKeyedMutex*>          swapchainKeyedMutexes;
    std::vector<uint32_t>   acquired;
    bool                    waited;

    LocalSwapchain(XrSwapchain sc, size_t count, ID3D11Device* d3d11, const XrSwapchainCreateInfo* createInfo) :
        swapchain(sc),
        nextSwapchain(0),
        swapchainTextures(count),
        swapchainHandles(count),
        waited(false)
    {
        for(int i = 0; i < count; i++) {
            D3D11_TEXTURE2D_DESC desc;
            desc.Width = createInfo->width;
            desc.Height = createInfo->height;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN ;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

            CHECK(d3d11->CreateTexture2D(&desc, NULL, &swapchainTextures[i]));
            IDXGIResource1* pDXGIResource = NULL;
            CHECK(swapchainTextures[i]->QueryInterface(__uuidof(IDXGIResource1), (LPVOID*) &pDXGIResource));
            CHECK(pDXGIResource->CreateSharedHandle(NULL, DXGI_SHARED_RESOURCE_READ, NULL, &swapchainHandles[i]));
            CHECK(swapchainTextures[i]->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&swapchainKeyedMutexes[i]));
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
    bool skipped = true;

    do {

        if(!srcbase) {
            return nullptr;
        }

        switch(srcbase->type) {

            case XR_TYPE_SESSION_CREATE_INFO: {
                const XrSessionCreateInfo* src = reinterpret_cast<const XrSessionCreateInfo*>(srcbase);
                XrSessionCreateInfo* dst = new(ipcbuf) XrSessionCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src; // sloppy, should copy just non-pointers
                skipped = false;
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT: {
                const XrSessionCreateInfoOverlayEXT* src = reinterpret_cast<const XrSessionCreateInfoOverlayEXT*>(srcbase);
                XrSessionCreateInfoOverlayEXT* dst = new(ipcbuf) XrSessionCreateInfoOverlayEXT;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src; // sloppy, should copy just non-pointers
                skipped = false;
                break;
            }

            case XR_TYPE_REFERENCE_SPACE_CREATE_INFO: {
                auto src = reinterpret_cast<const XrReferenceSpaceCreateInfo*>(srcbase);
                auto dst = new(ipcbuf) XrReferenceSpaceCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                *dst = *src; // sloppy, should copy just non-pointers
                skipped = false;
                break;
            }

            case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
                // We know what this is but do not send it through.  Skip it.
                srcbase = srcbase->next;
                break;
            }

            default: {
                // I don't know what this is, skip it and try the next one
                std::string str = fmt("IPCSerialize called on %p of unknown type %d - dropped from \"next\" chain.\n", srcbase, srcbase->type);
                OutputDebugStringA(str.data());
                srcbase = srcbase->next;
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

    // TODO don't bother copying instance in here because out only
    dst->instance = IPCSerializeNoCopy(ipcbuf, header, src->instance);
    header->addOffsetToPointer(ipcbuf.base, &dst->instance);

    return dst;
}

template <>
void IPCCopyOut(IPCXrHandshake* dst, const IPCXrHandshake* src)
{
    IPCCopyOut(dst->instance, src->instance);
}

XrResult ipcxrHandshake(
    XrInstance *instance)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    auto header = new(ipcbuf) IPCXrHeader{IPC_HANDSHAKE};

    IPCXrHandshake args {instance};
    IPCXrHandshake *argsSerialized = IPCSerialize(ipcbuf, header, &args);

#if 0
    printf("buf before Remote makePointersRelative: ");
    for(int i = 0; i < 32; i++)
        printf("%02X ", ((unsigned char*)argsSerialized)[i]);
    puts("");
#endif
    header->makePointersRelative(ipcbuf.base);

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

XrResult ipcxrCreateSession(
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
        OutputDebugStringA("CreateSession called with no D3D11Device\n");
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

XrResult ipcxrCreateReferenceSpace(
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

XrResult ipcxrEnumerateSwapchainFormats(
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

XrResult ipcxrCreateSwapchain(
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

XrResult ipcxrBeginFrame(
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

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

XrResult ipcxrWaitFrame(
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

XrResult ipcxrEndFrame(
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

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

XrResult ipcxrAcquireSwapchainImage(
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

XrResult ipcxrWaitSwapchainImage(
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

    IPCCopyOut(&args, argsSerialized);

    localSwapchain->waited = true;
    localSwapchain->swapchainKeyedMutexes[wasWaited]->AcquireSync(0, INFINITE);

    return header->result;
}

XrResult ipcxrReleaseSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageReleaseInfo*             waitInfo)
{
    if(!gLocalSwapchainMap[swapchain]->waited)
        return XR_ERROR_CALL_ORDER_INVALID;

    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_RELEASE_SWAPCHAIN_IMAGE};

    uint32_t beingReleased = gLocalSwapchainMap[swapchain]->acquired[0];
	auto& localSwapchain = gLocalSwapchainMap[swapchain];
	localSwapchain->acquired.erase(gLocalSwapchainMap[swapchain]->acquired.begin());
    localSwapchain->swapchainKeyedMutexes[beingReleased]->ReleaseSync(1);
    HANDLE sharedResourceHandle = localSwapchain->swapchainHandles[beingReleased];
    IPCXrReleaseSwapchainImage args {swapchain, waitInfo, sharedResourceHandle};

    IPCXrReleaseSwapchainImage* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    gLocalSwapchainMap[swapchain]->waited = false;

    return header->result;
}

XrResult ipcxrEnumerateSwapchainImages(
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

XrResult ipcxrDestroySession(
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

const uint64_t ONE_SECOND_IN_NANOSECONDS = 1000000000;

int main( void )
{
    //DebugBreak();

    // WCHAR cBuf[MAX_PATH];
    // GetSharedMem(cBuf, MAX_PATH);
    // printf("Child process read from shared memory: %S\n", cBuf);

    XrInstance instance;
    CHECK_XR(ipcxrHandshake(&instance));

    ID3D11Device* d3d11Device;
    CHECK(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
        D3D11_SDK_VERSION, &d3d11Device, NULL, NULL));

    ID3D11DeviceContext* d3dContext;
	d3d11Device->GetImmediateContext(&d3dContext);

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

        CHECK(d3d11Device->CreateTexture2D(&desc, NULL, &sourceImages[i]));

        D3D11_MAPPED_SUBRESOURCE mapped;
        CHECK(d3dContext->Map(sourceImages[i], 0, D3D11_MAP_WRITE, 0, &mapped));
        if(i == 0)
            memcpy(mapped.pData, Image2Bytes, 512 * 512 * 4);
        else
            memcpy(mapped.pData, Image1Bytes, 512 * 512 * 4);
        d3dContext->Unmap(sourceImages[i], 0);
    }

    XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
    sessionCreateInfoOverlay.next = nullptr;
    sessionCreateInfoOverlay.overlaySession = XR_TRUE;
    sessionCreateInfoOverlay.sessionLayersPlacement = 1;

    XrGraphicsBindingD3D11KHR d3dBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    d3dBinding.device = d3d11Device;
    d3dBinding.next = &sessionCreateInfoOverlay;

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &d3dBinding;

    XrSession session;
    CHECK_XR(ipcxrCreateSession(instance, &sessionCreateInfo, &session));

    XrSpace viewSpace;
    XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    createSpaceInfo.next = nullptr;
    createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    // Render head-locked 1.5m in front of device.
    createSpaceInfo.poseInReferenceSpace = Math::Pose::Translation({-1.0f, 0.5f, -1.5f});
    CHECK_XR(ipcxrCreateReferenceSpace(session, &createSpaceInfo, &viewSpace));

    uint64_t chosenFormat;
    {
        uint32_t count;
        CHECK_XR(ipcxrEnumerateSwapchainFormats(session, 0, &count, nullptr));
        std::vector<int64_t> runtimeFormats(count);
        CHECK_XR(ipcxrEnumerateSwapchainFormats(session, (uint32_t)count, &count, runtimeFormats.data()));
        std::vector<DXGI_FORMAT> appFormats { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
        auto formatFound = std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), appFormats.begin(), appFormats.end());
        if(formatFound == runtimeFormats.end()) {
            OutputDebugStringA("No supported swapchain format found\n");
            // XXX Do something smarter here
            DebugBreak();
        }
        chosenFormat = *formatFound;
        printf("%d formats returned, chosen format is %lld\n", count, chosenFormat);
    }

    XrSwapchain swapchains[2];
    XrSwapchainImageD3D11KHR *swapchainImages[2];
    for(int eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = chosenFormat;
        swapchainCreateInfo.width = 512;
        swapchainCreateInfo.height = 512;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = 1;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.next = nullptr;
        CHECK_XR(ipcxrCreateSwapchain(session, &swapchainCreateInfo, &swapchains[eye]));

        uint32_t count;
        CHECK_XR(ipcxrEnumerateSwapchainImages(swapchains[eye], 0, &count, nullptr));
        swapchainImages[eye] = new XrSwapchainImageD3D11KHR[count];
        for(uint32_t i = 0; i < count; i++) {
            swapchainImages[eye][i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            swapchainImages[eye][i].next = nullptr;
        }
        CHECK_XR(ipcxrEnumerateSwapchainImages(swapchains[eye], count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages[eye])));
    }
    OutputDebugStringA("**OVERLAY** success in thread creating swapchain\n");

    // Spawn a thread to wait for a keypress
    static bool quit = false;
    auto exitPollingThread = std::thread{[] {
        printf("Press any key to shutdown...");
        getchar();
        quit = true;
    }};
    exitPollingThread.detach();

    int whichImage = 0;
    auto then = std::chrono::steady_clock::now();
    while(!quit) {
        auto now = std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count() > 1000) {
            whichImage = (whichImage + 1) % 2;
            then = std::chrono::steady_clock::now();
        }

        XrFrameState waitFrameState;
        ipcxrWaitFrame(session, nullptr, &waitFrameState);
        OutputDebugStringA("**OVERLAY** exited overlay session xrWaitFrame\n");
        ipcxrBeginFrame(session, nullptr);
        for(int eye = 0; eye < 2; eye++) {
            uint32_t index;
            XrSwapchain sc = reinterpret_cast<XrSwapchain>(swapchains[eye]);
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            acquireInfo.next = nullptr;
            // TODO - these should be layered
            CHECK_XR(ipcxrAcquireSwapchainImage(sc, &acquireInfo, &index));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.next = nullptr;
            waitInfo.timeout = ONE_SECOND_IN_NANOSECONDS;
            CHECK_XR(ipcxrWaitSwapchainImage(sc, &waitInfo));

            d3dContext->CopyResource(swapchainImages[eye][index].texture, sourceImages[whichImage]);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            releaseInfo.next = nullptr;
            CHECK_XR(ipcxrReleaseSwapchainImage(sc, &releaseInfo));
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
        frameEndInfo.displayTime = waitFrameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE; // XXX ignored
        ipcxrEndFrame(session, &frameEndInfo);
    }

    CHECK_XR(ipcxrDestroySession(session));


    return 0;
}
