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

// Use this macro to test if HANDLE or pointer functions succeeded that also update LastError
#define CHECK_NOT_NULL(a) CheckResultWithLastError(((a) != NULL), #a, __FILE__, __LINE__)

// Use this macro to test if functions succeeded that also update LastError
#define CHECK_LAST_ERROR(a) CheckResultWithLastError((a), #a, __FILE__, __LINE__)

// Use this macro to test Direct3D functions
#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

// Use this macro to test OpenXR functions
#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

// Local bookkeeping information associated with an XrSession (Mostly just a place to cache D3D11 device)
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

// The Id of the RPC Host Process
DWORD gHostProcessId;

// Local "Swapchain" in Xr parlance - others would call it RenderTarget
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
            desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
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

                // Get the Shared Handle for the texture. This is still local to this process but is an actual HANDLE
                CHECK(sharedResource->CreateSharedHandle(NULL,
                    DXGI_SHARED_RESOURCE_READ, // GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                    NULL, &handle));
                
                // Duplicate the handle so "Host" RPC service process can use it
                CHECK_LAST_ERROR(DuplicateHandle(thisProcessHandle, handle, hostProcessHandle, &swapchainHandles[i], 0, TRUE, DUPLICATE_SAME_ACCESS));
                CHECK_LAST_ERROR(CloseHandle(handle));
                sharedResource->Release();
            }
        }
    }

    ~LocalSwapchain()
    {
        // XXX Need to AcquireSync from remote side?
        for(int i = 0; i < swapchainTextures.size(); i++) {
            swapchainTextures[i]->Release();
        }
    }
};

typedef std::unique_ptr<LocalSwapchain> LocalSwapchainPtr;
std::map<XrSwapchain, LocalSwapchainPtr> gLocalSwapchainMap;


