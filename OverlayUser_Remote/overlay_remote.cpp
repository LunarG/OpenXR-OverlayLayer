// Remote.cpp 

#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>

#define XR_USE_GRAPHICS_API_D3D11 1

#include "../XR_overlay_ext/xr_overlay_dll.h"

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

std::map<XrSession, LocalSession> gLocalSessionMap;

struct LocalSwapchain
{
    XrSwapchain             swapchain;
    size_t                  swapchainTextureCount;
    int                     nextSwapchain; 
    ID3D11Texture2D*        swapchainTextures;
    LocalSwapchain(XrSwapchain sc, size_t count, ID3D11Device* d3d11) :
        swapchain(sc),
        nextSwapchain(0)
    {
        abort();
        // Create D3D textures from saved device
    }
    ~LocalSwapchain()
    {
        abort();
        // Need to acquire back from remote side?
        // destroy swapchains
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
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_CREATE_SESSION};

    IPCXrCreateSession args {instance, createInfo, session};

    IPCXrCreateSession* argsSerialized = IPCSerialize(ipcbuf, header, &args);
    header->makePointersRelative(ipcbuf.base);

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

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


int main( void )
{
    //DebugBreak();

    // WCHAR cBuf[MAX_PATH];
    // GetSharedMem(cBuf, MAX_PATH);
    // printf("Child process read from shared memory: %S\n", cBuf);

    XrInstance instance;
    CHECK_XR(ipcxrHandshake(&instance));

    XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
    sessionCreateInfoOverlay.next = nullptr;
    sessionCreateInfoOverlay.overlaySession = XR_TRUE;
    sessionCreateInfoOverlay.sessionLayersPlacement = 1;

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &sessionCreateInfoOverlay;

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

    return 0;
}
