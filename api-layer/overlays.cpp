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
//

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

#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(_WIN32)
#define LAYER_EXPORT __declspec(dllexport)
#else
#define LAYER_EXPORT
#endif

const char *kOverlayLayerName = "xr_extx_overlay";

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

const std::set<HandleTypePair> OverlaysLayerNoObjectInfo = {};


void OverlaysLayerLogMessage(XrInstance instance,
                         XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message)
{
    // If we have instance information, see if we need to log this information out to a debug messenger
    // callback.
    if(instance != XR_NULL_HANDLE) {

        OverlaysLayerXrInstanceHandleInfo& instanceInfo = gOverlaysLayerXrInstanceToHandleInfo.at(instance);

        // To be a little more performant, check all messenger's
        // messageSeverities and messageTypes to make sure we will call at
        // least one

        if ( /* !instanceInfo.debug_data.Empty() && */ !instanceInfo.debugUtilsMessengers.empty()) {

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

#endif
            // Setup our callback data once
            XrDebugUtilsMessengerCallbackDataEXT callback_data = {};
            callback_data.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
            callback_data.messageId = "Overlays API Layer";
            callback_data.functionName = command_name;
            callback_data.message = message;
#if 0
            names_and_labels.PopulateCallbackData(callback_data);
#endif

            // Loop through all active messengers and give each a chance to output information
            for (const auto &messenger : instanceInfo.debugUtilsMessengers) {

                std::unique_lock<std::mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
                XrDebugUtilsMessengerCreateInfoEXT messenger_create_info = gOverlaysLayerXrDebugUtilsMessengerEXTToHandleInfo.at(messenger).createInfo;
                mlock.unlock();

                // If a callback exists, and the message is of a type this callback cares about, call it.
                if (nullptr != messenger_create_info.userCallback &&
                    0 != (messenger_create_info.messageSeverities & message_severity) &&
                    0 != (messenger_create_info.messageTypes & XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)) {

                    XrBool32 ret_val = messenger_create_info.userCallback(message_severity,
                                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                                                           &callback_data, messenger_create_info.userData);
                }
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



template <class XR_STRUCT>
XrBaseInStructure* AllocateAndCopy(const XR_STRUCT* srcbase, CopyType copyType, std::function<void* (size_t size)> alloc)
{
    auto src = reinterpret_cast<const XR_STRUCT*>(srcbase);
    auto dst = reinterpret_cast<XR_STRUCT*>(alloc(sizeof(XR_STRUCT)));
    if(copyType == COPY_EVERYTHING) {
        *dst = *src;
    } else {
        dst->type = src->type;
    }
    return reinterpret_cast<XrBaseInStructure*>(dst);
}

XrBaseInStructure *CopyXrStructChain(const XrBaseInStructure* srcbase, CopyType copyType, AllocateFunc alloc, std::function<void (void* pointerToPointer)> addOffsetToPointer)
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
            case XR_TYPE_INSTANCE_CREATE_INFO: {
                auto src = reinterpret_cast<const XrInstanceCreateInfo*>(srcbase);
                auto dst = reinterpret_cast<XrInstanceCreateInfo*>(alloc(sizeof(XrInstanceCreateInfo)));
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                dst->type = src->type;

                dst->createFlags = src->createFlags;
                dst->applicationInfo = src->applicationInfo;

                // Lay down API layer names...
                auto enabledApiLayerNames = (char **)alloc(sizeof(char *) * src->enabledApiLayerCount);
                dst->enabledApiLayerNames = enabledApiLayerNames;
                dst->enabledApiLayerCount = src->enabledApiLayerCount;
                addOffsetToPointer(&dst->enabledApiLayerNames);
                for(uint32_t i = 0; i < dst->enabledApiLayerCount; i++) {
                    enabledApiLayerNames[i] = (char *)alloc(strlen(src->enabledApiLayerNames[i]) + 1);
                    strncpy_s(enabledApiLayerNames[i], strlen(src->enabledApiLayerNames[i]) + 1, src->enabledApiLayerNames[i], strlen(src->enabledApiLayerNames[i]) + 1);
                    addOffsetToPointer(&enabledApiLayerNames[i]);
                }

                // Lay down extension layer names...
                auto enabledExtensionNames = (char **)alloc(sizeof(char *) * src->enabledExtensionCount);
                dst->enabledExtensionNames = enabledExtensionNames;
                dst->enabledExtensionCount = src->enabledExtensionCount;
                addOffsetToPointer(&dst->enabledExtensionNames);
                for(uint32_t i = 0; i < dst->enabledExtensionCount; i++) {
                    enabledExtensionNames[i] = (char *)alloc(strlen(src->enabledExtensionNames[i]) + 1);
                    strncpy_s(enabledExtensionNames[i], strlen(src->enabledExtensionNames[i]) + 1, src->enabledExtensionNames[i], strlen(src->enabledExtensionNames[i]) + 1);
                    addOffsetToPointer(&enabledExtensionNames[i]);
                }
                break;
            }

            case XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrGraphicsRequirementsD3D11KHR*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SPACE_LOCATION: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSpaceLocation*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_FRAME_STATE: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrFrameState*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SYSTEM_PROPERTIES: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSystemProperties*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_INSTANCE_PROPERTIES: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrInstanceProperties*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_VIEW: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrViewConfigurationView*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_PROPERTIES: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrViewConfigurationProperties*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SESSION_BEGIN_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSessionBeginInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SYSTEM_GET_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSystemGetInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SWAPCHAIN_CREATE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainCreateInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_FRAME_WAIT_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrFrameWaitInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_FRAME_BEGIN_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrFrameBeginInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_COMPOSITION_LAYER_QUAD: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrCompositionLayerQuad*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_EVENT_DATA_BUFFER: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataBuffer*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
                auto src = reinterpret_cast<const XrCompositionLayerProjection*>(srcbase);
                auto dst = reinterpret_cast<XrCompositionLayerProjection*>(alloc(sizeof(XrCompositionLayerProjection)));
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                dst->type = src->type;

                dst->layerFlags = src->layerFlags;
                dst->space = src->space;
                dst->viewCount = src->viewCount;

                // Lay down views...
                auto views = (XrCompositionLayerProjectionView*)alloc(sizeof(XrCompositionLayerProjectionView) * src->viewCount);
                dst->views = views;
                addOffsetToPointer(&dst->views);
                for(uint32_t i = 0; i < dst->viewCount; i++) {
                    views[i] = src->views[i]; // XXX sloppy
                    views[i].next =(CopyXrStructChain(reinterpret_cast<const XrBaseInStructure*>(src->views[i].next), copyType, alloc, addOffsetToPointer));
                    addOffsetToPointer(&(views[i].next));
                }
                break;
            }

            case XR_TYPE_FRAME_END_INFO: {
                auto src = reinterpret_cast<const XrFrameEndInfo*>(srcbase);
                auto dst = reinterpret_cast<XrFrameEndInfo*>(alloc(sizeof(XrFrameEndInfo)));
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                dst->type = src->type;
                dst->displayTime = src->displayTime;
                dst->environmentBlendMode = src->environmentBlendMode;
                dst->layerCount = src->layerCount;

                // Lay down layers...
                auto layers = (XrCompositionLayerBaseHeader**)alloc(sizeof(XrCompositionLayerBaseHeader*) * src->layerCount);
                dst->layers = layers;
                addOffsetToPointer(&dst->layers);
                for(uint32_t i = 0; i < dst->layerCount; i++) {
                    layers[i] = reinterpret_cast<XrCompositionLayerBaseHeader*>(CopyXrStructChain(reinterpret_cast<const XrBaseInStructure*>(src->layers[i]), copyType, alloc, addOffsetToPointer));
                    addOffsetToPointer(&layers[i]);
                }
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainImageAcquireInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainImageWaitInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSwapchainImageReleaseInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSessionCreateInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrSessionCreateInfoOverlayEXTX*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_REFERENCE_SPACE_CREATE_INFO: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrReferenceSpaceCreateInfo*>(srcbase), copyType, alloc);
                break;
            }

            case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
                // We know what this is but do not send it through because we process it locally.
                srcbase = srcbase->next;
                skipped = true;
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataInstanceLossPending*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataSessionStateChanged*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataEventsLost*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataInteractionProfileChanged*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: { 
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataPerfSettingsEXT*>(srcbase), copyType, alloc);
                break;
            }
            case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR: {
                dstbase = AllocateAndCopy(reinterpret_cast<const XrEventDataVisibilityMaskChangedKHR*>(srcbase), copyType, alloc);
                break;
            }

            default: {
                // I don't know what this is, skip it and try the next one
                OverlaysLayerLogMessage(XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                         nullptr, OverlaysLayerNoObjectInfo, fmt("CopyXrStructChain called on %p of unknown type %d - dropped from \"next\" chain.\n", srcbase, srcbase->type).c_str());
                srcbase = srcbase->next;
                skipped = true;
                break;
            }
        }
    } while(skipped);

    dstbase->next = reinterpret_cast<XrBaseInStructure*>(CopyXrStructChain(srcbase->next, copyType, alloc, addOffsetToPointer));
    addOffsetToPointer(&dstbase->next);

    return dstbase;
}

