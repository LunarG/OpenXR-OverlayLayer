// Copyright (c) 2017-2020 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2020 LunarG, Inc.
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
// Author: Mark Young <marky@lunarg.com>
// Author: Dave Houlton <daveh@lunarg.com>
// Author: Brad Grantham <brad@lunarg.com>


#ifndef NOMINMAX
#define NOMINMAX
#endif  // !NOMINMAX

#include "loader_interfaces.h"
#include "platform_utils.hpp"

#include "overlays.h"

#include "xr_generated_overlays.hpp"
#include "xr_generated_dispatch_table.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <dxgi1_2.h>
#include <d3d11_1.h>


#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(_WIN32)
#define LAYER_EXPORT __declspec(dllexport)
#else
#define LAYER_EXPORT
#endif

const char *kOverlayLayerName = "xr_extx_overlay";

const std::set<HandleTypePair> OverlaysLayerNoObjectInfo = {};

uint64_t GetNextLocalHandle()
{
    static std::atomic_uint64_t nextHandle = 1;
    return nextHandle++;
}

void LogWindowsLastError(const char *xrfunc, const char* what, const char *file, int line)
{
    DWORD lastError = GetLastError();
    LPVOID messageBuf;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrfunc,
        OverlaysLayerNoObjectInfo, fmt("FATAL: %s at %s:%d failed with %d (%s)\n", what, file, line, lastError, messageBuf).c_str());
    LocalFree(messageBuf);
}

void LogWindowsError(HRESULT result, const char *xrfunc, const char* what, const char *file, int line)
{
    LPVOID messageBuf;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrfunc,
        OverlaysLayerNoObjectInfo, fmt("FATAL: %s at %s:%d failed with %d (%s)\n", what, file, line, result, messageBuf).c_str());
    LocalFree(messageBuf);
}

bool OverlaySwapchain::CreateTextures(XrInstance instance, ID3D11Device *d3d11, DWORD mainProcessId)
{
    for(int i = 0; i < swapchainTextures.size(); i++) {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        HRESULT result;
        if((result = d3d11->CreateTexture2D(&desc, NULL, &swapchainTextures[i])) != S_OK) {
            LogWindowsError(result, "xrCreateSwapchain", "CreateTexture2D", __FILE__, __LINE__);
            return false;
        }

        {
            IDXGIResource1* sharedResource = NULL;
            if((result = swapchainTextures[i]->QueryInterface(__uuidof(IDXGIResource1), (LPVOID*) &sharedResource)) != S_OK) {
                LogWindowsError(result, "xrCreateSwapchain", "QueryInterface", __FILE__, __LINE__);
                return false;
            }

            HANDLE thisProcessHandle = GetCurrentProcess();
            HANDLE hostProcessHandle;
            hostProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, mainProcessId);
            if(hostProcessHandle == NULL) {
                LogWindowsLastError("xrCreateSwapchain", "OpenProcess", __FILE__, __LINE__);
                return false;
            }

            HANDLE handle;

            // Get the Shared Handle for the texture. This is still local to this process but is an actual HANDLE
            if((result = sharedResource->CreateSharedHandle(NULL,
                DXGI_SHARED_RESOURCE_READ, // GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                NULL, &handle)) != S_OK) {

                LogWindowsError(result, "xrCreateSwapchain", "CreateSharedHandle", __FILE__, __LINE__);
                return false;
            }
            
            // Duplicate the handle so "Host" RPC service process can use it
            if(!DuplicateHandle(thisProcessHandle, handle, hostProcessHandle, &swapchainHandles[i], 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                LogWindowsLastError("xrCreateSwapchain", "DuplicateHandle", __FILE__, __LINE__);
                return false;
            }
            CloseHandle(handle);
            sharedResource->Release();
        }
    }
    return true;
}

OptionalSessionStateChange SessionStateTracker::GetAndDoPendingStateChange(MainSessionSessionState *mainState)
{
    if((sessionState != XR_SESSION_STATE_LOSS_PENDING) &&
        ((mainState->GetLossState() == LOST) ||
        (mainState->GetLossState() == LOSS_PENDING))) {

        return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_LOSS_PENDING };
    }

    switch(sessionState) {
        case XR_SESSION_STATE_UNKNOWN:
            if(mainState->sessionState != XR_SESSION_STATE_UNKNOWN) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_IDLE };
            }
            break;

        case XR_SESSION_STATE_IDLE:
            if(exitRequested || (mainState->sessionState == XR_SESSION_STATE_EXITING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_EXITING };
            } else if(mainState->isRunning && mainState->hasCalledWaitFrame) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_READY };
            }
            break;

        case XR_SESSION_STATE_READY:
            if(isRunning) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } 
            break;

        case XR_SESSION_STATE_SYNCHRONIZED:
            if(exitRequested || !mainState->isRunning || (mainState->sessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_STOPPING };
            } else if((mainState->sessionState == XR_SESSION_STATE_VISIBLE) || (mainState->sessionState == XR_SESSION_STATE_FOCUSED)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_VISIBLE };
            } 
            break;

        case XR_SESSION_STATE_VISIBLE:
            if(exitRequested || !mainState->isRunning || (mainState->sessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } else if(mainState->sessionState == XR_SESSION_STATE_SYNCHRONIZED) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } else if(mainState->sessionState == XR_SESSION_STATE_FOCUSED) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_FOCUSED };
            }
            break;

        case XR_SESSION_STATE_FOCUSED:
            if(exitRequested || !mainState->isRunning || (mainState->sessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_VISIBLE };
            } else if((mainState->sessionState == XR_SESSION_STATE_VISIBLE) || (mainState->sessionState == XR_SESSION_STATE_SYNCHRONIZED)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_VISIBLE };
            }
            break;

        case XR_SESSION_STATE_STOPPING:
            if(!isRunning) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_IDLE };
            }
            break;

        default:
            // No other combination of states requires an Overlay SessionStateChange
            break;
    }

    return OptionalSessionStateChange { false, XR_SESSION_STATE_UNKNOWN };
}


SwapchainCachedData::~SwapchainCachedData()
{
    for(HANDLE acquired : remoteImagesAcquired) {
        IDXGIKeyedMutex* keyedMutex;
        ID3D11Texture2D *sharedTexture;
        auto it = handleTextureMap.find(acquired);
        if(it != handleTextureMap.end()) {
            sharedTexture = it->second;
            HRESULT result = sharedTexture->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex);
            if(result == S_OK) {
                keyedMutex->ReleaseSync(KEYED_MUTEX_OVERLAY);
                keyedMutex->Release();
            }
        }
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

ID3D11Texture2D* SwapchainCachedData::getSharedTexture(ID3D11Device *d3d11Device, HANDLE sourceHandle)
{
    ID3D11Texture2D *sharedTexture;

    ID3D11Device1 *device1;

    HRESULT result;

    if((result = d3d11Device->QueryInterface(__uuidof (ID3D11Device1), (void **)&device1)) != S_OK) {
        LogWindowsError(result, "xrCreateSwapchain", "QueryInterface", __FILE__, __LINE__);
        return nullptr;
    }

    auto it = handleTextureMap.find(sourceHandle);
    if(it == handleTextureMap.end()) {
        if((result = device1->OpenSharedResource1(sourceHandle, __uuidof(ID3D11Texture2D), (LPVOID*) &sharedTexture)) != S_OK) {
            LogWindowsError(result, "xrCreateSwapchain", "OpenSharedResource1", __FILE__, __LINE__);
            return nullptr;
        }
        handleTextureMap[sourceHandle] = sharedTexture;
    } else  {
        sharedTexture = it->second;
    }
    device1->Release();

    return sharedTexture;
}


// XXX could generate
bool OverlaysLayerRemoveXrSpaceHandleInfo(XrSpace localHandle)
{
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
    auto it = gOverlaysLayerXrSpaceToHandleInfo.find(localHandle);
    if(it == gOverlaysLayerXrSpaceToHandleInfo.end()) {
        return false;
    }
    gOverlaysLayerXrSpaceToHandleInfo.erase(localHandle);
    return true;
}

// XXX could generate
bool OverlaysLayerRemoveXrSwapchainHandleInfo(XrSwapchain localHandle)
{
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSwapchainToHandleInfoMutex);
    auto it = gOverlaysLayerXrSwapchainToHandleInfo.find(localHandle);
    if(it != gOverlaysLayerXrSwapchainToHandleInfo.end()) {
        return false;
    }
    gOverlaysLayerXrSwapchainToHandleInfo.erase(localHandle);
    return true;
}

