// Copyright (c) 2020-2021 LunarG, Inc.
// Copyright (c) 2017-2021 PlutoVR Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Brad Grantham <brad@lunarg.com>
// Author: John Zulauf <jzulauf@lunarg.com>
#ifndef _OVERLAYS_H_
#define _OVERLAYS_H_

#include <openxr/openxr.h>
#include <mutex>
#include <new>
#include <set>
#include <unordered_map>
#include <queue>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

struct OverlaysLayerXrException
{
    OverlaysLayerXrException(XrResult result) :
        mresult(result)
    {}

    XrResult result() const
    {
        return mresult;
    }

private:
    XrResult mresult;
};

enum CopyType {
    COPY_EVERYTHING,       // XR command will consume (aka input)
    COPY_ONLY_TYPE_NEXT,   // XR command will fill (aka output)
};

typedef std::function<void* (size_t size)> AllocateFunc;
typedef std::function<void (const void* p)> FreeFunc;
XrBaseInStructure *CopyXrStructChain(XrInstance instance, const XrBaseInStructure* srcbase, CopyType copyType, AllocateFunc alloc, std::function<void (void* pointerToPointer)> addOffsetToPointer);
void FreeXrStructChain(XrInstance instance, const XrBaseInStructure* p, FreeFunc free);
XrBaseInStructure* CopyEventChainIntoBuffer(XrInstance instance, const XrEventDataBaseHeader* eventData, XrEventDataBuffer* buffer);
XrBaseInStructure* CopyXrStructChainWithMalloc(XrInstance instance, const void* xrstruct);
void FreeXrStructChainWithFree(XrInstance instance, const void* xrstruct);

bool RestoreActualHandles(XrInstance instance, XrBaseInStructure *xrstruct);
void SubstituteLocalHandles(XrInstance instance, XrBaseOutStructure *xrstruct);

typedef std::pair<uint64_t, XrObjectType> HandleTypePair;

extern const std::set<HandleTypePair> OverlaysLayerNoObjectInfo;

void OverlaysLayerLogMessage(XrInstance instance,
                         XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message);

inline std::string fmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int size = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if(size >= 0) {
        int provided = size + 1;
        std::unique_ptr<char[]> buf(new char[provided]);

        va_start(args, fmt);
        vsnprintf(buf.get(), provided, fmt, args);
        va_end(args);

        return std::string(buf.get());
    }
    return "(fmt() failed, vsnprintf returned -1)";
}

// Header laid into the shared memory tracking the RPC type, the result,
// and all pointers inside the shared memory which have to be fixed up
// passing from Remote to Host and then back
struct IPCHeader
{
    uint64_t requestType;
    XrResult result;

    int pointerFixupCount;
    constexpr static int maxPointerFixupCount = 128;
    size_t pointerOffsets[maxPointerFixupCount];

    IPCHeader(uint64_t requestType) :
        requestType(requestType),
        pointerFixupCount(0)
    {}

