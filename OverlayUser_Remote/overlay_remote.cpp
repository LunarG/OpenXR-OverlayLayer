// Remote.cpp 

#include <windows.h>
#include <tchar.h>
#include <iostream>

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


// MUST ONLY DEFAULT FOR STRUCTS WITH NO POINTERS IN THEM
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

// MUST ONLY DEFAULT FOR STRUCTS WITH NO POINTERS IN THEM
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

// MUST ONLY DEFAULT FOR STRUCTS WITH NO POINTERS IN THEM
template <typename T>
void IPCCopyOut(T* dst, const T* src)
{
    if(!src)
        return;

    *dst = *src;
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
IPCXrCreateSession* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrCreateSession* src)
{
    IPCXrCreateSession *dst = new(ipcbuf) IPCXrCreateSession;

    dst->instance = src->instance;

    dst->createInfo = reinterpret_cast<const XrSessionCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo)));
    header->addOffsetToPointer(ipcbuf.base, &dst->createInfo);

    // TODO don't bother copying session here since session is only out
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
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_HANDSHAKE};

    IPCXrHandshake handshakeArgs {instance};
    IPCXrHandshake *handshakeArgsSerialized = IPCSerialize(ipcbuf, header, &handshakeArgs);

    printf("buf before Remote makePointersRelative: ");
    for(int i = 0; i < 32; i++)
        printf("%02X ", ((unsigned char*)handshakeArgsSerialized)[i]);
    puts("");
    header->makePointersRelative(ipcbuf.base);
    printf("buf after Remote makePointersRelative : ");
    for(int i = 0; i < 32; i++)
        printf("%02X ", ((unsigned char*)handshakeArgsSerialized)[i]);
    puts("");

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    printf("buf before Remote makePointersAbsolute: ");
    for(int i = 0; i < 32; i++)
        printf("%02X ", ((unsigned char*)handshakeArgsSerialized)[i]);
    puts("");
    header->makePointersAbsolute(ipcbuf.base);
    printf("buf after Remote makePointersAbsolute : ");
    for(int i = 0; i < 32; i++)
        printf("%02X ", ((unsigned char*)handshakeArgsSerialized)[i]);
    puts("");

    IPCCopyOut(&handshakeArgs, handshakeArgsSerialized);

    return header->result;
}

XrResult ipcxrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_CREATE_SESSION};

    IPCXrCreateSession createSessionArgs {instance, createInfo, session};

    IPCXrCreateSession* createSessionArgsSerialized = IPCSerialize(ipcbuf, header, &createSessionArgs);
    header->makePointersRelative(ipcbuf.base);

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&createSessionArgs, createSessionArgsSerialized);

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
    printf("instance is %p\n", instance);

    XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
    sessionCreateInfoOverlay.next = nullptr;
    sessionCreateInfoOverlay.overlaySession = XR_TRUE;
    sessionCreateInfoOverlay.sessionLayersPlacement = 1;

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &sessionCreateInfoOverlay;

    XrSession session;
    CHECK_XR(ipcxrCreateSession(instance, &sessionCreateInfo, &session));
    printf("session is %p\n", session);
    OutputDebugStringA("**OVERLAY** success in thread creating overlay session\n");
    
    return 0;
}