void OverlaysLayerLogMessage(XrInstance instance,
                         XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message)
{
    // If we have instance information, see if we need to log this information out to a debug messenger
    // callback.
    if(instance != XR_NULL_HANDLE) {

        OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = gOverlaysLayerXrInstanceToHandleInfo[instance];

        // To be a little more performant, check all messenger's
        // messageSeverities and messageTypes to make sure we will call at
        // least one

        /* XXX TBD !instanceInfo->debug_data.Empty() */

        if (!instanceInfo->debugUtilsMessengers.empty()) {

            // Setup our callback data once
            XrDebugUtilsMessengerCallbackDataEXT callback_data = {};
            callback_data.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
            callback_data.messageId = "Overlays API Layer";
            callback_data.functionName = command_name;
            callback_data.message = message;

#if 0
            // TBD
            NamesAndLabels names_and_labels;
            std::vector<XrSdkLogObjectInfo> objects;
            objects.reserve(objects_info.size());
            std::transform(objects_info.begin(), objects_info.end(), std::back_inserter(objects),
                           [](GenValidUsageXrObjectInfo const &info) {
                               return XrSdkLogObjectInfo{info.handle, info.type};
                           });
            names_and_labels = instance_info->debug_data.PopulateNamesAndLabels(std::move(objects));
            names_and_labels.PopulateCallbackData(callback_data);
#endif

            // Loop through all active messengers and give each a chance to output information
            for (const auto &messenger : instanceInfo->debugUtilsMessengers) {

                std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
                XrDebugUtilsMessengerCreateInfoEXT *messenger_create_info = gOverlaysLayerXrDebugUtilsMessengerEXTToHandleInfo[messenger]->createInfo;
                mlock.unlock();

                // If a callback exists, and the message is of a type this callback cares about, call it.
                if (nullptr != messenger_create_info->userCallback &&
                    0 != (messenger_create_info->messageSeverities & message_severity) &&
                    0 != (messenger_create_info->messageTypes & XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)) {

                    XrBool32 ret_val = messenger_create_info->userCallback(message_severity,
                                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                                                           &callback_data, messenger_create_info->userData);
                }
            }
        } else {
            if(command_name) {
                OutputDebugStringA(fmt("Overlays API Layer: %s, %s\n", command_name, message).c_str());
            } else {
                OutputDebugStringA(fmt("Overlays API Layer: %s\n", message).c_str());
            }
        }
    } else {
        if(command_name) {
            OutputDebugStringA(fmt("Overlays API Layer: %s, %s\n", command_name, message).c_str());
        } else {
            OutputDebugStringA(fmt("Overlays API Layer: %s\n", message).c_str());
        }
    }
}

void OverlaysLayerLogMessage(XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message)
{
    OverlaysLayerLogMessage(XR_NULL_HANDLE, message_severity, command_name, objects_info, message);
}



XrResult OverlaysLayerXrCreateInstance(const XrInstanceCreateInfo * /*info*/, XrInstance * /*instance*/)
{
    return XR_SUCCESS;
}


XrResult OverlaysLayerXrCreateApiLayerInstance(const XrInstanceCreateInfo *instanceCreateInfo,
        const struct XrApiLayerCreateInfo *apiLayerInfo, XrInstance *instance)
{
    PFN_xrGetInstanceProcAddr next_get_instance_proc_addr = nullptr;
    PFN_xrCreateApiLayerInstance next_create_api_layer_instance = nullptr;
    XrApiLayerCreateInfo new_api_layer_info = {};

    // Validate the API layer info and next API layer info structures before we try to use them
    if (!apiLayerInfo ||
        XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO != apiLayerInfo->structType ||
        XR_API_LAYER_CREATE_INFO_STRUCT_VERSION > apiLayerInfo->structVersion ||
        sizeof(XrApiLayerCreateInfo) > apiLayerInfo->structSize ||
        !apiLayerInfo->nextInfo ||
        XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO != apiLayerInfo->nextInfo->structType ||
        XR_API_LAYER_NEXT_INFO_STRUCT_VERSION > apiLayerInfo->nextInfo->structVersion ||
        sizeof(XrApiLayerNextInfo) > apiLayerInfo->nextInfo->structSize ||
        0 != strcmp(kOverlayLayerName, apiLayerInfo->nextInfo->layerName) ||
        nullptr == apiLayerInfo->nextInfo->nextGetInstanceProcAddr ||
        nullptr == apiLayerInfo->nextInfo->nextCreateApiLayerInstance) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Copy the contents of the layer info struct, but then move the next info up by
    // one slot so that the next layer gets information.
    memcpy(&new_api_layer_info, apiLayerInfo, sizeof(XrApiLayerCreateInfo));
    new_api_layer_info.nextInfo = apiLayerInfo->nextInfo->next;

    // Remove XR_EXTX_overlay from the extension list if requested
    const char** extensionNamesMinusOverlay = new const char*[instanceCreateInfo->enabledExtensionCount];
    uint32_t extensionCountMinusOverlay = 0;
    for(uint32_t i = 0; i < instanceCreateInfo->enabledExtensionCount; i++) {
        if(strncmp(instanceCreateInfo->enabledExtensionNames[i], XR_EXTX_OVERLAY_EXTENSION_NAME, strlen(XR_EXTX_OVERLAY_EXTENSION_NAME)) != 0) {
            extensionNamesMinusOverlay[extensionCountMinusOverlay++] = instanceCreateInfo->enabledExtensionNames[i];
        }
    }

    XrInstanceCreateInfo createInfoMinusOverlays = *instanceCreateInfo;
    createInfoMinusOverlays.enabledExtensionNames = extensionNamesMinusOverlay;
    createInfoMinusOverlays.enabledExtensionCount = extensionCountMinusOverlay;

    // Get the function pointers we need
    next_get_instance_proc_addr = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;
    next_create_api_layer_instance = apiLayerInfo->nextInfo->nextCreateApiLayerInstance;

    // Create the instance
    XrInstance returned_instance = *instance;
    XrResult result = next_create_api_layer_instance(&createInfoMinusOverlays, &new_api_layer_info, &returned_instance);
    *instance = returned_instance;

    delete[] extensionNamesMinusOverlay;

    // Create the dispatch table to the next levels
    std::shared_ptr<XrGeneratedDispatchTable> next_dispatch = std::make_shared<XrGeneratedDispatchTable>();
    GeneratedXrPopulateDispatchTable(next_dispatch.get(), returned_instance, next_get_instance_proc_addr);

    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
    OverlaysLayerXrInstanceHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrInstanceHandleInfo>(next_dispatch);
    info->createInfo = reinterpret_cast<XrInstanceCreateInfo*>(CopyXrStructChainWithMalloc(*instance, instanceCreateInfo));
    gOverlaysLayerXrInstanceToHandleInfo[*instance] = info;

    return result;
}