    bool addOffsetToPointer(void* vbase, void* vp)
    {
        if(pointerFixupCount >= maxPointerFixupCount)
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
            if(pointer) { // nullptr remains nulltpr
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
            if(pointer) { // nullptr remains nulltpr
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

// Convenience object representing the shared memory buffer after the
// header, allowing apps to allocate bytes and then fill them or to read
// bytes and step over them
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

// New and delete for the buffer above
inline void* operator new (std::size_t size, IPCBuffer& buffer)
{
    return buffer.allocate(size);
}

inline void operator delete(void* p, IPCBuffer& buffer)
{
    buffer.deallocate(p);
}

struct NegotiationParams
{
    DWORD mainProcessId;
    DWORD overlayProcessId;
    uint32_t mainLayerBinaryVersion;
    uint32_t overlayLayerBinaryVersion;
    enum {SUCCESS, DIFFERENT_BINARY_VERSION} status;
};

struct NegotiationChannels
{
    XrInstance instance;

    HANDLE mutexHandle;

    HANDLE shmemHandle;
    NegotiationParams* params;

    HANDLE overlayWaitSema;
    HANDLE mainWaitSema;

    std::thread mainThread;

    HANDLE mainNegotiateThreadStop;

    constexpr static char *shmemName = "LUNARG_XR_EXTX_overlay_negotiation_shmem";
    constexpr static char *overlayWaitSemaName = "LUNARG_XR_EXTX_overlay_negotiation_overlay_wait_sema";
    constexpr static char *mainWaitSemaName = "LUNARG_XR_EXTX_overlay_negotiation_main_wait_sema";
    constexpr static char *mutexName = "LUNARG_XR_EXTX_overlay_negotiation_mutex";
    constexpr static uint32_t shmemSize = sizeof(NegotiationParams);
    constexpr static DWORD mutexWaitMillis = 500;
    constexpr static DWORD negotiationWaitMillis = 2000;
    constexpr static int NegotiationChannels::maxAttempts = 30;

};

extern bool gHaveMainSessionActive;
extern XrInstance gMainSessionInstance;
extern HANDLE gMainMutexHandle; // Held by Main for duration of operation as Main Session

struct RPCChannels
{
    XrInstance instance;

    HANDLE shmemHandle;
    void* shmem;

    HANDLE mutexHandle;

    HANDLE overlayRequestSema;
    HANDLE mainResponseSema;

    DWORD otherProcessId;
    HANDLE otherProcessHandle;

    constexpr static char *shmemNameTemplate = "LUNARG_XR_EXTX_overlay_rpc_shmem_%u";
    constexpr static char *overlayRequestSemaNameTemplate = "LUNARG_XR_EXTX_overlay_rpc_overlay_request_sema_%u";
    constexpr static char *mainResponseSemaNameTemplate = "LUNARG_XR_EXTX_overlay_rpc_main_response_sema_%u";
    constexpr static char *mutexNameTemplate = "LUNARG_XR_EXTX_overlay_rpc_mutex_%u";
    constexpr static uint32_t shmemSize = 1024 * 1024;
    constexpr static DWORD mutexWaitMillis = 500;
    constexpr static DWORD overlayRequestWaitMillis = 500;

    enum WaitResult {
        OVERLAY_REQUEST_READY,
        MAIN_RESPONSE_READY,
        OVERLAY_PROCESS_TERMINATED_UNEXPECTEDLY,
        MAIN_PROCESS_TERMINATED_UNEXPECTEDLY,
        OVERLAY_PROCESS_TERMINATED_GRACEFULLY,
        MAIN_PROCESS_TERMINATED_GRACEFULLY,
        REQUEST_PROCESSED_SUCCESSFULLY,
        WAIT_ERROR,
    };

    // Get the shared memory wrapped in a convenient structure
    IPCBuffer GetIPCBuffer()
    {
        return IPCBuffer(shmem, shmemSize);
    }

    WaitResult WaitForMainResponseOrFail()
    {
        HANDLE handles[2];

        handles[0] = mainResponseSema;
        handles[1] = otherProcessHandle;

        DWORD result;

        do {
            result = WaitForMultipleObjects(2, handles, FALSE, overlayRequestWaitMillis);
        } while(result == WAIT_TIMEOUT);

        if(result == WAIT_OBJECT_0 + 0) {
            return WaitResult::MAIN_RESPONSE_READY;
        }

        if(result == WAIT_OBJECT_0 + 1) {
            return WaitResult::MAIN_PROCESS_TERMINATED_UNEXPECTEDLY;
        }

        // XXX log error
        return WaitResult::WAIT_ERROR;
    }

    // Call from Host to get complete request in shmem
    WaitResult WaitForOverlayRequestOrFail()
    {
        HANDLE handles[2];

        handles[0] = overlayRequestSema;
        handles[1] = otherProcessHandle;

        DWORD result;

        do {
            result = WaitForMultipleObjects(2, handles, FALSE, overlayRequestWaitMillis);
        } while(result == WAIT_TIMEOUT);

        if(result == WAIT_OBJECT_0 + 0) {
            return WaitResult::OVERLAY_REQUEST_READY;
        }

        if(result == WAIT_OBJECT_0 + 1) {
            return WaitResult::OVERLAY_PROCESS_TERMINATED_UNEXPECTEDLY;
        }

        // XXX log error
        return WaitResult::WAIT_ERROR;
    }

    void FinishOverlayRequest()
    {
        ReleaseSemaphore(overlayRequestSema, 1, nullptr);
    }

    void FinishMainResponse()
    {
        ReleaseSemaphore(mainResponseSema, 1, nullptr);
    }
};

void OverlaysLayerRemoveXrSpaceHandleInfo(XrSpace localHandle);
void OverlaysLayerRemoveXrSwapchainHandleInfo(XrSwapchain localHandle);
void OverlaysLayerRemoveXrActionHandleInfo(XrAction localHandle);
void OverlaysLayerRemoveXrActionSetHandleInfo(XrActionSet actionSet);
void OverlaysLayerRemoveXrSessionHandleInfo(XrSession session);
void OverlaysLayerRemoveXrInstanceHandleInfo(XrInstance instance);

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
};

typedef std::pair<bool, XrSessionState> OptionalSessionStateChange;

struct MainSessionSessionState;

struct SessionStateTracker
{
    SessionLossState lossState = NOT_LOST;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool isRunning = false;
    bool exitRequested = false;

    SessionStateTracker()
    {
    }

    void DoCommand(OpenXRCommand command)
    {
        if(command == BEGIN_SESSION) {
            isRunning = true;
        } else if (command == END_SESSION) {
            isRunning = false;
        } else if (command == REQUEST_EXIT_SESSION) {
            exitRequested = true;
        }
    }

    void DoSessionLost()
    {
        lossState = LOST;
    }

    SessionLossState GetLossState()
    {
        return lossState;
    }

    OptionalSessionStateChange GetAndDoPendingStateChange(MainSessionSessionState *mainSession);
};


struct MainSessionSessionState : public SessionStateTracker
{
    XrTime currentTime;
    bool hasCalledWaitFrame = false;
    std::shared_ptr<XrFrameState> savedFrameState;

    MainSessionSessionState()
    {
    }

    void DoStateChange(XrSessionState state, XrTime when)
    {
        sessionState = state;
        currentTime = when;
    }

    void DoCommand(OpenXRCommand command)
    {
        if (command == WAIT_FRAME) {
            // XXX saved predicted times updated separately
        } else {
            if(command == BEGIN_SESSION) {
                hasCalledWaitFrame = true; // XXX this is where hasCalledWaitFrame was updated in old layer :shrug:
            }
            SessionStateTracker::DoCommand(command);
        }
    }

    void IncrementPredictedDisplayTime()
    {
        if(savedFrameState) {
            savedFrameState->predictedDisplayTime += 1; // XXX This is legal, but not really what we want
        }
    }

};

// Bookkeeping of SwapchainImages for copying remote SwapchainImages on ReleaseSwapchainImage
struct SwapchainCachedData
{
    enum {
        KEYED_MUTEX_OVERLAY = 0,
        KEYED_MUTEX_MAIN = 1,
    };

    XrSwapchain swapchain;
    std::vector<ID3D11Texture2D*> swapchainImages;
    std::set<HANDLE> remoteImagesAcquired;
    std::unordered_map<HANDLE, ID3D11Texture2D*> handleTextureMap;
    std::vector<uint32_t>   acquired;

    SwapchainCachedData(XrSwapchain swapchain_, const std::vector<ID3D11Texture2D*>& swapchainImages_) :
        swapchain(swapchain_),
        swapchainImages(swapchainImages_)
    {
        for(auto texture : swapchainImages) {
            texture->AddRef();
        }
    }

    ~SwapchainCachedData();
    ID3D11Texture2D* getSharedTexture(ID3D11Device *d3d11Device, HANDLE sourceHandle);

    typedef std::shared_ptr<SwapchainCachedData> Ptr;
};

struct OverlaysLayerXrSwapchainHandleInfo;

struct MainSessionContext
{
    XrSession session;
    MainSessionSessionState sessionState;
    std::set<std::shared_ptr<OverlaysLayerXrSwapchainHandleInfo>> swapchainsInFlight;

    MainSessionContext(XrSession session) :
        session(session)
    {}

    std::recursive_mutex mutex;
    std::unique_lock<std::recursive_mutex> GetLock()
    {
        return std::unique_lock<std::recursive_mutex>(mutex);
    }

    typedef std::shared_ptr<MainSessionContext> Ptr;
};

typedef std::shared_ptr<XrEventDataBuffer> EventDataBufferPtr;

struct MainAsOverlaySessionContext
{
    uint32_t sessionLayersPlacement;
    // local handles so they can be looked up in our tracking maps
    std::set<XrSpace> localSpaces; // use swapchainMap? 
    std::set<XrSwapchain> localSwapchains;

    SessionStateTracker sessionState;

    constexpr static int maxEventsSavedForOverlay = 16;
    std::queue<EventDataBufferPtr> eventsSaved;

    constexpr static int maxOverlayCompositionLayers = 16;
    std::vector<std::shared_ptr<const XrCompositionLayerBaseHeader>> overlayLayers;

    // This structure needs to be locked because Main could Destroy its
    // shared XrSession and all of its children and that would need to go
    // through here to mark those handles destroyed.
    // It would be smarter to provide accessors that lock.
    // Or perhaps all objects should be shared_ptr so they get deleted thread-safely.

    std::recursive_mutex mutex;
    std::unique_lock<std::recursive_mutex> GetLock()
    {
        return std::unique_lock<std::recursive_mutex>(mutex);
    }

    MainAsOverlaySessionContext(const XrSessionCreateInfoOverlayEXTX* createInfoOverlay) : sessionLayersPlacement(createInfoOverlay->sessionLayersPlacement) {}

    ~MainAsOverlaySessionContext()
    {
        for(auto s: localSpaces) {
            OverlaysLayerRemoveXrSpaceHandleInfo(s);
        }
        for(auto s: localSwapchains) {
            OverlaysLayerRemoveXrSwapchainHandleInfo(s);
        }
    }

    typedef std::shared_ptr<MainAsOverlaySessionContext> Ptr;
};

struct ConnectionToOverlay
{
    bool closed = false;
    std::recursive_mutex mutex;
    RPCChannels conn;
    MainAsOverlaySessionContext::Ptr ctx = nullptr;
    std::thread thread;

    ConnectionToOverlay(const RPCChannels& conn) :
        conn(conn)
    { }

    // This structure probably does not need to be locked.
    std::unique_lock<std::recursive_mutex> GetLock()
    {
        return std::unique_lock<std::recursive_mutex>(mutex);
    }

    ~ConnectionToOverlay()
    {
        // ...
    }

    typedef std::shared_ptr<ConnectionToOverlay> Ptr;
};

struct ConnectionToMain
{
    RPCChannels conn;
    typedef std::shared_ptr<ConnectionToMain> Ptr;
};

extern std::recursive_mutex gSynchronizeEveryProcMutex;
extern bool gSynchronizeEveryProc;

extern std::recursive_mutex gMainSessionContextMutex;
extern MainSessionContext::Ptr gMainSessionContext;

extern ConnectionToMain::Ptr gConnectionToMain;

extern std::recursive_mutex gConnectionsToOverlayByProcessIdMutex;
extern std::unordered_map<DWORD, ConnectionToOverlay::Ptr> gConnectionsToOverlayByProcessId;

constexpr uint32_t gLayerBinaryVersion = 0x00000001;

uint64_t GetNextLocalHandle();

// Local render target for passing to "Swapchain"
struct OverlaySwapchain
{
    XrSwapchain             swapchain;
    std::vector<ID3D11Texture2D*> swapchainTextures;
    std::vector<HANDLE>          swapchainHandles;
    std::vector<uint32_t>   acquired;
    bool                    waited;
    int                     width;
    int                     height;
    DXGI_FORMAT             format;


    OverlaySwapchain(XrSwapchain sc, size_t count, const XrSwapchainCreateInfo* createInfo) :
        swapchain(sc),
        swapchainTextures(count),
        swapchainHandles(count),
        waited(false),
        width(createInfo->width),
        height(createInfo->height),
        format(static_cast<DXGI_FORMAT>(createInfo->format))
    {
    }
    bool CreateTextures(XrInstance instance, ID3D11Device *d3d11, DWORD mainProcessId);
    ~OverlaySwapchain()
    {
        // XXX Need to AcquireSync from remote side?
        for(int i = 0; i < swapchainTextures.size(); i++) {
            swapchainTextures[i]->Release();
        }
    }
    typedef std::shared_ptr<OverlaySwapchain> Ptr;
};


// Serialization helpers ----------------------------------------------------

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
template <typename T>
T* IPCSerialize(IPCBuffer& ipcbuf, IPCHeader* header, const T* p)
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
T* IPCSerialize(IPCBuffer& ipcbuf, IPCHeader* header, const T* p, size_t count)
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
T* IPCSerializeNoCopy(IPCBuffer& ipcbuf, IPCHeader* header, const T* p)
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
T* IPCSerializeNoCopy(IPCBuffer& ipcbuf, IPCHeader* header, const T* p, size_t count)
{
    if(!p)
        return nullptr;

    T* t = new(ipcbuf) T[count];
    if(!t)
        return nullptr;

    return t;
}

XrBaseInStructure* IPCSerialize(XrInstance instance, IPCBuffer& ipcbuf, IPCHeader* header, const XrBaseInStructure* srcbase, CopyType copyType);

XrBaseInStructure* IPCSerialize(XrInstance instance, IPCBuffer& ipcbuf, IPCHeader* header, const XrBaseInStructure* srcbase, CopyType copyType, size_t count);


// Serialization of XR structs ----------------------------------------------

struct OverlaysLayerRPCCreateSession
{
    XrFormFactor                                formFactor;
    const XrInstanceCreateInfo*                 instanceCreateInfo;
    const XrSessionCreateInfo*                  createInfo;
    XrSession*                                  session;
};

template <typename T> 
std::shared_ptr<T> GetSharedCopyHandlesRestored(XrInstance instance, const char *func, const T *obj)
{
    XrBaseInStructure *chainCopy = CopyXrStructChainWithMalloc(instance, obj);
    if(!RestoreActualHandles(instance, chainCopy)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, func,
            OverlaysLayerNoObjectInfo, "FATAL: handles could not be restored.\n");
        throw OverlaysLayerXrException(XR_ERROR_HANDLE_INVALID);
    }
    std::shared_ptr<T> chainPtr(reinterpret_cast<T*>(chainCopy), [instance](const T *p){FreeXrStructChainWithFree(instance, p);});
    return chainPtr;
}

enum ActionBindLocation
{
    BIND_PENDING,
    BOUND_MAIN,
    BOUND_OVERLAY,
};

enum SpaceType
{
    SPACE_REFERENCE,
    SPACE_ACTION,
};

union ActionStateUnion
{
    XrActionStateBoolean booleanState;
    XrActionStateFloat floatState;
    XrActionStateVector2f vector2fState;
    XrActionStatePose poseState;
};

// All applications within an overlay session must use the same version of the overlay layer to ensure compatibility, or the some
// RPC to share the main overlay's enum -> string map must be added.
enum WellKnownStringIndex {
    NULL_PATH = 0,
    INPUT_A_CLICK = 1,
    INPUT_A_TOUCH = 2,
    INPUT_AIM_POSE = 3,
    INPUT_B_CLICK = 4,
    INPUT_B_TOUCH = 5,
    INPUT_BACK_CLICK = 6,
    INPUT_DPAD_DOWN_CLICK = 7,
    INPUT_DPAD_LEFT_CLICK = 8,
    INPUT_DPAD_RIGHT_CLICK = 9,
    INPUT_DPAD_UP_CLICK = 10,
    INPUT_GAZE_EXT_POSE = 11,
    INPUT_GRIP_POSE = 12,
    INPUT_MENU_CLICK = 13,
    INPUT_MUTE_MIC_CLICK = 14,
    INPUT_SELECT_CLICK = 15,
    INPUT_SELECT_VALUE = 16,
    INPUT_SHOULDER_CLICK = 17,
    INPUT_SHOULDER_LEFT_CLICK = 18,
    INPUT_SHOULDER_RIGHT_CLICK = 19,
    INPUT_SQUEEZE_CLICK = 20,
    INPUT_SQUEEZE_FORCE = 21,
    INPUT_SQUEEZE_VALUE = 22,
    INPUT_SYSTEM_CLICK = 23,
    INPUT_SYSTEM_TOUCH = 24,
    INPUT_THUMBREST_TOUCH = 25,
    INPUT_THUMBSTICK = 26,
    INPUT_THUMBSTICK_CLICK = 27,
    INPUT_THUMBSTICK_TOUCH = 28,
    INPUT_THUMBSTICK_X = 29,
    INPUT_THUMBSTICK_Y = 30,
    INPUT_THUMBSTICK_LEFT = 31,
    INPUT_THUMBSTICK_LEFT_CLICK = 32,
    INPUT_THUMBSTICK_LEFT_X = 33,
    INPUT_THUMBSTICK_LEFT_Y = 34,
    INPUT_THUMBSTICK_RIGHT = 35,
    INPUT_THUMBSTICK_RIGHT_CLICK = 36,
    INPUT_THUMBSTICK_RIGHT_X = 37,
    INPUT_THUMBSTICK_RIGHT_Y = 38,
    INPUT_TRACKPAD = 39,
    INPUT_TRACKPAD_CLICK = 40,
    INPUT_TRACKPAD_FORCE = 41,
    INPUT_TRACKPAD_TOUCH = 42,
    INPUT_TRACKPAD_X = 43,
    INPUT_TRACKPAD_Y = 44,
    INPUT_TRIGGER_CLICK = 45,
    INPUT_TRIGGER_TOUCH = 46,
    INPUT_TRIGGER_VALUE = 47,
    INPUT_TRIGGER_LEFT_VALUE = 48,
    INPUT_TRIGGER_RIGHT_VALUE = 49,
    INPUT_VIEW_CLICK = 50,
    INPUT_VOLUME_DOWN_CLICK = 51,
    INPUT_VOLUME_UP_CLICK = 52,
    INPUT_X_CLICK = 53,
    INPUT_X_TOUCH = 54,
    INPUT_Y_CLICK = 55,
    INPUT_Y_TOUCH = 56,
    INTERACTION_PROFILES_EXT_EYE_GAZE_INTERACTION = 57,
    INTERACTION_PROFILES_GOOGLE_DAYDREAM_CONTROLLER = 58,
    INTERACTION_PROFILES_HP_MIXED_REALITY_CONTROLLER = 59,
    INTERACTION_PROFILES_HTC_VIVE_CONTROLLER = 60,
    INTERACTION_PROFILES_HTC_VIVE_COSMOS_CONTROLLER = 61,
    INTERACTION_PROFILES_HTC_VIVE_PRO = 62,
    INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER = 63,
    INTERACTION_PROFILES_MICROSOFT_HAND_INTERACTION = 64,
    INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER = 65,
    INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER = 66,
    INTERACTION_PROFILES_OCULUS_GO_CONTROLLER = 67,
    INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER = 68,
    INTERACTION_PROFILES_SAMSUNG_ODYSSEY_CONTROLLER = 69,
    INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER = 70,
    OUTPUT_HAPTIC = 71,
    OUTPUT_HAPTIC_LEFT = 72,
    OUTPUT_HAPTIC_LEFT_TRIGGER = 73,
    OUTPUT_HAPTIC_RIGHT = 74,
    OUTPUT_HAPTIC_RIGHT_TRIGGER = 75,
    USER_EYES_EXT = 76,
    USER_EYES_EXT_INPUT_GAZE_EXT_POSE = 77,
    USER_GAMEPAD = 78,
    USER_GAMEPAD_INPUT_A_CLICK = 79,
    USER_GAMEPAD_INPUT_B_CLICK = 80,
    USER_GAMEPAD_INPUT_DPAD_DOWN_CLICK = 81,
    USER_GAMEPAD_INPUT_DPAD_LEFT_CLICK = 82,
    USER_GAMEPAD_INPUT_DPAD_RIGHT_CLICK = 83,
    USER_GAMEPAD_INPUT_DPAD_UP_CLICK = 84,
    USER_GAMEPAD_INPUT_MENU_CLICK = 85,
    USER_GAMEPAD_INPUT_SHOULDER_LEFT_CLICK = 86,
    USER_GAMEPAD_INPUT_SHOULDER_RIGHT_CLICK = 87,
    USER_GAMEPAD_INPUT_THUMBSTICK_LEFT = 88,
    USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_CLICK = 89,
    USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_X = 90,
    USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_Y = 91,
    USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT = 92,
    USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_CLICK = 93,
    USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_X = 94,
    USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_Y = 95,
    USER_GAMEPAD_INPUT_TRIGGER_LEFT_VALUE = 96,
    USER_GAMEPAD_INPUT_TRIGGER_RIGHT_VALUE = 97,
    USER_GAMEPAD_INPUT_VIEW_CLICK = 98,
    USER_GAMEPAD_INPUT_X_CLICK = 99,
    USER_GAMEPAD_INPUT_Y_CLICK = 100,
    USER_GAMEPAD_OUTPUT_HAPTIC_LEFT = 101,
    USER_GAMEPAD_OUTPUT_HAPTIC_LEFT_TRIGGER = 102,
    USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT = 103,
    USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT_TRIGGER = 104,
    USER_HAND_LEFT = 105,
    USER_HAND_LEFT_INPUT_A_CLICK = 106,
    USER_HAND_LEFT_INPUT_A_TOUCH = 107,
    USER_HAND_LEFT_INPUT_AIM_POSE = 108,
    USER_HAND_LEFT_INPUT_B_CLICK = 109,
    USER_HAND_LEFT_INPUT_B_TOUCH = 110,
    USER_HAND_LEFT_INPUT_BACK_CLICK = 111,
    USER_HAND_LEFT_INPUT_GRIP_POSE = 112,
    USER_HAND_LEFT_INPUT_MENU_CLICK = 113,
    USER_HAND_LEFT_INPUT_SELECT_CLICK = 114,
    USER_HAND_LEFT_INPUT_SELECT_VALUE = 115,
    USER_HAND_LEFT_INPUT_SHOULDER_CLICK = 116,
    USER_HAND_LEFT_INPUT_SQUEEZE_CLICK = 117,
    USER_HAND_LEFT_INPUT_SQUEEZE_FORCE = 118,
    USER_HAND_LEFT_INPUT_SQUEEZE_VALUE = 119,
    USER_HAND_LEFT_INPUT_SYSTEM_CLICK = 120,
    USER_HAND_LEFT_INPUT_SYSTEM_TOUCH = 121,
    USER_HAND_LEFT_INPUT_THUMBREST_TOUCH = 122,
    USER_HAND_LEFT_INPUT_THUMBSTICK = 123,
    USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK = 124,
    USER_HAND_LEFT_INPUT_THUMBSTICK_TOUCH = 125,
    USER_HAND_LEFT_INPUT_THUMBSTICK_X = 126,
    USER_HAND_LEFT_INPUT_THUMBSTICK_Y = 127,
    USER_HAND_LEFT_INPUT_TRACKPAD = 128,
    USER_HAND_LEFT_INPUT_TRACKPAD_CLICK = 129,
    USER_HAND_LEFT_INPUT_TRACKPAD_FORCE = 130,
    USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH = 131,
    USER_HAND_LEFT_INPUT_TRACKPAD_X = 132,
    USER_HAND_LEFT_INPUT_TRACKPAD_Y = 133,
    USER_HAND_LEFT_INPUT_TRIGGER_CLICK = 134,
    USER_HAND_LEFT_INPUT_TRIGGER_TOUCH = 135,
    USER_HAND_LEFT_INPUT_TRIGGER_VALUE = 136,
    USER_HAND_LEFT_INPUT_X_CLICK = 137,
    USER_HAND_LEFT_INPUT_X_TOUCH = 138,
    USER_HAND_LEFT_INPUT_Y_CLICK = 139,
    USER_HAND_LEFT_INPUT_Y_TOUCH = 140,
    USER_HAND_LEFT_OUTPUT_HAPTIC = 141,
    USER_HAND_RIGHT = 142,
    USER_HAND_RIGHT_INPUT_A_CLICK = 143,
    USER_HAND_RIGHT_INPUT_A_TOUCH = 144,
    USER_HAND_RIGHT_INPUT_AIM_POSE = 145,
    USER_HAND_RIGHT_INPUT_B_CLICK = 146,
    USER_HAND_RIGHT_INPUT_B_TOUCH = 147,
    USER_HAND_RIGHT_INPUT_BACK_CLICK = 148,
    USER_HAND_RIGHT_INPUT_GRIP_POSE = 149,
    USER_HAND_RIGHT_INPUT_MENU_CLICK = 150,
    USER_HAND_RIGHT_INPUT_SELECT_CLICK = 151,
    USER_HAND_RIGHT_INPUT_SELECT_VALUE = 152,
    USER_HAND_RIGHT_INPUT_SHOULDER_CLICK = 153,
    USER_HAND_RIGHT_INPUT_SQUEEZE_CLICK = 154,
    USER_HAND_RIGHT_INPUT_SQUEEZE_FORCE = 155,
    USER_HAND_RIGHT_INPUT_SQUEEZE_VALUE = 156,
    USER_HAND_RIGHT_INPUT_SYSTEM_CLICK = 157,
    USER_HAND_RIGHT_INPUT_SYSTEM_TOUCH = 158,
    USER_HAND_RIGHT_INPUT_THUMBREST_TOUCH = 159,
    USER_HAND_RIGHT_INPUT_THUMBSTICK = 160,
    USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK = 161,
    USER_HAND_RIGHT_INPUT_THUMBSTICK_TOUCH = 162,
    USER_HAND_RIGHT_INPUT_THUMBSTICK_X = 163,
    USER_HAND_RIGHT_INPUT_THUMBSTICK_Y = 164,
    USER_HAND_RIGHT_INPUT_TRACKPAD = 165,
    USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK = 166,
    USER_HAND_RIGHT_INPUT_TRACKPAD_FORCE = 167,
    USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH = 168,
    USER_HAND_RIGHT_INPUT_TRACKPAD_X = 169,
    USER_HAND_RIGHT_INPUT_TRACKPAD_Y = 170,
    USER_HAND_RIGHT_INPUT_TRIGGER_CLICK = 171,
    USER_HAND_RIGHT_INPUT_TRIGGER_TOUCH = 172,
    USER_HAND_RIGHT_INPUT_TRIGGER_VALUE = 173,
    USER_HAND_RIGHT_OUTPUT_HAPTIC = 174,
    USER_HEAD = 175,
    USER_HEAD_INPUT_MUTE_MIC_CLICK = 176,
    USER_HEAD_INPUT_SYSTEM_CLICK = 177,
    USER_HEAD_INPUT_VOLUME_DOWN_CLICK = 178,
    USER_HEAD_INPUT_VOLUME_UP_CLICK = 179,
};

enum ProfileExtensionReqMask {
    REQ_EXTENSION_NONE_MASK = 0,
    REQ_EXT_EYE_GAZE_INTERACTION_BIT = (1 << 0),
    REQ_EXT_HP_MIXED_REALITY_CONTROLLER_BIT = (1 << 1),
    REQ_EXT_SAMSUNG_ODYSSEY_CONTROLLER_BIT = (1 << 2),
    REQ_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_BIT = (1 << 3),
    REQ_MSFT_HAND_INTERACTION_BIT = (1 << 4),
    REQ_INTERACTION_PROFILES_EXT_EYE_GAZE_INTERACTION_MASK = REQ_EXT_EYE_GAZE_INTERACTION_BIT,
    REQ_INTERACTION_PROFILES_GOOGLE_DAYDREAM_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_HP_MIXED_REALITY_CONTROLLER_MASK = REQ_EXT_HP_MIXED_REALITY_CONTROLLER_BIT,
    REQ_INTERACTION_PROFILES_HTC_VIVE_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_HTC_VIVE_COSMOS_CONTROLLER_MASK = REQ_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_BIT,
    REQ_INTERACTION_PROFILES_HTC_VIVE_PRO_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_MICROSOFT_HAND_INTERACTION_MASK = REQ_MSFT_HAND_INTERACTION_BIT,
    REQ_INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_OCULUS_GO_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK,
    REQ_INTERACTION_PROFILES_SAMSUNG_ODYSSEY_CONTROLLER_MASK = REQ_EXT_SAMSUNG_ODYSSEY_CONTROLLER_BIT,
    REQ_INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER_MASK = REQ_EXTENSION_NONE_MASK
};

// Manually written functions -----------------------------------------------

XrResult OverlaysLayerCreateSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrFormFactor formFactor, const XrInstanceCreateInfo *instanceCreateInfo, const XrSessionCreateInfo *createInfo, const XrSessionCreateInfoOverlayEXTX *createInfoOverlay, XrSession *session);
XrResult OverlaysLayerCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);

XrResult OverlaysLayerCreateSwapchainMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain, uint32_t *swapchainCount);
XrResult OverlaysLayerCreateSwapchainOverlay(XrInstance instance, XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);
XrResult OverlaysLayerCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);

XrResult OverlaysLayerDestroySessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session);
XrResult OverlaysLayerDestroySessionOverlay(XrInstance instance, XrSession session);

XrResult OverlaysLayerEnumerateSwapchainFormatsMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats);
XrResult OverlaysLayerEnumerateSwapchainFormatsOverlay(XrInstance instance, XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats);

XrResult OverlaysLayerEnumerateSwapchainImagesOverlay(XrInstance instance, XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);

XrResult OverlaysLayerCreateReferenceSpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);
XrResult OverlaysLayerCreateReferenceSpaceOverlay(XrInstance instance, XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);

XrResult OverlaysLayerPollEventMainAsOverlay(ConnectionToOverlay::Ptr connection, XrEventDataBuffer* eventData);
XrResult OverlaysLayerPollEvent(XrInstance instance, XrEventDataBuffer* eventData);

XrResult OverlaysLayerBeginSessionOverlay(XrInstance instance, XrSession session, const XrSessionBeginInfo* beginInfo);
XrResult OverlaysLayerBeginSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrSessionBeginInfo* beginInfo);

