//
// Copyright 2019-2020 LunarG Inc. and PlutoVR Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Author: Brad Grantham <brad@lunarg.com>
//

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
#include "../include/util.h"
#include <openxr/openxr_platform.h>

#include <dxgi1_2.h>
#include <d3d11_1.h>

XrResult xrEnumerateApiLayerProperties(uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrApiLayerProperties* properties)
{
    outputDebugF("Application called xrEnumerateApiLayerProperties but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrDestroyInstance(XrInstance instance)
{
    outputDebugF("Application called xrDestroyInstance but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrResultToString(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE])
{
    outputDebugF("Application called xrResultToString but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrStructureTypeToString(XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE])
{
    outputDebugF("Application called xrStructureTypeToString but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput, uint32_t* environmentBlendModeCountOutput, XrEnvironmentBlendMode* environmentBlendModes)
{
    outputDebugF("Application called xrEnumerateEnvironmentBlendModes but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces)
{
    outputDebugF("Application called xrEnumerateReferenceSpaces but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetReferenceSpaceBoundsRect(XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds)
{
    outputDebugF("Application called xrGetReferenceSpaceBoundsRect but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
{
    outputDebugF("Application called xrCreateActionSpace but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
{
    outputDebugF("Application called xrLocateViews but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path)
{
    outputDebugF("Application called xrStringToPath but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    outputDebugF("Application called xrPathToString but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet)
{
    outputDebugF("Application called xrCreateActionSet but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrDestroyActionSet(XrActionSet actionSet)
{
    outputDebugF("Application called xrDestroyActionSet but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
{
    outputDebugF("Application called xrCreateAction but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrDestroyAction(XrAction action)
{
    outputDebugF("Application called xrDestroyAction but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
{
    outputDebugF("Application called xrSuggestInteractionProfileBindings but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
{
    outputDebugF("Application called xrAttachSessionActionSets but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile)
{
    outputDebugF("Application called xrGetCurrentInteractionProfile but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state)
{
    outputDebugF("Application called xrGetActionStateBoolean but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state)
{
    outputDebugF("Application called xrGetActionStateFloat but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state)
{
    outputDebugF("Application called xrGetActionStateVector2f but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state)
{
    outputDebugF("Application called xrGetActionStatePose but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
{
    outputDebugF("Application called xrSyncActions but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrEnumerateBoundSourcesForAction(XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources)
{
    outputDebugF("Application called xrEnumerateBoundSourcesForAction but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetInputSourceLocalizedName(XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    outputDebugF("Application called xrGetInputSourceLocalizedName but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback)
{
    outputDebugF("Application called xrApplyHapticFeedback but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo)
{
    outputDebugF("Application called xrStopHapticFeedback but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetVisibilityMaskKHR(XrSession session, XrViewConfigurationType viewConfigurationType, uint32_t viewIndex, XrVisibilityMaskTypeKHR visibilityMaskType, XrVisibilityMaskKHR* visibilityMask)
{
    outputDebugF("Application called xrGetVisibilityMaskKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrPerfSettingsSetPerformanceLevelEXT(XrSession session, XrPerfSettingsDomainEXT domain, XrPerfSettingsLevelEXT level)
{
    outputDebugF("Application called xrPerfSettingsSetPerformanceLevelEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrThermalGetTemperatureTrendEXT(XrSession session, XrPerfSettingsDomainEXT domain, XrPerfSettingsNotificationLevelEXT* notificationLevel, float* tempHeadroom, float* tempSlope)
{
    outputDebugF("Application called xrThermalGetTemperatureTrendEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSetDebugUtilsObjectNameEXT(XrInstance instance, const XrDebugUtilsObjectNameInfoEXT* nameInfo)
{
    outputDebugF("Application called xrSetDebugUtilsObjectNameEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrCreateDebugUtilsMessengerEXT(XrInstance instance, const XrDebugUtilsMessengerCreateInfoEXT* createInfo, XrDebugUtilsMessengerEXT* messenger)
{
    outputDebugF("Application called xrCreateDebugUtilsMessengerEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrDestroyDebugUtilsMessengerEXT(XrDebugUtilsMessengerEXT messenger)
{
    outputDebugF("Application called xrDestroyDebugUtilsMessengerEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSubmitDebugUtilsMessageEXT(XrInstance instance, XrDebugUtilsMessageSeverityFlagsEXT messageSeverity, XrDebugUtilsMessageTypeFlagsEXT messageTypes, const XrDebugUtilsMessengerCallbackDataEXT* callbackData)
{
    outputDebugF("Application called xrSubmitDebugUtilsMessageEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSessionBeginDebugUtilsLabelRegionEXT(XrSession session, const XrDebugUtilsLabelEXT* labelInfo)
{
    outputDebugF("Application called xrSessionBeginDebugUtilsLabelRegionEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSessionEndDebugUtilsLabelRegionEXT(XrSession session)
{
    outputDebugF("Application called xrSessionEndDebugUtilsLabelRegionEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSessionInsertDebugUtilsLabelEXT(XrSession session, const XrDebugUtilsLabelEXT* labelInfo)
{
    outputDebugF("Application called xrSessionInsertDebugUtilsLabelEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrCreateSpatialAnchorMSFT(XrSession session, const XrSpatialAnchorCreateInfoMSFT* createInfo, XrSpatialAnchorMSFT* anchor)
{
    outputDebugF("Application called xrCreateSpatialAnchorMSFT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrCreateSpatialAnchorSpaceMSFT(XrSession session, const XrSpatialAnchorSpaceCreateInfoMSFT* createInfo, XrSpace* space)
{
    outputDebugF("Application called xrCreateSpatialAnchorSpaceMSFT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrDestroySpatialAnchorMSFT(XrSpatialAnchorMSFT anchor)
{
    outputDebugF("Application called xrDestroySpatialAnchorMSFT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSetInputDeviceActiveEXT(XrSession session, XrPath interactionProfile, XrPath topLevelPath, XrBool32 isActive)
{
    outputDebugF("Application called xrSetInputDeviceActiveEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSetInputDeviceStateBoolEXT(XrSession session, XrPath topLevelPath, XrPath inputSourcePath, XrBool32 state)
{
    outputDebugF("Application called xrSetInputDeviceStateBoolEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSetInputDeviceStateFloatEXT(XrSession session, XrPath topLevelPath, XrPath inputSourcePath, float state)
{
    outputDebugF("Application called xrSetInputDeviceStateFloatEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSetInputDeviceStateVector2fEXT(XrSession session, XrPath topLevelPath, XrPath inputSourcePath, XrVector2f state)
{
    outputDebugF("Application called xrSetInputDeviceStateVector2fEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrSetInputDeviceLocationEXT(XrSession session, XrPath topLevelPath, XrPath inputSourcePath, XrSpace space, XrPosef pose)
{
    outputDebugF("Application called xrSetInputDeviceLocationEXT but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
#if 0
XrResult xrSetAndroidApplicationThreadKHR(XrSession session, XrAndroidThreadTypeKHR threadType, uint32_t threadId)
{
    outputDebugF("Application called xrSetAndroidApplicationThreadKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrCreateSwapchainAndroidSurfaceKHR(XrSession session, const XrSwapchainCreateInfo* info, XrSwapchain* swapchain, jobject* surface)
{
    outputDebugF("Application called xrCreateSwapchainAndroidSurfaceKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetOpenGLGraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsOpenGLKHR* graphicsRequirements)
{
    outputDebugF("Application called xrGetOpenGLGraphicsRequirementsKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetOpenGLESGraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsOpenGLESKHR* graphicsRequirements)
{
    outputDebugF("Application called xrGetOpenGLESGraphicsRequirementsKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetVulkanInstanceExtensionsKHR(XrInstance instance, XrSystemId systemId, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    outputDebugF("Application called xrGetVulkanInstanceExtensionsKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetVulkanDeviceExtensionsKHR(XrInstance instance, XrSystemId systemId, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    outputDebugF("Application called xrGetVulkanDeviceExtensionsKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetVulkanGraphicsDeviceKHR(XrInstance instance, XrSystemId systemId, VkInstance vkInstance, VkPhysicalDevice* vkPhysicalDevice)
{
    outputDebugF("Application called xrGetVulkanGraphicsDeviceKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrGetVulkanGraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkanKHR* graphicsRequirements)
{
    outputDebugF("Application called xrGetVulkanGraphicsRequirementsKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
#endif
XrResult xrGetD3D12GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D12KHR* graphicsRequirements)
{
    outputDebugF("Application called xrGetD3D12GraphicsRequirementsKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrConvertWin32PerformanceCounterToTimeKHR(XrInstance instance, const LARGE_INTEGER* performanceCounter, XrTime* time)
{
    outputDebugF("Application called xrConvertWin32PerformanceCounterToTimeKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrConvertTimeToWin32PerformanceCounterKHR(XrInstance instance, XrTime time, LARGE_INTEGER* performanceCounter)
{
    outputDebugF("Application called xrConvertTimeToWin32PerformanceCounterKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrConvertTimespecTimeToTimeKHR(XrInstance instance, const struct timespec* timespecTime, XrTime* time)
{
    outputDebugF("Application called xrConvertTimespecTimeToTimeKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
XrResult xrConvertTimeToTimespecTimeKHR(XrInstance instance, XrTime time, struct timespec* timespecTime)
{
    outputDebugF("Application called xrConvertTimeToTimespecTimeKHR but that's unimplemented.\n");
    return XR_ERROR_RUNTIME_FAILURE;
}