XrResult OverlaysLayerXrDestroyInstance(XrInstance instance)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = gOverlaysLayerXrInstanceToHandleInfo[instance];
    std::shared_ptr<XrGeneratedDispatchTable> next_dispatch = instanceInfo->downchain;
    instanceInfo->Destroy();
    mlock.unlock();

    next_dispatch->DestroyInstance(instance);

    return XR_SUCCESS;
}

std::recursive_mutex gActualSessionToLocalHandleMutex;
std::unordered_map<XrSession, XrSession> gActualSessionToLocalHandle;

NegotiationChannels gNegotiationChannels;

bool gAppHasAMainSession = false;
XrInstance gMainSessionInstance;
MainSessionContext::Ptr gMainSessionContext;
DWORD gMainProcessId;   // Set by Overlay to check for main process unexpected exit
HANDLE gMainMutexHandle; // Held by Main for duration of operation as Main Session

// Both main and overlay processes call this function, which creates/opens
// the negotiation mutex, shmem, and semaphores.
bool OpenNegotiationChannels(XrInstance instance, NegotiationChannels &ch)
{
    ch.instance = instance;
    ch.mutexHandle = CreateMutexA(NULL, TRUE, NegotiationChannels::mutexName);
    if (ch.mutexHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the negotiation mutex: CreateMutex error was %d (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.shmemHandle = CreateFileMappingA( 
        INVALID_HANDLE_VALUE,   // use sys paging file instead of an existing file
        NULL,                   // default security attributes
        PAGE_READWRITE,         // read/write access
        0,                      // size: high 32-bits
        NegotiationChannels::shmemSize,         // size: low 32-bits
        NegotiationChannels::shmemName);        // name of map object

    if (ch.shmemHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the negotiation shmem: CreateFileMappingA error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    // Get a pointer to the file-mapped shared memory, read/write
    ch.params = reinterpret_cast<NegotiationParams*>(MapViewOfFile(ch.shmemHandle, FILE_MAP_WRITE, 0, 0, 0));
    if (!ch.params) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not get the negotiation shmem: MapViewOfFile error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    ch.overlayWaitSema = CreateSemaphoreA(nullptr, 0, 1, NegotiationChannels::overlayWaitSemaName);
    if(ch.overlayWaitSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create negotiation overlay wait sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.mainWaitSema = CreateSemaphoreA(nullptr, 0, 1, NegotiationChannels::mainWaitSemaName);
    if(ch.mainWaitSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create negotiation main wait sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    return true;
}

bool OpenRPCChannels(XrInstance instance, DWORD otherProcessId, DWORD overlayId, RPCChannels& ch)
{
    ch.instance = instance;

    ch.otherProcessId = otherProcessId;
    ch.otherProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, ch.otherProcessId);

    ch.mutexHandle = CreateMutexA(NULL, TRUE, fmt(RPCChannels::mutexNameTemplate, overlayId).c_str());
    if (ch.mutexHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "no function", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the RPC mutex: CreateMutex error was %d (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.shmemHandle = CreateFileMappingA( 
        INVALID_HANDLE_VALUE,   // use sys paging file instead of an existing file
        NULL,                   // default security attributes
        PAGE_READWRITE,         // read/write access
        0,                      // size: high 32-bits
        RPCChannels::shmemSize,         // size: low 32-bits
        fmt(RPCChannels::shmemNameTemplate, overlayId).c_str());        // name of map object

    if (ch.shmemHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "no function", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the RPC shmem: CreateFileMappingA error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    // Get a pointer to the file-mapped shared memory, read/write
    ch.shmem = MapViewOfFile(ch.shmemHandle, FILE_MAP_WRITE, 0, 0, 0);
    if (ch.shmem == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not get the RPC shmem: MapViewOfFile error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    ch.overlayRequestSema = CreateSemaphoreA(nullptr, 0, 1, fmt(RPCChannels::overlayRequestSemaNameTemplate, overlayId).c_str());
    if(ch.overlayRequestSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create RPC overlay request sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.mainResponseSema = CreateSemaphoreA(nullptr, 0, 1, fmt(RPCChannels::mainResponseSemaNameTemplate, overlayId).c_str());
    if(ch.mainResponseSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create RPC main response sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    return true;
}


std::unordered_map<DWORD, std::shared_ptr<ConnectionToOverlay>> gConnectionsToOverlayByProcessId;
std::recursive_mutex gConnectionsToOverlayByProcessIdMutex;


XrBaseInStructure* IPCSerialize(XrInstance instance, IPCBuffer& ipcbuf, IPCHeader* header, const XrBaseInStructure* srcbase, CopyType copyType)
{
    return CopyXrStructChain(instance, srcbase, copyType,
            [&ipcbuf](size_t size){return ipcbuf.allocate(size);},
            [&ipcbuf,&header](void* pointerToPointer){header->addOffsetToPointer(ipcbuf.base, pointerToPointer);});
}


// CopyOut XR structs -------------------------------------------------------

void IPCCopyOut(XrBaseOutStructure* dstbase, const XrBaseOutStructure* srcbase)
{
    bool skipped = true;

    do {
        skipped = false;

        if(!srcbase) {
            return;
        }

        switch(dstbase->type) {
            case XR_TYPE_SPACE_LOCATION: {
                auto src = reinterpret_cast<const XrSpaceLocation*>(srcbase);
                auto dst = reinterpret_cast<XrSpaceLocation*>(dstbase);
                dst->locationFlags = src->locationFlags;
                dst->pose = src->pose;
                break;
            }

            case XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR: {
                auto src = reinterpret_cast<const XrGraphicsRequirementsD3D11KHR*>(srcbase);
                auto dst = reinterpret_cast<XrGraphicsRequirementsD3D11KHR*>(dstbase);
                dst->adapterLuid = src->adapterLuid;
                dst->minFeatureLevel = src->minFeatureLevel;
                break;
            }

            case XR_TYPE_FRAME_STATE: {
                auto src = reinterpret_cast<const XrFrameState*>(srcbase);
                auto dst = reinterpret_cast<XrFrameState*>(dstbase);
                dst->predictedDisplayTime = src->predictedDisplayTime;
                dst->predictedDisplayPeriod = src->predictedDisplayPeriod;
                dst->shouldRender = src->shouldRender;
                break;
            }

            case XR_TYPE_INSTANCE_PROPERTIES: {
                auto src = reinterpret_cast<const XrInstanceProperties*>(srcbase);
                auto dst = reinterpret_cast<XrInstanceProperties*>(dstbase);
                dst->runtimeVersion = src->runtimeVersion;
                strncpy_s(dst->runtimeName, src->runtimeName, XR_MAX_RUNTIME_NAME_SIZE);
                break;
            }

            case XR_TYPE_EXTENSION_PROPERTIES: {
                auto src = reinterpret_cast<const XrExtensionProperties*>(srcbase);
                auto dst = reinterpret_cast<XrExtensionProperties*>(dstbase);
                strncpy_s(dst->extensionName, src->extensionName, XR_MAX_EXTENSION_NAME_SIZE);
                dst->extensionVersion = src->extensionVersion;
                break;
            }

            case XR_TYPE_SYSTEM_PROPERTIES: {
                auto src = reinterpret_cast<const XrSystemProperties*>(srcbase);
                auto dst = reinterpret_cast<XrSystemProperties*>(dstbase);
                dst->systemId = src->systemId;
                dst->vendorId = src->vendorId;
                dst->graphicsProperties = src->graphicsProperties;
                dst->trackingProperties = src->trackingProperties;
                strncpy_s(dst->systemName, src->systemName, XR_MAX_SYSTEM_NAME_SIZE);
                break;
            }

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
                OverlaysLayerLogMessage(XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "unknown",
                    OverlaysLayerNoObjectInfo, fmt("WARNING: IPCCopyOut called to copy out to %p of unknown type %d - skipped.\n", dstbase, dstbase->type).c_str());

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

template <>
OverlaysLayerRPCCreateSession* IPCSerialize(IPCBuffer& ipcbuf, IPCHeader* header, const OverlaysLayerRPCCreateSession* src)
{
    auto dst = new(ipcbuf) OverlaysLayerRPCCreateSession;

    dst->formFactor = src->formFactor;

    dst->instanceCreateInfo = reinterpret_cast<const XrInstanceCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->instanceCreateInfo), COPY_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->instanceCreateInfo);
    dst->createInfo = reinterpret_cast<const XrSessionCreateInfo*>(IPCSerialize(ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->createInfo), COPY_EVERYTHING));
    header->addOffsetToPointer(ipcbuf.base, &dst->createInfo);

    dst->session = IPCSerializeNoCopy(ipcbuf, header, src->session);
    header->addOffsetToPointer(ipcbuf.base, &dst->session);

    return dst;
}

void IPCCopyOut(OverlaysLayerRPCCreateSession* dst, const OverlaysLayerRPCCreateSession* src)
{
    dst->session = src->session;
}


template <class T>
const T* FindStructInChain(const void *head, XrStructureType type)
{
    const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(head);
    while(p) {
        if(p->type == type) {
            return reinterpret_cast<const T*>(p);
        }
		p = p->next;
    }
    return nullptr;
}

bool FindExtensionInList(const char* extension, uint32_t extensionsCount, const char * const* extensions)
{
    for(uint32_t i = 0; i < extensionsCount; i++) {
        if(strcmp(extension, extensions[i]) == 0) {
            return true;
        }
    }
    return false;
}

XrResult OverlaysLayerCreateSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrFormFactor formFactor, const XrInstanceCreateInfo *instanceCreateInfo, const XrSessionCreateInfo *createInfo, XrSession *session)
{
    auto mainSession = gMainSessionContext;
    auto l = mainSession->GetLock();
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[mainSession->session];

    if(createInfo->createFlags != sessionInfo->createInfo->createFlags) {
        OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("WARNING: xrCreateSession for overlay session had different flags (%08X) than those with which main session was created (%08X). Effect is unknown, proceeding anyway.\n", createInfo->createFlags, sessionInfo->createInfo->createFlags).c_str());
    }

    // Verify that any structures match that are chained off createInfo
    for(const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next); p; p = reinterpret_cast<const XrBaseInStructure*>(p->next)) {
        switch(p->type) {

            case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
                const XrGraphicsBindingD3D11KHR* d3dbinding = reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(p);
                const XrGraphicsBindingD3D11KHR* other = FindStructInChain< XrGraphicsBindingD3D11KHR>(sessionInfo->createInfo->next, p->type);

                if(!other) {
                    // XXX send directly to channels somehow
                    OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
                        OverlaysLayerNoObjectInfo, "FATAL: xrCreateSession for overlay session specified a XrGraphicsBindingD3D11KHR but main session did not.\n");
                    return XR_ERROR_INITIALIZATION_FAILED;
                }

                if(other->device != d3dbinding->device) {
                    OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                        OverlaysLayerNoObjectInfo, fmt("WARNING: xrCreateSession for overlay session used different D3D11 Device (%08X) than that with which main session was created (%08X). Effect is unknown, proceeding anyway.\n", d3dbinding->device, other->device).c_str());
                }
                break;
            }

            // XXX Check out all other GAPI structs as support for them is added

            case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX:
                // This is fine, ignore.  We could probably remove it on the Overlay side and not pass it through...
                break;

            default: {
                char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
                XrResult r = sessionInfo->downchain->StructureTypeToString(gMainSessionInstance, p->type, structureTypeName);
                if(r != XR_SUCCESS) {
                    sprintf(structureTypeName, "(type %08X)", p->type);
                }

                OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
                    OverlaysLayerNoObjectInfo, fmt("FATAL: xrCreateSession for an overlay session used a struct (%s) which the Overlay API Layer does not know how to check.\n", structureTypeName).c_str());
                return XR_ERROR_INITIALIZATION_FAILED;
                break;
            }
        }
    }

    // Verify that main session didn't have any unexpected structures that the overlay didn't have
    for(const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next); p; p = reinterpret_cast<const XrBaseInStructure*>(p->next)) {
        // XXX this should probably just reject any structure that isn't known.
        const XrBaseInStructure* other = FindStructInChain<XrBaseInStructure>(sessionInfo->createInfo->next, p->type);
        if(!other) {
            char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
            XrResult r = sessionInfo->downchain->StructureTypeToString(gMainSessionInstance, p->type, structureTypeName);
            if(r != XR_SUCCESS) {
                sprintf(structureTypeName, "(type %08X)", p->type);
            }

            OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                OverlaysLayerNoObjectInfo, fmt("WARNING: xrCreateSession for the main session used a struct (%s) which the overlay session did not. Effect is unknown, proceeding anyway.\n", structureTypeName).c_str());
        }
    }
    
    XrFormFactor mainSessionFormFactor;
    {
        std::unique_lock<std::recursive_mutex> m(gOverlaysLayerXrSessionToHandleInfoMutex);
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[mainSession->session];
        XrSystemId systemId = sessionInfo->createInfo->systemId;

        std::unique_lock<std::recursive_mutex> m2(gOverlaysLayerSystemIdToAtomInfoMutex);
        const XrSystemGetInfo* systemGetInfo = gOverlaysLayerSystemIdToAtomInfo[systemId]->getInfo;
        mainSessionFormFactor = systemGetInfo->formFactor;
    }
    if(mainSessionFormFactor != formFactor) {
        OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: xrCreateSession for the overlay session used an XrFormFactor (%s) which the main session did not.\n", formFactor).c_str());
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    bool didntFindExtension = false;
    {
        std::unique_lock<std::recursive_mutex> m(gOverlaysLayerXrInstanceToHandleInfoMutex);
        OverlaysLayerXrInstanceHandleInfo::Ptr mainInstanceInfo = gOverlaysLayerXrInstanceToHandleInfo[gMainSessionInstance];
        const XrInstanceCreateInfo* mainInstanceCreateInfo = mainInstanceInfo->createInfo;
        for(uint32_t i = 0; i < instanceCreateInfo->enabledExtensionCount; i++) {
            bool alsoInMain = FindExtensionInList(instanceCreateInfo->enabledExtensionNames[i], mainInstanceCreateInfo->enabledExtensionCount, mainInstanceCreateInfo->enabledExtensionNames);
            if(!alsoInMain) {
                OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                    OverlaysLayerNoObjectInfo, fmt("FATAL: xrCreateInstance for the parent of the overlay session specified an extension (%s) which the main session did not.\n", instanceCreateInfo->enabledExtensionNames[i]).c_str());
                didntFindExtension = true;
            }
        }
    }
    if(didntFindExtension) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    {
        auto l = connection->GetLock();
        connection->ctx = std::make_shared<MainAsOverlaySessionContext>();
    }
    // XXX create stand in for Main managing all Overlay app's stuff (in case Overlay unexpectedly exits); OverlaySessionContext

    // XXX should gMainSession be the downchain session so MainAsOverlay can use it directly?
    // Then looking it up in ...ToHandleInfo requires mapping backward to the local handle
    *session = mainSession->session;

    return XR_SUCCESS;
}


void MainRPCThreadBody(ConnectionToOverlay::Ptr connection, DWORD overlayProcessId)
{
    auto l = connection->GetLock();
    RPCChannels rpc = connection->conn;
    l.unlock();

    bool connectionLost = false;

    do {
        RPCChannels::WaitResult result = rpc.WaitForOverlayRequestOrFail();

        if(result == RPCChannels::WaitResult::OVERLAY_PROCESS_TERMINATED_UNEXPECTEDLY) {

            OutputDebugStringA("**OVERLAY** other process terminated\n");
#if 0 // XXX this should happen through graceful dtor on connection
            if(gMainSession && gMainSession->overlaySession) { // Might have LOST SESSION
                ScopedMutex scopedMutex(gOverlayCallMutex, INFINITE, "overlay layer command mutex", __FILE__, __LINE__);
                gMainSession->overlaySession->swapchainMap.clear();
                gMainSession->ClearOverlayLayers();
            }
#endif
            connectionLost = true;

        } else if(result == RPCChannels::WaitResult::WAIT_ERROR) {

            OutputDebugStringA("**OVERLAY** IPC Wait Error\n");
            DebugBreak();
#if 0 // XXX
            if(gMainSession && gMainSession->overlaySession) { // Might have LOST SESSION
                ScopedMutex scopedMutex(gOverlayCallMutex, INFINITE, "overlay layer command mutex", __FILE__, __LINE__);
                gMainSession->overlaySession->swapchainMap.clear();
                gMainSession->ClearOverlayLayers();
            }
#endif
            connectionLost = true;

        } else {

            IPCBuffer ipcbuf = rpc.GetIPCBuffer();
            IPCHeader *hdr = ipcbuf.getAndAdvance<IPCHeader>();

            hdr->makePointersAbsolute(ipcbuf.base);

            bool success = ProcessOverlayRequestOrReturnConnectionLost(connection, ipcbuf, hdr);

            if(success) {
                hdr->makePointersRelative(ipcbuf.base);
                rpc.FinishMainResponse();
            } else {
                connectionLost = true;
            }
        }

#if 0 // XXX
        if(connectionLost && gMainSession && gMainSession->overlaySession) {
            gMainSession->DestroyOverlaySession();
        }
#endif

    } while(!connectionLost && !connection->closed);

    {
        std::unique_lock<std::recursive_mutex> m(gConnectionsToOverlayByProcessIdMutex);
        gConnectionsToOverlayByProcessId.erase(connection->conn.otherProcessId);
    }
}

void MainNegotiateThreadBody()
{
    DWORD result;
    HANDLE handles[2];
    handles[0] = gNegotiationChannels.mainNegotiateThreadStop;
    handles[1] = gNegotiationChannels.mainWaitSema;

    while(1) {
        // Signal that one overlay app may attempt to connect
        ReleaseSemaphore(gNegotiationChannels.overlayWaitSema, 1, nullptr);

        do {
            result = WaitForMultipleObjects(2, handles, FALSE, NegotiationChannels::negotiationWaitMillis);
        } while(result == WAIT_TIMEOUT);

        if(result == WAIT_OBJECT_0 + 0) {

            // Main process has signaled us to stop, probably Session was destroyed.
            return;

        } else if(result != WAIT_OBJECT_0 + 1) {

            // WAIT_FAILED
            DWORD lastError = GetLastError();
            LPVOID messageBuf;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
            OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "no function", 
                OverlaysLayerNoObjectInfo, fmt("FATAL: Could not wait on negotiation sema sema: WaitForMultipleObjects error was %08X (%s)\n", lastError, messageBuf).c_str());
            // XXX need way to signal main process that thread errored unexpectedly
            LocalFree(messageBuf);
            return;
        }

        if(gNegotiationChannels.params->status != NegotiationParams::SUCCESS) {

            OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "no function",
                OverlaysLayerNoObjectInfo, fmt("WARNING: the Overlay API Layer in the overlay app has a different version (%u) than in the main app (%u), connection rejected.\n", gNegotiationChannels.params->overlayLayerBinaryVersion, gNegotiationChannels.params->mainLayerBinaryVersion).c_str());

        } else {

            DWORD overlayProcessId = gNegotiationChannels.params->overlayProcessId;
            RPCChannels channels;

            if(!OpenRPCChannels(gNegotiationChannels.instance, overlayProcessId, overlayProcessId, channels)) {

                OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "no function",
                    OverlaysLayerNoObjectInfo, fmt("WARNING: couldn't open RPC channels to overlay app, connection rejected.\n").c_str());

            } else {
                ConnectionToOverlay::Ptr connection = std::make_shared<ConnectionToOverlay>(channels);

                {
                    std::unique_lock<std::recursive_mutex> m(gConnectionsToOverlayByProcessIdMutex);
                    gConnectionsToOverlayByProcessId[overlayProcessId] = connection;
                }

                std::thread receiverThread(MainRPCThreadBody, connection, overlayProcessId);

                {
                    auto l = connection->GetLock();
                    connection->thread = std::move(receiverThread);
					connection->thread.detach();
                }
            }
        }
    }
}