// Serialization helpers ----------------------------------------------------

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
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

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
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

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
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

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
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

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
template <typename T>
void IPCCopyOut(T* dst, const T* src)
{
    if(!src)
        return;

    *dst = *src;
}

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
template <typename T>
void IPCCopyOut(T* dst, const T* src, size_t count)
{
    if(!src)
        return;

    for(size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

// Serialization of XR structs ----------------------------------------------

enum SerializationType {
    SERIALIZE_EVERYTHING,       // XR command will consume (aka input)
    SERIALIZE_ONLY_TYPE_NEXT,   // XR command will fill (aka output)
};

XrBaseInStructure* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const XrBaseInStructure* srcbase, SerializationType serializationType)
{
    XrBaseInStructure *dstbase = nullptr;
    bool skipped;

    do {
        skipped = false;

        if(!srcbase) {
            return nullptr;
        }

        switch(srcbase->type) {

            // Should copy only non-pointers instead of "*dst = *src"

            case XR_TYPE_FRAME_STATE: {
                auto src = reinterpret_cast<const XrFrameState*>(srcbase);
                auto dst = new(ipcbuf) XrFrameState;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_INSTANCE_PROPERTIES: {
                auto src = reinterpret_cast<const XrInstanceProperties*>(srcbase);
                auto dst = new(ipcbuf) XrInstanceProperties;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_VIEW: {
                auto src = reinterpret_cast<const XrViewConfigurationView*>(srcbase);
                auto dst = new(ipcbuf) XrViewConfigurationView;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_PROPERTIES: {
                auto src = reinterpret_cast<const XrViewConfigurationProperties*>(srcbase);
                auto dst = new(ipcbuf) XrViewConfigurationProperties;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_SESSION_BEGIN_INFO: {
                auto src = reinterpret_cast<const XrSessionBeginInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSessionBeginInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_SWAPCHAIN_CREATE_INFO: {
                auto src = reinterpret_cast<const XrSwapchainCreateInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_FRAME_WAIT_INFO: {
                auto src = reinterpret_cast<const XrFrameWaitInfo*>(srcbase);
                auto dst = new(ipcbuf) XrFrameWaitInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_FRAME_BEGIN_INFO: {
                auto src = reinterpret_cast<const XrFrameBeginInfo*>(srcbase);
                auto dst = new(ipcbuf) XrFrameBeginInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_COMPOSITION_LAYER_QUAD: {
                auto src = reinterpret_cast<const XrCompositionLayerQuad*>(srcbase);
                auto dst = new(ipcbuf) XrCompositionLayerQuad;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
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
                    layers[i] = reinterpret_cast<XrCompositionLayerBaseHeader*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->layers[i]), serializationType));
                    header->addOffsetToPointer(ipcbuf.base, &layers[i]);
                }
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO: {
                auto src = reinterpret_cast<const XrSwapchainImageAcquireInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainImageAcquireInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO: {
                auto src = reinterpret_cast<const XrSwapchainImageWaitInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainImageWaitInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO: {
                auto src = reinterpret_cast<const XrSwapchainImageReleaseInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSwapchainImageReleaseInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO: {
                auto src = reinterpret_cast<const XrSessionCreateInfo*>(srcbase);
                auto dst = new(ipcbuf) XrSessionCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT: {
                auto src = reinterpret_cast<const XrSessionCreateInfoOverlayEXT*>(srcbase);
                auto dst = new(ipcbuf) XrSessionCreateInfoOverlayEXT;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
                break;
            }

            case XR_TYPE_REFERENCE_SPACE_CREATE_INFO: {
                auto src = reinterpret_cast<const XrReferenceSpaceCreateInfo*>(srcbase);
                auto dst = new(ipcbuf) XrReferenceSpaceCreateInfo;
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                if(serializationType == SERIALIZE_EVERYTHING) {
                    *dst = *src;
                } else {
                    dst->type = src->type;
                }
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

    dstbase->next = reinterpret_cast<XrBaseInStructure*>(IPCSerialize(ipcbuf, header, srcbase->next, serializationType));
    header->addOffsetToPointer(ipcbuf.base, &dstbase->next);

    return dstbase;
}

// CopyOut XR structs -------------------------------------------------------

template <>
void IPCCopyOut(XrBaseOutStructure* dstbase, const XrBaseOutStructure* srcbase)
{
    bool skipped = true;

    do {
        skipped = false;

        if(!srcbase) {
            return;
        }

        switch(dstbase->type) {

            case XR_TYPE_VIEW_CONFIGURATION_PROPERTIES: {
                auto src = reinterpret_cast<const XrViewConfigurationProperties*>(srcbase);
                auto dst = reinterpret_cast<XrViewConfigurationProperties*>(dstbase);
                dst->viewConfigurationType = src->viewConfigurationType;
                dst->fovMutable = src->fovMutable;
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_VIEW: {
                auto src = reinterpret_cast<const XrViewConfigurationView*>(srcbase);
                auto dst = reinterpret_cast<XrViewConfigurationView*>(dstbase);
                dst->recommendedImageRectWidth = src->recommendedImageRectWidth;
                dst->maxImageRectWidth = src->maxImageRectWidth;
                dst->recommendedImageRectHeight = src->recommendedImageRectHeight;
                dst->maxImageRectHeight = src->maxImageRectHeight;
                dst->recommendedSwapchainSampleCount = src->recommendedSwapchainSampleCount;
                dst->maxSwapchainSampleCount = src->maxSwapchainSampleCount;
                break;
            }

            default: {
                // I don't know what this is, drop it and keep going
                std::string str = fmt("IPCCopyOut called to copy out to %p of unknown type %d - skipped.\n", dstbase, dstbase->type);
                OutputDebugStringA(str.data());

                dstbase = dstbase->next;
                skipped = true;

                // Don't increment srcbase.  Unknown structs were
                // dropped during serialization, so keep going until we
                // see a type we know and then we'll have caught up with
                // what was serialized.
                //
                break;
            }
        }
    } while(skipped);

    IPCCopyOut(dstbase->next, srcbase->next);
}

// xrEnumerateSwapchainFormats ----------------------------------------------

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

    return header->result;
}

// xrEnumerateViewConfigurations --------------------------------------------

template <>
IPCXrEnumerateViewConfigurations* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrEnumerateViewConfigurations* src)
{
    auto dst = new(ipcbuf) IPCXrEnumerateViewConfigurations;

    dst->instance = src->instance;
    dst->systemId = src->systemId;
    dst->viewConfigurationTypeCapacityInput = src->viewConfigurationTypeCapacityInput;
    dst->viewConfigurationTypeCountOutput = IPCSerializeNoCopy(ipcbuf, header, src->viewConfigurationTypeCountOutput);
    header->addOffsetToPointer(ipcbuf.base, &dst->viewConfigurationTypeCountOutput);
    dst->viewConfigurationTypes = IPCSerializeNoCopy(ipcbuf, header, src->viewConfigurationTypes, src->viewConfigurationTypeCapacityInput);
    header->addOffsetToPointer(ipcbuf.base, &dst->viewConfigurationTypes);

    return dst;
}

template <>
void IPCCopyOut(IPCXrEnumerateViewConfigurations* dst, const IPCXrEnumerateViewConfigurations* src)
{
    IPCCopyOut(dst->viewConfigurationTypeCountOutput, src->viewConfigurationTypeCountOutput);
    if (src->viewConfigurationTypes) {
        IPCCopyOut(dst->viewConfigurationTypes, src->viewConfigurationTypes, *src->viewConfigurationTypeCountOutput);
    }
}

XrResult xrEnumerateViewConfigurations(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    viewConfigurationTypeCapacityInput,
    uint32_t*                                   viewConfigurationTypeCountOutput,
    XrViewConfigurationType*                    viewConfigurationTypes)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_ENUMERATE_VIEW_CONFIGURATIONS};

    IPCXrEnumerateViewConfigurations args {instance, systemId, viewConfigurationTypeCapacityInput, viewConfigurationTypeCountOutput, viewConfigurationTypes};

    IPCXrEnumerateViewConfigurations* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

// xrGetInstanceProperties --------------------------------------------------

template <>
IPCXrGetInstanceProperties* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrGetInstanceProperties* src)
{
    auto dst = new(ipcbuf) IPCXrGetInstanceProperties;

    dst->instance = src->instance;

    dst->properties = reinterpret_cast<XrInstanceProperties*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->properties), SERIALIZE_ONLY_TYPE_NEXT));
    header->addOffsetToPointer(ipcbuf.base, &dst->properties);

    return dst;
}

template <>
void IPCCopyOut(IPCXrGetInstanceProperties* dst, const IPCXrGetInstanceProperties* src)
{
    IPCCopyOut(dst->properties, src->properties);
}

XrResult xrGetInstanceProperties (
    XrInstance                                   instance,
    XrInstanceProperties*                        properties)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_GET_INSTANCE_PROPERTIES};

    IPCXrGetInstanceProperties args {instance, properties};

    IPCXrGetInstanceProperties* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

// xrCreateSwapchain --------------------------------------------------------

template <>
IPCXrCreateSwapchain* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrCreateSwapchain* src)
{
    auto dst = new(ipcbuf) IPCXrCreateSwapchain;

    dst->session = src->session;

    dst->createInfo = reinterpret_cast<const XrSwapchainCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->createInfo);

    dst->swapchain = IPCSerializeNoCopy(ipcbuf, header, src->swapchain);
    header->addOffsetToPointer(ipcbuf.base, &dst->swapchain);

    dst->swapchainCount = IPCSerializeNoCopy(ipcbuf, header, src->swapchainCount);
    header->addOffsetToPointer(ipcbuf.base, &dst->swapchainCount);

    return dst;
}

template <>
void IPCCopyOut(IPCXrCreateSwapchain* dst, const IPCXrCreateSwapchain* src)
{
    IPCCopyOut(dst->swapchain, src->swapchain);
    IPCCopyOut(dst->swapchainCount, src->swapchainCount);
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

// xrWaitFrame --------------------------------------------------------------

template <>
IPCXrWaitFrame* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrWaitFrame* src)
{
    auto dst = new(ipcbuf) IPCXrWaitFrame;

    dst->session = src->session;

    dst->frameWaitInfo = reinterpret_cast<const XrFrameWaitInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->frameWaitInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->frameWaitInfo);

    dst->frameState = reinterpret_cast<XrFrameState*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->frameState), SERIALIZE_ONLY_TYPE_NEXT));
    header->addOffsetToPointer(ipcbuf.base, &dst->frameState);

    return dst;
}