XrResult OverlaysLayerWaitFrameOverlay(XrInstance instance, XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState);
XrResult OverlaysLayerWaitFrameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState);

XrResult OverlaysLayerBeginFrameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrFrameBeginInfo* frameBeginInfo);
XrResult OverlaysLayerBeginFrameOverlay(XrInstance instance, XrSession session, const XrFrameBeginInfo* frameBeginInfo);

XrResult OverlaysLayerAcquireSwapchainImageMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t *index);
XrResult OverlaysLayerAcquireSwapchainImageOverlay(XrInstance instance, XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t *index);

XrResult OverlaysLayerWaitSwapchainImageMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo, HANDLE sourceImage);
XrResult OverlaysLayerWaitSwapchainImageOverlay(XrInstance instance, XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);

XrResult OverlaysLayerReleaseSwapchainImageMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* waitInfo, HANDLE sourceImage);
XrResult OverlaysLayerReleaseSwapchainImageOverlay(XrInstance instance, XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* waitInfo);

XrResult OverlaysLayerEndFrameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrFrameEndInfo* frameEndInfo);
XrResult OverlaysLayerEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo);

XrResult OverlaysLayerEnumerateReferenceSpacesMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces);
XrResult OverlaysLayerEnumerateReferenceSpacesOverlay(XrInstance instance, XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces);