bool CreateMainSessionNegotiateThread(XrInstance instance, XrSession hostingSession)
{
    gMainSessionInstance = instance;
    gMainSessionContext = std::make_shared<MainSessionContext>(hostingSession);
    if(!OpenNegotiationChannels(instance, gNegotiationChannels)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create overlays negotiation channels\n").c_str());
        return false;
    }

    DWORD waitresult = WaitForSingleObject(gMainMutexHandle, NegotiationChannels::mutexWaitMillis);
    if (waitresult == WAIT_TIMEOUT) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not take main mutex sema; is there another main app running?\n").c_str());
        return false;
    }

    gNegotiationChannels.params->mainProcessId = GetCurrentProcessId();
    gNegotiationChannels.params->mainLayerBinaryVersion = gLayerBinaryVersion;
    gNegotiationChannels.mainNegotiateThreadStop = CreateEventA(nullptr, false, false, nullptr);
    gNegotiationChannels.mainThread = std::thread(MainNegotiateThreadBody);
    gNegotiationChannels.mainThread.detach();

    return true;
}

ConnectionToMain::Ptr gConnectionToMain;

XrResult OverlaysLayerCreateSessionMain(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = gOverlaysLayerXrInstanceToHandleInfo[instance];
    mlock.unlock();

    XrResult xrresult = instanceInfo->downchain->CreateSession(instance, createInfo, session);

    // XXX create unique local id, place as that instead of created handle
    XrSession actualHandle = *session;
    XrSession localHandle = (XrSession)GetNextLocalHandle();
    *session = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualSessionToLocalHandleMutex);
        gActualSessionToLocalHandle[actualHandle] = localHandle;
    }

    OverlaysLayerXrSessionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrSessionHandleInfo>(instance, instance, instanceInfo->downchain);
    info->createInfo = reinterpret_cast<XrSessionCreateInfo*>(CopyXrStructChainWithMalloc(instance, createInfo));
    info->actualHandle = actualHandle;
    info->isProxied = false;

    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSessionToHandleInfoMutex);
    gOverlaysLayerXrSessionToHandleInfo[localHandle] = info;
    mlock2.unlock();

    gAppHasAMainSession = true;

    bool result = CreateMainSessionNegotiateThread(instance, localHandle);

    if(!result) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the Main App listener thread.\n").c_str());
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    return xrresult;
}