template <>
void IPCCopyOut(IPCXrWaitFrame* dst, const IPCXrWaitFrame* src)
{
    IPCCopyOut(dst->frameState, src->frameState);
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

// xrBeginFrame -------------------------------------------------------------

template <>
IPCXrBeginFrame* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrBeginFrame* src)
{
    auto dst = new(ipcbuf) IPCXrBeginFrame;

    dst->session = src->session;

    dst->frameBeginInfo = reinterpret_cast<const XrFrameBeginInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->frameBeginInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->frameBeginInfo);

    return dst;
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

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    return header->result;
}

// xrEndFrame ---------------------------------------------------------------

template <>
IPCXrEndFrame* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrEndFrame* src)
{
    auto dst = new(ipcbuf) IPCXrEndFrame;

    dst->session = src->session;

    dst->frameEndInfo = reinterpret_cast<const XrFrameEndInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->frameEndInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->frameEndInfo);

    return dst;
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

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    return header->result;
}

// xrAcquireSwapchainImage --------------------------------------------------

template <>
IPCXrAcquireSwapchainImage* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrAcquireSwapchainImage* src)
{
    auto dst = new(ipcbuf) IPCXrAcquireSwapchainImage;

    dst->swapchain = src->swapchain;

    dst->acquireInfo = reinterpret_cast<const XrSwapchainImageAcquireInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->acquireInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->acquireInfo);

    dst->index = IPCSerializeNoCopy(ipcbuf, header, src->index);
    header->addOffsetToPointer(ipcbuf.base, &dst->index);

    return dst;
}