XrResult OverlaysLayerGetReferenceSpaceBoundsRectMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds);
XrResult OverlaysLayerGetReferenceSpaceBoundsRectOverlay(XrInstance, XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds);

XrResult OverlaysLayerLocateViewsMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);
XrResult OverlaysLayerLocateViewsOverlay(XrInstance instance, XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);

XrResult OverlaysLayerLocateSpaceOverlay(XrInstance instance, XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location);
XrResult OverlaysLayerLocateSpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location);

XrResult OverlaysLayerDestroySpaceOverlay(XrInstance instance, XrSpace space);
XrResult OverlaysLayerDestroySpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSpace space);

XrResult OverlaysLayerRequestExitSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session);
XrResult OverlaysLayerRequestExitSessionOverlay(XrInstance instance, XrSession session);

XrResult OverlaysLayerEndSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session);
XrResult OverlaysLayerEndSessionOverlay(XrInstance instance, XrSession session);

XrResult OverlaysLayerDestroySwapchainMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain);
XrResult OverlaysLayerDestroySwapchainOverlay(XrInstance instance, XrSwapchain swapchain);

XrResult OverlaysLayerCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet);
XrResult OverlaysLayerCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action);

XrResult OverlaysLayerSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings);

XrResult OverlaysLayerCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space);