bool ConnectToMain(XrInstance instance)
{
    // check to make sure not already main session process
    if(gMainSessionInstance != XR_NULL_HANDLE) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, "FATAL: attempt to make an overlay session while also having a main session\n");
        return false;
    }

    gConnectionToMain = std::make_shared<ConnectionToMain>();

    if(!OpenNegotiationChannels(instance, gNegotiationChannels)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, "FATAL: could not open negotiation channels\n");
        return false;
    }

    DWORD result;
    int attempts = 0;
    do {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("Attempt #%d (of %d) to connect to the main app\n", attempts, NegotiationChannels::maxAttempts).c_str());
        result = WaitForSingleObject(gNegotiationChannels.overlayWaitSema, NegotiationChannels::negotiationWaitMillis);
        attempts++;
    } while(attempts < NegotiationChannels::maxAttempts && result == WAIT_TIMEOUT);

    if(result == WAIT_TIMEOUT) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: the Overlay API Layer in the overlay app could not connect to the main app after %d tries.\n", attempts).c_str());
        return false;
    }

    OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateSession", 
        OverlaysLayerNoObjectInfo, fmt("connected to the main app after %d %s.\n", attempts, (attempts < 2) ? "try" : "tries").c_str());

    if(gNegotiationChannels.params->mainLayerBinaryVersion != gLayerBinaryVersion) {
        gNegotiationChannels.params->status = NegotiationParams::DIFFERENT_BINARY_VERSION;
        ReleaseSemaphore(gNegotiationChannels.mainWaitSema, 1, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: the Overlay API Layer in the overlay app has a different version (%u) than in the main app (%u).\n").c_str());
        return false;
    }

    /* save off negotiation parameters because they may be overwritten at any time after we Release mainWait */
    gMainProcessId = gNegotiationChannels.params->mainProcessId;
    gNegotiationChannels.params->overlayProcessId = GetCurrentProcessId();
    gNegotiationChannels.params->status = NegotiationParams::SUCCESS;

    ReleaseSemaphore(gNegotiationChannels.mainWaitSema, 1, nullptr);

    if(!OpenRPCChannels(gNegotiationChannels.instance, gMainProcessId, GetCurrentProcessId(), gConnectionToMain->conn)) {
        OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "no function",
            OverlaysLayerNoObjectInfo, "WARNING: couldn't open RPC channels to main app, connection failed.\n");
        return false;
    }

    return true;
}