template <>
void IPCCopyOut(IPCXrAcquireSwapchainImage* dst, const IPCXrAcquireSwapchainImage* src)
{
    IPCCopyOut(dst->index, src->index);
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

// xrWaitSwapchainImage -----------------------------------------------------

template <>
IPCXrWaitSwapchainImage* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrWaitSwapchainImage* src)
{
    auto dst = new(ipcbuf) IPCXrWaitSwapchainImage;

    dst->swapchain = src->swapchain;

    dst->waitInfo = reinterpret_cast<const XrSwapchainImageWaitInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->waitInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->waitInfo);

    dst->sourceImage = src->sourceImage;

    return dst;
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

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    localSwapchain->waited = true;
    IDXGIKeyedMutex* keyedMutex;
    CHECK(localSwapchain->swapchainTextures[wasWaited]->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex));
    CHECK(keyedMutex->AcquireSync(KEYED_MUTEX_IPC_REMOTE, INFINITE));
    keyedMutex->Release();

    return header->result;
}

// xrReleaseSwapchainImage --------------------------------------------------

template <>
IPCXrReleaseSwapchainImage* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrReleaseSwapchainImage* src)
{
    auto dst = new(ipcbuf) IPCXrReleaseSwapchainImage;

    dst->swapchain = src->swapchain;

    dst->releaseInfo = reinterpret_cast<const XrSwapchainImageReleaseInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->releaseInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->releaseInfo);

    dst->sourceImage = src->sourceImage;

    return dst;
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
    keyedMutex->Release();

    HANDLE sharedResourceHandle = localSwapchain->swapchainHandles[beingReleased];
    IPCXrReleaseSwapchainImage args {swapchain, waitInfo, sharedResourceHandle};

    IPCXrReleaseSwapchainImage* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    gLocalSwapchainMap[swapchain]->waited = false;

    return header->result;
}

// xrDestroySession ---------------------------------------------------------

