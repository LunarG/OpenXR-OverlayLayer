#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <new>
#include <cstdlib>

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

typedef struct XrSessionCreateInfoOverlayEXT
{
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrBool32                    overlaySession;
    uint32_t                    sessionLayersPlacement;
} XrSessionCreateInfoOverlayEXT;

struct IPCXrHandshake {
    XrInstance *instance;
};

struct IPCXrCreateSession {
    XrInstance instance;
    const XrSessionCreateInfo *createInfo;
    XrSession* session;
};

struct IPCXrCreateReferenceSpace {
    XrSession session;
    const XrReferenceSpaceCreateInfo *createInfo;
    XrSpace* space;
};

#define MAX_POINTER_FIXUP_COUNT 128

struct IPCXrHeader
{
    uint64_t requestType;
    XrResult result;

    int pointerFixupCount;
    size_t pointerOffsets[MAX_POINTER_FIXUP_COUNT];

    IPCXrHeader(uint64_t requestType) :
        requestType(requestType),
        pointerFixupCount(0)
    {}

    bool addOffsetToPointer(void* vbase, void* vp)
    {
        if(pointerFixupCount >= MAX_POINTER_FIXUP_COUNT)
            return false;

        unsigned char* base = reinterpret_cast<unsigned char *>(vbase);
        unsigned char* p = reinterpret_cast<unsigned char *>(vp);
        pointerOffsets[pointerFixupCount++] = p - base;
        return true;
    }

    void makePointersRelative(void* vbase)
    {
        unsigned char* base = reinterpret_cast<unsigned char *>(vbase);
        for(int i = 0; i < pointerFixupCount; i++) {
            unsigned char* pointerToByte = base + pointerOffsets[i];
            unsigned char** pointerToPointer = reinterpret_cast<unsigned char **>(pointerToByte);
            unsigned char*& pointer = *pointerToPointer;
            if(pointer != nullptr) { // nullptr remains nulltpr
                pointer = pointer - (base - reinterpret_cast<unsigned char *>(0));
            }
        }
    }

    void makePointersAbsolute(void* vbase)
    {
        unsigned char* base = reinterpret_cast<unsigned char *>(vbase);
        for(int i = 0; i < pointerFixupCount; i++) {
            unsigned char* pointerToByte = base + pointerOffsets[i];
            unsigned char** pointerToPointer = reinterpret_cast<unsigned char **>(pointerToByte);
            unsigned char*& pointer = *pointerToPointer;
            if(pointer != nullptr) { // nullptr remains nulltpr
                pointer = pointer + (base - reinterpret_cast<unsigned char *>(0));
            }
        }
    }
};

static const int memberAlignment = 8;

static size_t pad(size_t s)
{
    return (s + memberAlignment - 1) / memberAlignment * memberAlignment;
}

struct IPCBuffer
{
    unsigned char *base;
    size_t size;
    unsigned char *current;

    static const int memberAlignment = 8;

    IPCBuffer(void *base_, size_t size_) :
        base(reinterpret_cast<unsigned char*>(base_)),
        size(size_)
    {
        reset();
    }

    void reset(void)
    {
        current = base;
    }

    void advance(size_t s)
    {
        current += pad(s);
    }

    bool write(const void* p, size_t s)
    {
        if((current - base + s) > size)
            return false;
        memcpy(current, p, s);
        advance(s);
        return true;
    }

    void read(void *p, size_t s)
    {
        if((current - base + s) > size)
            abort();
        memcpy(p, current, s);
        advance(s);
    }

    template <typename T>
    bool write(const T* p)
    {
        if((current - base + s) > size)
            return false;
        memcpy(current, p, s);
        advance(sizeof(T));
        return true;
    }
    
    template <typename T>
    bool read(T* p)
    {
        if((current - base + s) > size)
            return false;
        memcpy(p, current, s);
        advance(sizeof(T));
        return true;
    }

    template <typename T>
    T* getAndAdvance()
    {
        if(current - base + sizeof(T) > size)
            return nullptr;
        T *p = reinterpret_cast<T*>(current);
        advance(sizeof(T));
        return p;
    }

    void *allocate (std::size_t s)
    {
        if((current - base + s) > size)
            return nullptr;
        void *p = current;
        advance(s);
        return p;
    }
    void deallocate (void *) {}
};

inline void* operator new (std::size_t size, IPCBuffer& buffer)
{
    return buffer.allocate(size);
}

inline void operator delete(void* p, IPCBuffer& buffer)
{
    buffer.deallocate(p);
}


template <typename T>
T unpack(unsigned char*& ptr)
{
    T tmp = *reinterpret_cast<T*>(ptr);
    ptr += sizeof(T);
	return tmp;
}

template <typename T>
bool pack(unsigned char*& ptr, const T& v)
{
    // XXX TODO - don't overflow the buffer - also pass and check size
    *reinterpret_cast<T*>(ptr) = v;
    ptr += sizeof(T);
    return true;
}

const uint64_t IPC_HANDSHAKE = 1;
const uint64_t IPC_XR_CREATE_SESSION = 2;
const uint64_t IPC_XR_CREATE_REFERENCE_SPACE = 3;

XR_OVERLAY_EXT_API IPCBuffer IPCGetBuffer();

#ifdef __cplusplus    // If used by C++ code, 
extern "C" {          // we need to export the C interface
#endif

// Shared memory entry points
XR_OVERLAY_EXT_API bool MapSharedMemory(UINT32 size);
XR_OVERLAY_EXT_API bool UnmapSharedMemory();
XR_OVERLAY_EXT_API void SetSharedMem(LPCWSTR lpszBuf);
XR_OVERLAY_EXT_API void GetSharedMem(LPWSTR lpszBuf, size_t cchSize);
XR_OVERLAY_EXT_API void* IPCGetSharedMemory();
XR_OVERLAY_EXT_API bool IPCWaitForGuestRequest();
XR_OVERLAY_EXT_API void IPCFinishGuestRequest();
XR_OVERLAY_EXT_API bool IPCWaitForHostResponse();
XR_OVERLAY_EXT_API void IPCFinishHostResponse();

enum {
    // XXX need to do this with an enum generated from ext as part of build
    XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT = 1000099999,
};


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
XR_OVERLAY_EXT_API XrResult Overlay_xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);
XR_OVERLAY_EXT_API XrResult Overlay_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);


// Function used to negotiate an interface betewen the loader and a layer.
XR_OVERLAY_EXT_API XrResult Overlay_xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *li, const char *ln, XrNegotiateApiLayerRequest *lr);

     
#ifdef __cplusplus
}
#endif