XrResult OverlaysLayerAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo);

XrResult OverlaysLayerSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo);

XrResult OverlaysLayerGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state);
XrResult OverlaysLayerGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state);
XrResult OverlaysLayerGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state);
XrResult OverlaysLayerGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state);

XrResult OverlaysLayerSyncActionsAndGetStateMainAsOverlay(
    ConnectionToOverlay::Ptr connection, XrSession session,
    uint32_t countProfileAndBindings, const WellKnownStringIndex *profileStrings, const WellKnownStringIndex *bindingStrings,   /* input is profiles and bindings for which to Get */
    ActionStateUnion *states,                                                                                                   /* output is result of Get */
    uint32_t countSubactionStrings, const WellKnownStringIndex *subactionStrings,                                               /* input is subactionPaths for which to get current interaction Profile */
    WellKnownStringIndex *interactionProfileStrings);                                                                           /* output is current interaction profiles */

XrResult OverlaysLayerCreateActionSpaceFromBinding(ConnectionToOverlay::Ptr connection, XrSession session, WellKnownStringIndex profileString, WellKnownStringIndex bindingString, const XrPosef* poseInActionSpace, XrSpace *space);

XrResult OverlaysLayerLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location);

XrResult OverlaysLayerStopHapticFeedbackMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t profileStringCount, const WellKnownStringIndex *profileStrings, const WellKnownStringIndex *bindingStrings);
XrResult OverlaysLayerApplyHapticFeedbackMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t profileStringCount, const WellKnownStringIndex *profileStrings, const WellKnownStringIndex *bindingStrings, const XrHapticBaseHeader* hapticFeedback);
XrResult OverlaysLayerApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback);
XrResult OverlaysLayerStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo);

XrResult OverlaysLayerGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile);

XrResult OverlaysLayerLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);

XrResult OverlaysLayerEnumerateBoundSourcesForActionOverlay(XrInstance instance, XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources);
XrResult OverlaysLayerEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId,
                                                     XrViewConfigurationType viewConfigurationType,
                                                     uint32_t environmentBlendModeCapacityInput,
                                                     uint32_t* environmentBlendModeCountOutput,
                                                     XrEnvironmentBlendMode* environmentBlendModes);

XrResult OverlaysLayerGetInputSourceLocalizedNameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo /* sourcePath ignored */, WellKnownStringIndex sourceString, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer);
XrResult OverlaysLayerGetInputSourceLocalizedNameOverlay( XrInstance instance, XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer);

XrResult OverlaysLayerDestroyActionSetMainAsOverlay(ConnectionToOverlay::Ptr connection, XrActionSet actionSet);
XrResult OverlaysLayerDestroyActionSetOverlay(XrInstance instance, XrActionSet actionSet);

XrResult OverlaysLayerDestroyActionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrAction action);
XrResult OverlaysLayerDestroyActionOverlay(XrInstance instance, XrAction action);

#endif /* _OVERLAYS_H_ */
