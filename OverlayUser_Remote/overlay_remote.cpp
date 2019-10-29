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

bool packXrStruct(unsigned char*& ptr, const void* p);

template <>
bool pack(unsigned char*& ptr, const XrSessionCreateInfo& v)
{
    pack(ptr, v.type);
    packXrStruct(ptr, v.next);
    pack(ptr, v.createFlags);
    pack(ptr, v.systemId);
	return true;
}

template <>
bool pack(unsigned char*& ptr, const XrSessionCreateInfoOverlayEXT& v)
{
    pack(ptr, v.type);
    packXrStruct(ptr, v.next);
    pack(ptr, v.overlaySession);
    pack(ptr, v.sessionLayersPlacement);
	return true;
}

bool packXrStruct(unsigned char*& ptr, const void *vp)
{
    const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(vp);
    if(p) {
        pack(ptr, true);

        bool chainedNext = false;
        do {
            switch(p->type) {
                case XR_TYPE_SESSION_CREATE_INFO:
                    pack(ptr, *reinterpret_cast<const XrSessionCreateInfo*>(p));
                    break;
                case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT:
                    pack(ptr, *reinterpret_cast<const XrSessionCreateInfoOverlayEXT*>(p));
                    break;
                default:
                    // I don't know what this is
                    std::string str = fmt("packXrStruct called to pack unknown type %d - dropped from \"next\" chain.\n", p->type);
                    OutputDebugStringA(str.data());
                    chainedNext = true;
                    p = reinterpret_cast<const XrBaseInStructure*>(p->next);
                    break;
            }
        } while (chainedNext);
    } else {
        pack(ptr, false);
    }
	return true;
}

template <>
bool pack(unsigned char*& ptr, const IPCXrCreateSessionIn& v)
{
    pack(ptr, v.instance);
    packXrStruct(ptr, v.createInfo);
    return true;
}


XrResult ipcxrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session)
{
    XrResult result;

    unsigned char* packPtr = reinterpret_cast<unsigned char*>(IPCGetSharedMemory());

    pack(packPtr, (uint64_t)IPC_XR_CREATE_SESSION);
    IPCXrCreateSessionIn in {instance, createInfo};
    pack(packPtr, in);

    IPCFinishGuestRequest();
    IPCWaitForHostResponse();

    unsigned char* unpackPtr = reinterpret_cast<unsigned char*>(IPCGetSharedMemory());
    result = unpack<XrResult>(unpackPtr);
    *session = unpack<XrSession>(unpackPtr);
    printf("session is %p\n", *session);

    return result;
}


int main( void )
{
    //DebugBreak();

    // WCHAR cBuf[MAX_PATH];
    // GetSharedMem(cBuf, MAX_PATH);
    // printf("Child process read from shared memory: %S\n", cBuf);


    unsigned char* packPtr = reinterpret_cast<unsigned char*>(IPCGetSharedMemory());

    uint64_t requestType = IPC_REQUEST_HANDOFF;
    size_t requestSize = 0;
    pack(packPtr, requestType);

    IPCFinishGuestRequest();

    IPCWaitForHostResponse();

    unsigned char* unpackPtr = reinterpret_cast<unsigned char*>(IPCGetSharedMemory());
    XrResult result = unpack<XrResult>(unpackPtr); /* ignored for HANDOFF */
    XrInstance instance = unpack<XrInstance>(unpackPtr);
    printf("instance is %p\n", instance);

    XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
    sessionCreateInfoOverlay.next = nullptr;
    sessionCreateInfoOverlay.overlaySession = XR_TRUE;
    sessionCreateInfoOverlay.sessionLayersPlacement = 1;

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &sessionCreateInfoOverlay;

    XrSession session;
    CHECK_XR(ipcxrCreateSession(instance, &sessionCreateInfo, &session));
    OutputDebugStringA("**OVERLAY** success in thread creating overlay session\n");

    
    return 0;
}
