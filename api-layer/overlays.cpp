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

        /* XXX TBD !instanceInfo.debug_data.Empty() */

        if (!instanceInfo.debugUtilsMessengers.empty()) {

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
            for (const auto &messenger : instanceInfo.debugUtilsMessengers) {

                std::unique_lock<std::mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
                XrDebugUtilsMessengerCreateInfoEXT *messenger_create_info = gOverlaysLayerXrDebugUtilsMessengerEXTToHandleInfo.at(messenger).createInfo;
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

    // Get the function pointers we need
    next_get_instance_proc_addr = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;
    next_create_api_layer_instance = apiLayerInfo->nextInfo->nextCreateApiLayerInstance;

    // Create the instance
    XrInstance returned_instance = *instance;
    XrResult result = next_create_api_layer_instance(instanceCreateInfo, &new_api_layer_info, &returned_instance);
    *instance = returned_instance;

    // Create the dispatch table to the next levels
    auto *next_dispatch = new XrGeneratedDispatchTable();
    GeneratedXrPopulateDispatchTable(next_dispatch, returned_instance, next_get_instance_proc_addr);

    std::unique_lock<std::mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
	gOverlaysLayerXrInstanceToHandleInfo.emplace(*instance, next_dispatch);
    gOverlaysLayerXrInstanceToHandleInfo.at(*instance).createInfo = reinterpret_cast<XrInstanceCreateInfo*>(CopyXrStructChainWithMalloc(*instance, instanceCreateInfo));

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