template <>
IPCXrDestroySession* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrDestroySession* src)
{
    auto dst = new(ipcbuf) IPCXrDestroySession;

    dst->session = src->session;

    return dst;
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

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    if(header->result == XR_SUCCESS) {
        gLocalSessionMap.erase(session);
    }

    return header->result;
}

// xrCreateSession ----------------------------------------------------------

template <>
IPCXrCreateSession* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrCreateSession* src)
{
    auto dst = new(ipcbuf) IPCXrCreateSession;

    dst->instance = src->instance;

    dst->createInfo = reinterpret_cast<const XrSessionCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo), SERIALIZE_EVERYTHING));
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

// xrCreateReferenceSpace ---------------------------------------------------

template <>
IPCXrCreateReferenceSpace* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrCreateReferenceSpace* src)
{
    auto dst = new(ipcbuf) IPCXrCreateReferenceSpace;

    dst->session = src->session;

    dst->createInfo = reinterpret_cast<const XrReferenceSpaceCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo), SERIALIZE_EVERYTHING));
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

// ipcxrHandshake -----------------------------------------------------------

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

// xrDestroySwapchain -------------------------------------------------------

template <>
IPCXrDestroySwapchain* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrDestroySwapchain* src)
{
    auto dst = new(ipcbuf) IPCXrDestroySwapchain;

    dst->swapchain = src->swapchain;

    return dst;
}

XrResult xrDestroySwapchain(
    XrSwapchain                                 swapchain)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_DESTROY_SWAPCHAIN};

    IPCXrDestroySwapchain args {swapchain};

    IPCXrDestroySwapchain* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    gLocalSwapchainMap.erase(gLocalSwapchainMap.find(swapchain));

    return header->result;
}

// xrDestroySpace -----------------------------------------------------------

template <>
IPCXrDestroySpace* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrDestroySpace* src)
{
    auto dst = new(ipcbuf) IPCXrDestroySpace;

    dst->space = src->space;

    return dst;
}

XrResult xrDestroySpace(
    XrSpace                                     space)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_DESTROY_SPACE};

    IPCXrDestroySpace args {space};

    IPCXrDestroySpace* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    return header->result;
}

// xrEndSession -------------------------------------------------------------

template <>
IPCXrEndSession* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrEndSession* src)
{
    auto dst = new(ipcbuf) IPCXrEndSession;

    dst->session = src->session;

    return dst;
}

XrResult xrEndSession(
    XrSession                                   session)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_END_SESSION};

    IPCXrEndSession args {session};

    IPCXrEndSession* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();

    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    return header->result;
}

// xrBeginSession -----------------------------------------------------------

template <>
IPCXrBeginSession* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrBeginSession* src)
{
    auto dst = new(ipcbuf) IPCXrBeginSession;

    dst->session = src->session;

    dst->beginInfo = reinterpret_cast<const XrSessionBeginInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->beginInfo), SERIALIZE_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->beginInfo);

    return dst;
}

XrResult xrBeginSession(
    XrSession                                   session,
    const XrSessionBeginInfo*                   beginInfo)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_BEGIN_SESSION};

    IPCXrBeginSession args {session, beginInfo};

    IPCXrBeginSession* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    // IPCCopyOut(&args, argsSerialized); // Nothing to copy back out

    return header->result;
}

// xrGetViewConfigurationProperties -----------------------------------------

template <>
IPCXrGetViewConfigurationProperties* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrGetViewConfigurationProperties* src)
{
    auto dst = new(ipcbuf) IPCXrGetViewConfigurationProperties;

    dst->instance = src->instance;
    dst->systemId = src->systemId;
    dst->viewConfigurationType = src->viewConfigurationType;

    dst->configurationProperties = reinterpret_cast<XrViewConfigurationProperties*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->configurationProperties), SERIALIZE_ONLY_TYPE_NEXT));
    header->addOffsetToPointer(ipcbuf.base, &dst->configurationProperties);

    return dst;
}