XrResult OverlaysLayerCreateSessionOverlay(
    XrInstance                                  instance,
    const XrSessionCreateInfo*                  createInfo,
    XrSession*                                  session,
    ID3D11Device*   d3d11Device)
{
    XrResult result = XR_SUCCESS;

    // Only on Overlay XrSession Creation, connect to the main app.
    if(!ConnectToMain(instance)) {
        OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "XrCreateSession",
            OverlaysLayerNoObjectInfo, "WARNING: couldn't connect to main app.\n");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Get our tracked information on this XrInstance 
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = gOverlaysLayerXrInstanceToHandleInfo[instance];

    XrFormFactor formFactor;

    {
        // XXX REALLY SHOULD RPC TO GET THE REMOTE SYSTEMID HERE AND THEN SUBSTITUTE THAT IN THE OVERLAY BEFORE THE RPC.
        std::unique_lock<std::recursive_mutex> m(gOverlaysLayerSystemIdToAtomInfoMutex);
        const XrSystemGetInfo* systemGetInfo = gOverlaysLayerSystemIdToAtomInfo[createInfo->systemId]->getInfo;
        formFactor = systemGetInfo->formFactor;
        // XXX should check here that systemGetInfo->next == nullptr
        // but current API Layer is not specified to support any
        // XrSystemId-related extensions
    }

    auto instanceCreateInfo = instanceInfo->createInfo;

    const char** extensionNamesMinusOverlay = new const char*[instanceCreateInfo->enabledExtensionCount];
    uint32_t extensionCountMinusOverlay = 0;
    for(uint32_t i = 0; i < instanceCreateInfo->enabledExtensionCount; i++) {
        if(strncmp(instanceCreateInfo->enabledExtensionNames[i], XR_EXTX_OVERLAY_EXTENSION_NAME, strlen(XR_EXTX_OVERLAY_EXTENSION_NAME)) != 0) {
            extensionNamesMinusOverlay[extensionCountMinusOverlay++] = instanceCreateInfo->enabledExtensionNames[i];
        }
    }

    XrInstanceCreateInfo createInfoMinusOverlays = *instanceCreateInfo;
    createInfoMinusOverlays.enabledExtensionNames = extensionNamesMinusOverlay;
    createInfoMinusOverlays.enabledExtensionCount = extensionCountMinusOverlay;

    result = RPCCallCreateSession(instance, formFactor, &createInfoMinusOverlays, createInfo, session);
#if 0
    // Create a header for RPC to MainAsOverlay
    IPCBuffer ipcbuf = gConnectionToMain->conn.GetIPCBuffer();
    IPCHeader* header = new(ipcbuf) IPCHeader{RPC_XR_CREATE_SESSION};

    // Serialize our RPC args into the IPC buffer in shared memory
    OverlaysLayerRPCCreateSession args { formFactor, instanceInfo->createInfo, createInfo, session }; // ignore instance since Main won't use ours
    OverlaysLayerRPCCreateSession* argsSerialized = IPCSerialize(ipcbuf, header, &args);

    // Make pointers relative in anticipation of RPC (who will make them absolute, work on them, then make them relative again)
    header->makePointersRelative(ipcbuf.base);

    // Release Main process to do our work
    gConnectionToMain->conn.FinishOverlayRequest();

    // Wait for Main to report to us it has done the work
    bool success = gConnectionToMain->conn.WaitForMainResponseOrFail();
    if(!success) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "XrCreateSession",
            OverlaysLayerNoObjectInfo, "WARNING: couldn't RPC CreateSession to main process.\n");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Set pointers absolute so they are valid in our process space again
    header->makePointersAbsolute(ipcbuf.base);

    // Copy anything that were "output" parameters into the command arguments
    IPCCopyOut(&args, argsSerialized);
#endif

	if (!XR_SUCCEEDED(result)) {
		return result;
	}

    // Since Overlays are the parent object of a hierarchy of objects that the Main hosts on behalf of the Overlay,
    // make a unique local XrSession that notes that this is actually an overlay session and any command on this handle has to be proxied.
    // Non-Overlay XrSessions are also replaced locally with a unique local handle in case an overlay app has one.
    XrSession actualHandle = *session;
    XrSession localHandle = (XrSession)GetNextLocalHandle();
	*session = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualSessionToLocalHandleMutex);
        gActualSessionToLocalHandle[actualHandle] = localHandle;
    }
 
    OverlaysLayerXrSessionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrSessionHandleInfo>(instance, instance, instanceInfo->downchain);
    info->actualHandle = actualHandle;
    info->isProxied = true;
    info->d3d11Device = d3d11Device;
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSessionToHandleInfoMutex);
    gOverlaysLayerXrSessionToHandleInfo[localHandle] = info;
    mlock2.unlock();

    return result;
}