void FreeXrStructChain(const XrBaseInStructure* p, FreeFunc free)
{
    if(!p) {
        return;
    }

    switch(p->type) {

        case XR_TYPE_INSTANCE_CREATE_INFO: {
            auto* actual = reinterpret_cast<const XrInstanceCreateInfo*>(p);

            // Delete API Layer names
            for(uint32_t i = 0; i < actual->enabledApiLayerCount; i++) {
                free(actual->enabledApiLayerNames[i]);
            }
            free(actual->enabledApiLayerNames);

            // Delete extension names
            for(uint32_t i = 0; i < actual->enabledExtensionCount; i++) {
                free(actual->enabledExtensionNames[i]);
            }
            free(actual->enabledApiLayerNames);
            break;
        }

        case XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR:
        case XR_TYPE_FRAME_STATE:
        case XR_TYPE_SYSTEM_PROPERTIES:
        case XR_TYPE_INSTANCE_PROPERTIES:
        case XR_TYPE_VIEW_CONFIGURATION_VIEW:
        case XR_TYPE_VIEW_CONFIGURATION_PROPERTIES:
        case XR_TYPE_SESSION_BEGIN_INFO:
        case XR_TYPE_SYSTEM_GET_INFO:
        case XR_TYPE_SWAPCHAIN_CREATE_INFO:
        case XR_TYPE_FRAME_WAIT_INFO:
        case XR_TYPE_FRAME_BEGIN_INFO:
        case XR_TYPE_COMPOSITION_LAYER_QUAD:
        case XR_TYPE_EVENT_DATA_BUFFER:
        case XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO:
        case XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO:
        case XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO:
        case XR_TYPE_SESSION_CREATE_INFO:
        case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX:
        case XR_TYPE_REFERENCE_SPACE_CREATE_INFO:
        case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR:
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        case XR_TYPE_EVENT_DATA_EVENTS_LOST:
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
        case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: 
        case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
        case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR:
            break;

        case XR_TYPE_FRAME_END_INFO: {
            auto* actual = reinterpret_cast<const XrFrameEndInfo*>(p);
            // Delete layers...
            for(uint32_t i = 0; i < actual->layerCount; i++) {
                FreeXrStructChain(reinterpret_cast<const XrBaseInStructure*>(actual->layers[i]), free);
            }
            free(actual->layers);
            break;
        }

        case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
            auto* actual = reinterpret_cast<const XrCompositionLayerProjection*>(p);
            // Delete views...
            for(uint32_t i = 0; i < actual->viewCount; i++) {
                free(actual->views[i].next);
            }
            free(actual->views);
            break;
        }

        default: {
            // I don't know what this is, skip it and try the next one
                OverlaysLayerLogMessage(XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                         nullptr, OverlaysLayerNoObjectInfo, fmt("Warning: Free called on %p of unknown type %d - will descend \"next\" but don't know any other pointers.\n", p, p->type).c_str());
            break;
        }
    }

    FreeXrStructChain(p->next, free);
    free(p);
}