template <>
void IPCCopyOut(IPCXrGetViewConfigurationProperties* dst, const IPCXrGetViewConfigurationProperties* src)
{
    IPCCopyOut(
            reinterpret_cast<XrBaseOutStructure*>(dst->configurationProperties),
            reinterpret_cast<const XrBaseOutStructure*>(src->configurationProperties)
            );
}

XrResult xrGetViewConfigurationProperties(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrViewConfigurationType                     viewConfigurationType,
    XrViewConfigurationProperties*              configurationProperties)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_GET_VIEW_CONFIGURATION_PROPERTIES};

    IPCXrGetViewConfigurationProperties args {instance, systemId, viewConfigurationType, configurationProperties};

    IPCXrGetViewConfigurationProperties* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

// xrEnumerateViewConfigurationViews ----------------------------------------

template <>
IPCXrEnumerateViewConfigurationViews* IPCSerialize(IPCBuffer& ipcbuf, IPCXrHeader* header, const IPCXrEnumerateViewConfigurationViews* src)
{
    auto dst = new(ipcbuf) IPCXrEnumerateViewConfigurationViews;

    dst->instance = src->instance;
    dst->systemId = src->systemId;
    dst->viewConfigurationType = src->viewConfigurationType;
    dst->viewCapacityInput = src->viewCapacityInput;

    dst->viewCountOutput = IPCSerializeNoCopy(ipcbuf, header, src->viewCountOutput);
    header->addOffsetToPointer(ipcbuf.base, &dst->viewCountOutput);

    if(dst->viewCapacityInput > 0) {
        dst->views = new(ipcbuf) XrViewConfigurationView[dst->viewCapacityInput];
        header->addOffsetToPointer(ipcbuf.base, &dst->views);
        for(uint32_t i = 0; i < dst->viewCapacityInput; i++) {
            dst->views[i].type = src->views[i].type;
            dst->views[i].next = IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->views[i].next), SERIALIZE_ONLY_TYPE_NEXT);
            header->addOffsetToPointer(ipcbuf.base, &dst->views[i].next);
        }
    }

    return dst;
}

template <>
void IPCCopyOut(IPCXrEnumerateViewConfigurationViews* dst, const IPCXrEnumerateViewConfigurationViews* src)
{
    IPCCopyOut(dst->viewCountOutput, src->viewCountOutput);
    uint32_t toCopy = std::min(src->viewCapacityInput, (uint32_t)*src->viewCountOutput);
    for(uint32_t i = 0; i < toCopy; i++) {
        IPCCopyOut(
            reinterpret_cast<XrBaseOutStructure*>(&dst->views[i]),
            reinterpret_cast<const XrBaseOutStructure*>(&src->views[i])
            );
    }
}

XrResult xrEnumerateViewConfigurationViews(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrViewConfigurationType                     viewConfigurationType,
    uint32_t                                    viewCapacityInput,
    uint32_t*                                   viewCountOutput,
    XrViewConfigurationView*                    views)
{
    IPCBuffer ipcbuf = IPCGetBuffer();
    IPCXrHeader* header = new(ipcbuf) IPCXrHeader{IPC_XR_ENUMERATE_VIEW_CONFIGURATION_VIEWS};

    IPCXrEnumerateViewConfigurationViews args {instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views};

    IPCXrEnumerateViewConfigurationViews* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    header->makePointersRelative(ipcbuf.base);
    IPCFinishGuestRequest();
    IPCWaitForHostResponse();
    header->makePointersAbsolute(ipcbuf.base);

    IPCCopyOut(&args, argsSerialized);

    return header->result;
}

// xrEnumerateSwapchainImages -----------------------------------------------
// (Not serialized, handled locally)

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

    // (If storage is provided) Give back the "local" swapchainimages (rendertarget) for rendering
    auto sci = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
    uint32_t toWrite = std::min(imageCapacityInput, (uint32_t)localSwapchain->swapchainTextures.size());
    for(uint32_t i = 0; i < toWrite; i++) {
        sci[i].texture = localSwapchain->swapchainTextures[i];
    }

    *imageCountOutput = toWrite;

    return XR_SUCCESS;
}