XrResult OverlaysLayerCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
{
    XrResult result;

    const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
    const XrSessionCreateInfoOverlayEXTX* cio = nullptr;
    const XrGraphicsBindingD3D11KHR* d3dbinding = nullptr;
    while(p) {
        if(p->type == XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX) {
            cio = reinterpret_cast<const XrSessionCreateInfoOverlayEXTX*>(p);
        }
        // XXX save off requested API in Overlay, match against Main API
        // XXX save off requested API in Main, match against Overlay API
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

    if(!cio) {
        result = OverlaysLayerCreateSessionMain(instance, createInfo, session);
    } else {
        result = OverlaysLayerCreateSessionOverlay(instance, createInfo, session, d3dbinding->device);
    }

    return result;
}

XrResult OverlaysLayerCreateSwapchainMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain, uint32_t *swapchainCount)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    XrResult result = sessionInfo->downchain->CreateSwapchain(sessionInfo->actualHandle, createInfo, swapchain);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    OutputDebugStringA("OVERLAY - EnumerateSwapchainFormats\\n");

    uint32_t count;
    result = sessionInfo->downchain->EnumerateSwapchainImages(*swapchain, 0, &count, nullptr);
    if(!XR_SUCCEEDED(result)) {
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSwapchain",
            OverlaysLayerNoObjectInfo, "FATAL: Couldn't call EnumerateSwapchainImages to get swapchain image count.\n");
        return result;
    }

    std::vector<XrSwapchainImageD3D11KHR> swapchainImages(count);
    std::vector<ID3D11Texture2D*> swapchainTextures(count);
    for(uint32_t i = 0; i < count; i++) {
        swapchainImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        swapchainImages[i].next = nullptr;
    }
    result = sessionInfo->downchain->EnumerateSwapchainImages(*swapchain, count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data()));
    if(!XR_SUCCEEDED(result)) {
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSwapchain",
            OverlaysLayerNoObjectInfo, "FATAL: Couldn't call EnumerateSwapchainImages to get swapchain images.\n");
        return result;
    }

    for(uint32_t i = 0; i < count; i++) {
        swapchainTextures[i] = swapchainImages[i].texture;
    }

    *swapchainCount = count;

    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = std::make_shared<OverlaysLayerXrSwapchainHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    swapchainInfo->mainCachedSwapchainData = std::make_shared<SwapchainCachedData>(*swapchain, swapchainTextures);
    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSwapchainToHandleInfoMutex);
        gOverlaysLayerXrSwapchainToHandleInfo[*swapchain] = swapchainInfo;
    }

    return result;
}

XrResult OverlaysLayerCreateSwapchainOverlay(XrInstance instance, XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    uint32_t swapchainCount;

    XrResult result = RPCCallCreateSwapchain(instance, sessionInfo->actualHandle, createInfo, swapchain, &swapchainCount);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    XrSwapchain actualHandle = *swapchain;
    XrSwapchain localHandle = (XrSwapchain)GetNextLocalHandle();
    *swapchain = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualXrSwapchainToLocalHandleMutex);
        gActualXrSwapchainToLocalHandle[actualHandle] = localHandle;
    }

    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = std::make_shared<OverlaysLayerXrSwapchainHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);

    swapchainInfo->actualHandle = actualHandle;
    swapchainInfo->isProxied = true;

    OverlaySwapchain::Ptr overlaySwapchain = std::make_shared<OverlaySwapchain>(*swapchain, swapchainCount, createInfo);
    swapchainInfo->overlaySwapchain = overlaySwapchain;

    if(!overlaySwapchain->CreateTextures(instance, sessionInfo->d3d11Device, gConnectionToMain->conn.otherProcessId)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSwapchain",
            OverlaysLayerNoObjectInfo, "FATAL: couldn't create D3D local resources for swapchain images\\n");
        // XXX This leaks the session in main process if the Session is not closed.
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSwapchainToHandleInfoMutex);
        gOverlaysLayerXrSwapchainToHandleInfo[*swapchain] = swapchainInfo;
    }

    return result;
}

XrResult OverlaysLayerCreateReferenceSpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    XrResult result = sessionInfo->downchain->CreateReferenceSpace(sessionInfo->actualHandle, createInfo, space);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSpaceToHandleInfoMutex);
        gOverlaysLayerXrSpaceToHandleInfo[*space] = spaceInfo;
    }

    return result;
}

XrResult OverlaysLayerCreateReferenceSpaceOverlay(XrInstance instance, XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    XrResult result = RPCCallCreateReferenceSpace(instance, sessionInfo->actualHandle, createInfo, space);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    XrSpace actualHandle = *space;
    XrSpace localHandle = (XrSpace)GetNextLocalHandle();
    *space = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualXrSpaceToLocalHandleMutex);
        gActualXrSpaceToLocalHandle[actualHandle] = localHandle;
    }

    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);

    spaceInfo->actualHandle = actualHandle;
    spaceInfo->isProxied = true;

    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSpaceToHandleInfoMutex);
        gOverlaysLayerXrSpaceToHandleInfo[*space] = spaceInfo;
    }

    return result;
}

XrResult OverlaysLayerDestroySessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    connection->closed = true;

    return XR_SUCCESS;
}

XrResult OverlaysLayerDestroySessionOverlay(XrInstance instance, XrSession session)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    XrResult result = RPCCallDestroySession(instance, sessionInfo->actualHandle);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

XrResult OverlaysLayerEnumerateSwapchainFormatsMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    // Already have our tracked information on this XrSession from generated code in sessionInfo
    XrResult result = sessionInfo->downchain->EnumerateSwapchainFormats(sessionInfo->actualHandle, formatCapacityInput, formatCountOutput, formats);

    return result;
}

XrResult OverlaysLayerEnumerateSwapchainFormatsOverlay(XrInstance instance, XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[session];

    XrResult result = RPCCallEnumerateSwapchainFormats(instance, sessionInfo->actualHandle, formatCapacityInput, formatCountOutput, formats);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

// EnumerateSwapchainImages is handled entirely on Overlay side because we
// already Enumerated the main SwapchainImages when we created the Swapchain.
XrResult OverlaysLayerEnumerateSwapchainImagesOverlay(
        XrInstance instance,
        XrSwapchain swapchain,
        uint32_t imageCapacityInput,
        uint32_t* imageCountOutput,
        XrSwapchainImageBaseHeader* images)
{
    std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSwapchainToHandleInfoMutex);
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = gOverlaysLayerXrSwapchainToHandleInfo[swapchain];

    auto& overlaySwapchain = swapchainInfo->overlaySwapchain;

    if(imageCapacityInput == 0) {
        *imageCountOutput = (uint32_t)overlaySwapchain->swapchainTextures.size();
        return XR_SUCCESS;
    }

    if(images[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
        std::unique_lock<std::recursive_mutex> lock2(gOverlaysLayerXrInstanceToHandleInfoMutex);
        OverlaysLayerXrInstanceHandleInfo::Ptr info = gOverlaysLayerXrInstanceToHandleInfo.at(instance);
        lock2.unlock();
        char structTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
        structTypeName[0] = '\0';
        if(info->downchain->StructureTypeToString(instance, images[0].type, structTypeName) != XR_SUCCESS) {
            sprintf(structTypeName, "<type %08X>", images[0].type);
        }
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
            OverlaysLayerNoObjectInfo, fmt("FATAL: images structure type is %s and not XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR.\n", structTypeName).c_str());
        return XR_ERROR_VALIDATION_FAILURE;
    }

    // (If storage is provided) Give back the "local" swapchainimages (rendertarget) for rendering
    auto sci = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
    uint32_t toWrite = std::min(imageCapacityInput, (uint32_t)overlaySwapchain->swapchainTextures.size());
    for(uint32_t i = 0; i < toWrite; i++) {
        sci[i].texture = overlaySwapchain->swapchainTextures[i];
    }

    *imageCountOutput = toWrite;

    return XR_SUCCESS;
}