XrBaseInStructure* CopyEventChainIntoBuffer(const XrEventDataBaseHeader* eventData, XrEventDataBuffer* buffer)
{
    size_t remaining = sizeof(XrEventDataBuffer);
    unsigned char* next = reinterpret_cast<unsigned char *>(buffer);
    return CopyXrStructChain(reinterpret_cast<const XrBaseInStructure*>(eventData), COPY_EVERYTHING,
            [&buffer,&remaining,&next](size_t s){unsigned char* cur = next; next += s; return cur; },
            [](void *){ });
}

XrBaseInStructure* CopyXrStructChainWithMalloc(const void* xrstruct)
{
    return CopyXrStructChain(reinterpret_cast<const XrBaseInStructure*>(xrstruct), COPY_EVERYTHING,
            [](size_t s){return malloc(s); },
            [](void *){ });
}

void FreeXrStructChainWithFree(const void* xrstruct)
{
    FreeXrStructChain(reinterpret_cast<const XrBaseInStructure*>(xrstruct),
            [](const void *p){free(const_cast<void*>(p));});
}

XrResult OverlaysLayerXrCreateInstance(const XrInstanceCreateInfo * /*info*/, XrInstance * /*instance*/)
{
    return XR_SUCCESS;
}


XrResult OverlaysLayerXrCreateApiLayerInstance(const XrInstanceCreateInfo *info,
        const struct XrApiLayerCreateInfo *apiLayerInfo, XrInstance *instance)
{
    PFN_xrGetInstanceProcAddr next_get_instance_proc_addr = nullptr;
    PFN_xrCreateApiLayerInstance next_create_api_layer_instance = nullptr;
    XrApiLayerCreateInfo new_api_layer_info = {};

#if 0
    gMainInstanceContext.savedRequestedExtensions.clear();
    gMainInstanceContext.savedRequestedExtensions.insert(XR_EXTX_OVERLAY_EXTENSION_NAME);
    gMainInstanceContext.savedRequestedExtensions.insert(info->enabledExtensionNames, info->enabledExtensionNames + info->enabledExtensionCount);

    gMainInstanceContext.savedRequestedApiLayers.clear();
    // Is there a way to query which layers are only downstream?
    // We can't get to the functionality of layers upstream (closer to
    // the app), so we can't claim all these layers are enabled (the remote
    // app can't use these layers)
    // gMainInstanceContext.savedRequestedApiLayers.insert(info->enabledApiLayerNames, info->enabledApiLayerNames + info->enabledApiLayerCount);
#endif

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

    // Get the function pointers we need
    next_get_instance_proc_addr = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;
    next_create_api_layer_instance = apiLayerInfo->nextInfo->nextCreateApiLayerInstance;

    // Create the instance
    XrInstance returned_instance = *instance;
    XrResult result = next_create_api_layer_instance(info, &new_api_layer_info, &returned_instance);
    *instance = returned_instance;

    // Create the dispatch table to the next levels
    auto *next_dispatch = new XrGeneratedDispatchTable();
    GeneratedXrPopulateDispatchTable(next_dispatch, returned_instance, next_get_instance_proc_addr);

    std::unique_lock<std::mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
	gOverlaysLayerXrInstanceToHandleInfo.emplace(*instance, next_dispatch);

    // CreateOverlaySessionThread();

    return result;
}

XrResult OverlaysLayerXrDestroyInstance(XrInstance instance) {
	std::unique_lock<std::mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
	OverlaysLayerXrInstanceHandleInfo& instanceInfo = gOverlaysLayerXrInstanceToHandleInfo.at(instance);
	XrGeneratedDispatchTable *next_dispatch = instanceInfo.downchain;
	instanceInfo.Destroy();
	mlock.unlock();

    next_dispatch->DestroyInstance(instance);

    return XR_SUCCESS;
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