XrResult OverlaysLayerPollEventMainAsOverlay(ConnectionToOverlay::Ptr connection, XrEventDataBuffer *eventData)
{
	XrResult result;

    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);

	MainSessionContext::Ptr mainSessionContext = gMainSessionContext;
	mainSessionContext->GetLock();

    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo[mainSessionContext->session];

    OptionalSessionStateChange pendingStateChange;

    pendingStateChange = connection->ctx->sessionState.GetAndDoPendingStateChange(&mainSessionContext->sessionState);

    if(pendingStateChange.first) {

		XrEventDataBuffer event{XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED, nullptr};
		
        auto* ssc = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
        ssc->session = mainSessionContext->session;
        ssc->state = pendingStateChange.second;
        XrTime calculatedTime = 1; // XXX 
        ssc->time = calculatedTime;
        CopyEventChainIntoBuffer(gMainSessionInstance, reinterpret_cast<XrEventDataBaseHeader *>(&event), eventData);
        result = XR_SUCCESS;

    } else {

		auto lock = connection->ctx->GetLock();

		if(connection->ctx->eventsSaved.size() == 0) {

			result = XR_EVENT_UNAVAILABLE;

		} else {

			EventDataBufferPtr event = connection->ctx->eventsSaved.front();
			connection->ctx->eventsSaved.pop();
			CopyEventChainIntoBuffer(gMainSessionInstance, reinterpret_cast<XrEventDataBaseHeader*>(event.get()), eventData);

			result = XR_SUCCESS;
		}
	}

    return result;
}

void EnqueueEventToOverlay(XrInstance instance, XrEventDataBuffer *eventData, MainAsOverlaySessionContext::Ptr overlay)
{
    auto lock = overlay->GetLock();

    bool queueFull = (overlay->eventsSaved.size() == MainAsOverlaySessionContext::maxEventsSavedForOverlay);
    bool queueOneShortOfFull = (overlay->eventsSaved.size() == MainAsOverlaySessionContext::maxEventsSavedForOverlay - 1);
    bool backIsEventsLostEvent = (overlay->eventsSaved.size() > 0) && (overlay->eventsSaved.back()->type == XR_TYPE_EVENT_DATA_EVENTS_LOST);

    bool alreadyLostSomeEvents = queueFull || (queueOneShortOfFull && backIsEventsLostEvent);

    EventDataBufferPtr newEvent(new XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER});
    CopyEventChainIntoBuffer(instance, const_cast<const XrEventDataBaseHeader*>(reinterpret_cast<XrEventDataBaseHeader*>(eventData)), newEvent.get());

    if(newEvent.get()->type != XR_TYPE_EVENT_DATA_BUFFER) {
        // We were able to find some known events in the event pointer chain

        if(alreadyLostSomeEvents) {

            auto* lost = reinterpret_cast<XrEventDataEventsLost*>(overlay->eventsSaved.back().get());
            lost->lostEventCount ++;

        } else if(queueOneShortOfFull) {

            EventDataBufferPtr newEvent(new XrEventDataBuffer);
            XrEventDataEventsLost* lost = reinterpret_cast<XrEventDataEventsLost*>(newEvent.get());
            lost->type = XR_TYPE_EVENT_DATA_EVENTS_LOST;
            lost->next = nullptr;
            lost->lostEventCount = 1;
            overlay->eventsSaved.push(newEvent);
            OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrPollEvent",
                OverlaysLayerNoObjectInfo, "WARNING: Enqueued a lost event.\n");

        } else {

            overlay->eventsSaved.push(newEvent);
            
        }
    }
}

XrResult OverlaysLayerPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
{
    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
    auto it = gOverlaysLayerXrInstanceToHandleInfo.find(instance);
    if(it == gOverlaysLayerXrInstanceToHandleInfo.end()) {
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrPollEvent",
            OverlaysLayerNoObjectInfo, "FATAL: invalid handle couldn't be found in tracking map.\n");
        return XR_ERROR_VALIDATION_FAILURE;
    }
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = it->second;

    XrResult result = instanceInfo->downchain->PollEvent(instance, eventData);

    if(result == XR_EVENT_UNAVAILABLE) {

        if(gConnectionToMain) {

            /* See if the main process has any events for us */
            result = RPCCallPollEvent(instance, eventData);
            if(result == XR_SUCCESS) {
                SubstituteLocalHandles(instance, (XrBaseOutStructure *)eventData);
            }
        }

    } else if(result == XR_SUCCESS) {

        SubstituteLocalHandles(instance, (XrBaseOutStructure *)eventData);

        std::unique_lock<std::recursive_mutex> lock(gConnectionsToOverlayByProcessIdMutex);
        if(!gConnectionsToOverlayByProcessId.empty()) {

            if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {

                // XXX ignores any chained event data
                const auto* ssc = reinterpret_cast<const XrEventDataSessionStateChanged*>(eventData);
                MainSessionContext::Ptr mainSessionContext = gMainSessionContext;
                mainSessionContext->GetLock();
                mainSessionContext->sessionState.DoStateChange(ssc->state, ssc->time);
                if(ssc->next) {
                    char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
					auto* p = reinterpret_cast<const XrBaseOutStructure*>(ssc->next);
                    XrResult r = instanceInfo->downchain->StructureTypeToString(gMainSessionInstance, p->type, structureTypeName);
                    if(r != XR_SUCCESS) {
                        sprintf(structureTypeName, "(type %08X)", p->type);
                    }

                    OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrPollEvent",
                        OverlaysLayerNoObjectInfo, fmt("WARNING: xrPollEvent filled a struct (%s) which the Overlay API Layer does not know how to check.\n", structureTypeName).c_str());
                }

            } else {

                if(eventData->type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {

                    for(auto& overlayconn: gConnectionsToOverlayByProcessId) {
                        auto conn = overlayconn.second;
                        auto lock = conn->GetLock();
                        if(conn->ctx) {
                            EnqueueEventToOverlay(instance, eventData, conn->ctx);
                        }
                    }

                } else if(eventData->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {

                    // XXX further processing
                    for(auto& overlayconn: gConnectionsToOverlayByProcessId) {
                        auto conn = overlayconn.second;
                        auto lock = conn->GetLock();
                        if(conn->ctx) {
                            EnqueueEventToOverlay(instance, eventData, conn->ctx);
                        }
                    }
                }

            }
        }
    }

    return result;
}


extern "C" {

// Function used to negotiate an interface betewen the loader and an API layer.  Each library exposing one or
// more API layers needs to expose at least this function.
XrResult LAYER_EXPORT XRAPI_CALL Overlays_xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *loaderInfo,
                                                                    const char* apiLayerName,
                                                                    XrNegotiateApiLayerRequest *apiLayerRequest)
{
    if (apiLayerName)
    {
        if (0 != strncmp(kOverlayLayerName, apiLayerName, strnlen_s(kOverlayLayerName, XR_MAX_API_LAYER_NAME_SIZE)))
        {
            return XR_ERROR_INITIALIZATION_FAILED;
        }
    }

    if (!loaderInfo ||
        !apiLayerRequest ||
        loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION ||
        loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        apiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION ||
        loaderInfo->minApiVersion > XR_CURRENT_API_VERSION) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
    apiLayerRequest->getInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(OverlaysLayerXrGetInstanceProcAddr);
    apiLayerRequest->createApiLayerInstance = reinterpret_cast<PFN_xrCreateApiLayerInstance>(OverlaysLayerXrCreateApiLayerInstance);

    return XR_SUCCESS;
}

}  // extern "C"
