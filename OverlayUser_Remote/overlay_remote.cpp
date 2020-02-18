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


#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <chrono>
#include <thread>

#include <cassert>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // !WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#define NOMINMAX
#endif  // !NOMINMAX

#include <windows.h>

#include <dxgi1_2.h>
#include <d3d11_4.h>

//
// OpenXR Headers
//
#undef XR_USE_GRAPHICS_API_D3D12
#undef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_D3D11 1

#define COMPILE_REMOTE_OVERLAY_APP 1

#if COMPILE_REMOTE_OVERLAY_APP
#include "../XR_overlay_ext/xr_overlay_dll.h"
#else // undef COMPILE_REMOTE_OVERLAY_APP
#include <openxr/openxr.h>
XR_DEFINE_ATOM(XrPermissionIdEXT)
#endif // COMPILE_REMOTE_OVERLAY_APP

#include <openxr/openxr_platform.h>


// Fun images for page-flipping the quad
extern "C" {
    extern int Image2Width;
    extern int Image2Height;
    extern unsigned char Image2Bytes[];
    extern int Image1Width;
    extern int Image1Height;
    extern unsigned char Image1Bytes[];
};


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
        vsnprintf(buf.get(), provided, fmt, args);
        va_end(args);

        return std::string(buf.get());
    }
    return "(fmt() failed, vsnprintf returned -1)";
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

// Use this macro to test Direct3D functions
#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

// Use this macro to test OpenXR functions
#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

const uint64_t ONE_SECOND_IN_NANOSECONDS = 1000000000;

ID3D11Device* GetD3D11DeviceFromAdapter(LUID adapterLUID)
{
    IDXGIFactory1 * pFactory;
    CHECK(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&pFactory)));

    UINT i = 0; 
    IDXGIAdapter * pAdapter = NULL; 
    bool found = false;
    while(!found && (pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND)) { 
        DXGI_ADAPTER_DESC desc;
        CHECK(pAdapter->GetDesc(&desc));
        if((desc.AdapterLuid.LowPart == adapterLUID.LowPart) && (desc.AdapterLuid.HighPart == adapterLUID.HighPart)) {
            found = true;
        }
        ++i; 
    } 
    if(!found)
        abort();

    ID3D11Device* d3d11Device;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1};
    D3D_FEATURE_LEVEL featureLevel;
    CHECK(D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG, levels,
        1, D3D11_SDK_VERSION, &d3d11Device, &featureLevel, NULL));
    if (featureLevel != D3D_FEATURE_LEVEL_11_1)
    {
        OutputDebugStringA("Direct3D Feature Level 11.1 not created\n");
        abort();
    }
    return d3d11Device;
}

void GetInstanceExtensions(std::map<std::string, uint32_t>& extensionMap)
{
// #if !COMPILE_REMOTE_OVERLAY_APP
    // XXX Layer doesn't support calling downchain->EnumerateInstanceExtensionProperties
    uint32_t extPropCount;
    CHECK_XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extPropCount, nullptr));
    if(extPropCount > 0) {
        std::vector<XrExtensionProperties> extensionProperties(extPropCount);
        for(auto& p: extensionProperties) {
            p.type = XR_TYPE_EXTENSION_PROPERTIES;
            p.next = nullptr;
        }
        CHECK_XR(xrEnumerateInstanceExtensionProperties(nullptr, extPropCount, &extPropCount, extensionProperties.data()));
        for(auto& p: extensionProperties) {
            extensionMap.insert({p.extensionName, p.extensionVersion});
        }
    }
// #endif // COMPILE_REMOTE_OVERLAY_APP
}

void CreateInstance(const std::string& appName, uint32_t appVersion, const std::string& engineName, uint32_t engineVersion, uint64_t apiVersion, std::vector<const char*> extensionNames, XrInstance* instance)
{
    XrInstanceCreateInfo createInstance{XR_TYPE_INSTANCE_CREATE_INFO};
    createInstance.next = nullptr;
    createInstance.createFlags = 0;
    strncpy_s(createInstance.applicationInfo.applicationName, appName.c_str(), appName.size() + 1);
    createInstance.applicationInfo.applicationVersion = appVersion;
    strncpy_s(createInstance.applicationInfo.engineName, engineName.c_str(), engineName.size() + 1);
    createInstance.applicationInfo.engineVersion = engineVersion;
	createInstance.applicationInfo.apiVersion = apiVersion;
    createInstance.enabledExtensionNames = extensionNames.data();
    createInstance.enabledExtensionCount = (uint32_t) extensionNames.size();
    createInstance.enabledApiLayerCount = 0;
    createInstance.enabledApiLayerNames = nullptr;
    CHECK_XR(xrCreateInstance(&createInstance, instance));
}

XrBool32 processDebugMessage(
    XrDebugUtilsMessageSeverityFlagsEXT /* messageSeverity */,
    XrDebugUtilsMessageTypeFlagsEXT /* messageTypes */,
    const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /* userData */)
{
    std::cout << "XR: " << callbackData->message << "\n";

    // typedef struct XrDebugUtilsMessengerCallbackDataEXT {
        // XrStructureType type;
        // const void* next;
        // const char* messageId;
        // const char* functionName;
        // const char* message;
        // uint32_t objectCount;
        // XrDebugUtilsObjectNameInfoEXT* objects;
        // uint32_t sessionLabelCount;
        // XrDebugUtilsLabelEXT* sessionLabels;
    // } XrDebugUtilsMessengerCallbackDataEXT;

    return XR_FALSE;
}

void CreateDebugMessenger(XrInstance instance, XrDebugUtilsMessengerEXT* messenger)
{
    PFN_xrCreateDebugUtilsMessengerEXT createDebugUtilsMessenger;
    CHECK_XR(xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
        reinterpret_cast<PFN_xrVoidFunction*>(&createDebugUtilsMessenger)));

    XrDebugUtilsMessengerCreateInfoEXT dumCreateInfo { XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    dumCreateInfo.messageSeverities = 
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dumCreateInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT |
       XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dumCreateInfo.userCallback = processDebugMessage;
    dumCreateInfo.userData = nullptr;
    CHECK_XR(createDebugUtilsMessenger(instance, &dumCreateInfo, messenger));
}

void CreateSession(ID3D11Device* d3d11Device, XrInstance instance, XrSystemId systemId, XrSession* session, bool usingOverlays, bool requestPosePermission)
{
    void* chain = nullptr;

    XrGraphicsBindingD3D11KHR d3dBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    d3dBinding.device = d3d11Device;
    chain = &d3dBinding;

    if(usingOverlays) {
#if COMPILE_REMOTE_OVERLAY_APP
        XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
        sessionCreateInfoOverlay.next = chain;
        sessionCreateInfoOverlay.createFlags = 0;
        sessionCreateInfoOverlay.sessionLayersPlacement = 1; 
        chain = &sessionCreateInfoOverlay;
#endif  // COMPILE_REMOTE_OVERLAY_APP
    }  

    if(requestPosePermission) {
#if COMPILE_REMOTE_OVERLAY_APP
        bool unfocusedPosePermissionAvailable = false;
        XrPermissionIdEXT unfocusedPosePermissionId = 0;
        PFN_xrEnumerateInstancePermissionsEXT enumerateInstancePermissionsEXT;
        CHECK_XR(xrGetInstanceProcAddr(instance, "xrEnumerateInstancePermissionsEXT",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&enumerateInstancePermissionsEXT)));
        uint32_t count;
        CHECK_XR(enumerateInstancePermissionsEXT(instance, 0, &count, nullptr));
        std::vector<XrPermissionPropertiesEXT> properties(count);
        CHECK_XR(enumerateInstancePermissionsEXT(instance, count, &count, properties.data()));
        for (auto& p : properties) {
            if (std::string(p.permissionName) == "XR_EXT_permissions_support") {
                unfocusedPosePermissionAvailable = true;
                unfocusedPosePermissionId = p.permissionId;
            }
        }

        XrSessionCreateInfoPermissionsEXT sessionCreateInfoPermissions{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_PERMISSIONS_EXT};
        std::vector<XrPermissionRequestEXT> permissions;
        XrPermissionRequestEXT posePermission {(XrStructureType)XR_TYPE_PERMISSION_REQUEST_EXT, nullptr, unfocusedPosePermissionId, false};
        permissions.push_back(posePermission);
        sessionCreateInfoPermissions.next = chain;
        sessionCreateInfoPermissions.requestedPermissionsCount = (uint32_t)permissions.size();
        sessionCreateInfoPermissions.requestedPermissions = permissions.data(); 
        chain = &sessionCreateInfoPermissions;
#endif  // COMPILE_REMOTE_OVERLAY_APP
    }

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.systemId = systemId;
    sessionCreateInfo.next = chain;

    CHECK_XR(xrCreateSession(instance, &sessionCreateInfo, session));
}

void CreateSpace(XrSession session, XrReferenceSpaceType spaceType, const XrPosef& pose, XrSpace* viewSpace)
{
    XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    createSpaceInfo.next = nullptr;
    createSpaceInfo.referenceSpaceType = spaceType;
    createSpaceInfo.poseInReferenceSpace = pose;
    CHECK_XR(xrCreateReferenceSpace(session, &createSpaceInfo, viewSpace));
}

void ChooseSwapchainFormat(XrSession session, uint64_t* chosenFormat)
{
    uint32_t count;
    CHECK_XR(xrEnumerateSwapchainFormats(session, 0, &count, nullptr));
    std::vector<int64_t> runtimeFormats(count);
    CHECK_XR(xrEnumerateSwapchainFormats(session, (uint32_t)count, &count, runtimeFormats.data()));
    std::vector<DXGI_FORMAT> appFormats { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
    auto formatFound = std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), appFormats.begin(), appFormats.end());
    if(formatFound == runtimeFormats.end()) {
        OutputDebugStringA("No supported swapchain format found\n");
        DebugBreak();
    }
    *chosenFormat = *formatFound;
}

void FindRecommendedDimensions(XrInstance instance, XrSystemId systemId, int32_t* recommendedWidth, int32_t* recommendedHeight)
{
    uint32_t count;
    CHECK_XR(xrEnumerateViewConfigurations(instance, systemId, 0, &count, nullptr));
    std::vector<XrViewConfigurationType> viewConfigurations(count);
    CHECK_XR(xrEnumerateViewConfigurations(instance, systemId, count, &count, viewConfigurations.data()));
    bool found = false;
    for(uint32_t i = 0; i < count; i++) {
        if(viewConfigurations[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        found = true;
    }
    if(!found) {
        std::cerr << "Failed to find XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO in " << count << " view configurations\n";
        abort();
    }

    XrViewConfigurationProperties configurationProperties {XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
    CHECK_XR(xrGetViewConfigurationProperties(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &configurationProperties));

    CHECK_XR(xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr));
    std::vector<XrViewConfigurationView> viewConfigurationViews(count);
    for(uint32_t i = 0; i < count; i++) {
        viewConfigurationViews[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        viewConfigurationViews[i].next = nullptr;
    }
    CHECK_XR(xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count, viewConfigurationViews.data()));
    // Be lazy and set recommended sizes to left eye
    *recommendedWidth = viewConfigurationViews[0].recommendedImageRectWidth;
    *recommendedHeight = viewConfigurationViews[0].recommendedImageRectHeight;
}

void CreateSwapchainsAndGetImages(XrSession session, uint64_t chosenFormat, int32_t recommendedWidth, int32_t recommendedHeight, XrSwapchain swapchains[2], XrSwapchainImageD3D11KHR *swapchainImages[2])
{
    for(int eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = chosenFormat;
        swapchainCreateInfo.width = recommendedWidth;
        swapchainCreateInfo.height = recommendedHeight;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = 1;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.next = nullptr;
        CHECK_XR(xrCreateSwapchain(session, &swapchainCreateInfo, &swapchains[eye]));

        uint32_t count;
        CHECK_XR(xrEnumerateSwapchainImages(swapchains[eye], 0, &count, nullptr));
        swapchainImages[eye] = new XrSwapchainImageD3D11KHR[count];
        for(uint32_t i = 0; i < count; i++) {
            swapchainImages[eye][i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            swapchainImages[eye][i].next = nullptr;
        }
        CHECK_XR(xrEnumerateSwapchainImages(swapchains[eye], count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages[eye])));
    }
}

void CreateSourceImages(ID3D11Device* d3d11Device, ID3D11DeviceContext* d3dContext, int32_t recommendedWidth, int32_t recommendedHeight, ID3D11Texture2D* sourceImages[2], DXGI_FORMAT format)
{
	assert(
		(format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) ||
		(format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) ||
		(format == DXGI_FORMAT_R8G8B8A8_UNORM) ||
		(format == DXGI_FORMAT_B8G8R8A8_UNORM)
	);

    for(int i = 0; i < 2; i++) {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = recommendedWidth;
        desc.Height = recommendedHeight;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = (UINT)D3D11_STANDARD_MULTISAMPLE_PATTERN;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        CHECK(d3d11Device->CreateTexture2D(&desc, NULL, &sourceImages[i]));

        D3D11_MAPPED_SUBRESOURCE mapped;
        CHECK(d3dContext->Map(sourceImages[i], 0, D3D11_MAP_WRITE, 0, &mapped));

        unsigned char *imageBytes = (i == 0) ? Image1Bytes : Image2Bytes;
        int width = (i == 0) ? Image1Width : Image2Width;
        int height = (i == 0) ? Image1Height : Image2Height;
        for(int32_t y = 0; y < recommendedHeight; y++)
            for(int32_t x = 0; x < recommendedWidth; x++) {
                int srcX = x * width / recommendedWidth;
                int srcY = y * height / recommendedHeight;
                unsigned char *dst = reinterpret_cast<unsigned char*>(mapped.pData) + mapped.RowPitch * y + 4 * x;
                unsigned char *src = imageBytes + 4 * (width * srcY + srcX);
                // Source data is in RGBA
                if(format == DXGI_FORMAT_R8G8B8A8_UNORM || DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) { 
                    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
                } else if(format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) { 
                    dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3];
                }
            }
        d3dContext->Unmap(sourceImages[i], 0);
    }
}

void FillSwapchainImage(XrSwapchain swapchain, XrSwapchainImageD3D11KHR *swapchainImages, ID3D11Texture2D *sourceImage, ID3D11DeviceContext* d3dContext)
{
    uint32_t index;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    acquireInfo.next = nullptr;

    CHECK_XR(xrAcquireSwapchainImage(swapchain, &acquireInfo, &index));

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.next = nullptr;
    waitInfo.timeout = ONE_SECOND_IN_NANOSECONDS;
    CHECK_XR(xrWaitSwapchainImage(swapchain, &waitInfo));

    d3dContext->CopyResource(swapchainImages[index].texture, sourceImage);

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    releaseInfo.next = nullptr;
    CHECK_XR(xrReleaseSwapchainImage(swapchain, &releaseInfo));
}

std::map<XrSessionState, std::string> SessionStateToString {
    {XR_SESSION_STATE_IDLE, "XR_SESSION_STATE_IDLE"},
    {XR_SESSION_STATE_READY, "XR_SESSION_STATE_READY"},
    {XR_SESSION_STATE_SYNCHRONIZED, "XR_SESSION_STATE_SYNCHRONIZED"},
    {XR_SESSION_STATE_VISIBLE, "XR_SESSION_STATE_VISIBLE"},
    {XR_SESSION_STATE_FOCUSED, "XR_SESSION_STATE_FOCUSED"},
    {XR_SESSION_STATE_STOPPING, "XR_SESSION_STATE_STOPPING"},
    {XR_SESSION_STATE_LOSS_PENDING, "XR_SESSION_STATE_LOSS_PENDING"},
    {XR_SESSION_STATE_EXITING, "XR_SESSION_STATE_EXITING"},
};

void ProcessSessionStateChangedEvent(XrEventDataBuffer* event, bool* quit, XrSession session, bool& runFrameLoop, XrSessionState& sessionState)
{
    const auto& e = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
    if(e.session == session) {
        if(false) std::cout << "transition to " << SessionStateToString.at(e.state) << "\n";

        // Handle state change of our session
        if((e.state == XR_SESSION_STATE_EXITING) ||
           (e.state == XR_SESSION_STATE_LOSS_PENDING)) {

            *quit = true;
            runFrameLoop = false;

        } else {

            switch(sessionState) {
                case XR_SESSION_STATE_IDLE: {
                    if(e.state == XR_SESSION_STATE_READY) {
                        XrSessionBeginInfo beginInfo {XR_TYPE_SESSION_BEGIN_INFO, nullptr, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
                        CHECK_XR(xrBeginSession(session, &beginInfo));
                        std::cout << "running\n";
                        runFrameLoop = true;
                    }
                    // ignore other transitions
                    break;
                }
                case XR_SESSION_STATE_READY: {
                    // ignore
                    break;
                }
                case XR_SESSION_STATE_SYNCHRONIZED: {
                    if(e.state == XR_SESSION_STATE_STOPPING) {
                        CHECK_XR(xrEndSession(session));
                        std::cout << "not running\n";
                        runFrameLoop = false;
                    }
                    // ignore other transitions
                    break;
                }
                case XR_SESSION_STATE_VISIBLE: {
                    // ignore
                    break;
                }
                case XR_SESSION_STATE_FOCUSED: {
                    // ignore
                    break;
                }
                case XR_SESSION_STATE_STOPPING: {
                    // ignore
                    break;
                }
                default: {
                    std::cout << "Warning: ignored unknown new session state " << e.state << "\n";
                    break;
                }
            }

        }
        sessionState = e.state;
    }
}

void ProcessEvent(XrEventDataBuffer *event, bool* quit, XrSession session, bool& runFrameLoop, XrSessionState& sessionState)
{
    switch(event->type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            // const auto& e = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
            *quit = true;
            break;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            ProcessSessionStateChangedEvent(event, quit, session, runFrameLoop, sessionState);
        }
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            // const auto& e = *reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(event);
            // Handle reference space change pending
            break;
        }
        case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
            const auto& e = *reinterpret_cast<const XrEventDataEventsLost*>(event);
            std::cout << "Warning: lost " << e.lostEventCount << " events\n";
            break;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
            // const auto& e = *reinterpret_cast<const XrEventDataInteractionProfileChanged*>(event);
            // Handle data interaction profile changed
            break;
        }
        default: {
            std::cout << "Warning: ignoring event type " << event->type << "\n";
            break;
        }
    }
}

void SetLayer(XrCompositionLayerQuad *layer, XrSpace space, XrEyeVisibility visibility, const XrSwapchainSubImage& subImage, XrPosef pose, const XrExtent2Df& extent)
{
    layer->type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    layer->next = nullptr;
    layer->layerFlags = 0;
    layer->space = space;
    layer->eyeVisibility = visibility;
    layer->subImage = subImage;
    layer->pose = pose;
    layer->size = extent;
}

int main( void )
{
    bool sawFirstSuccessfulFrame = false;
    bool useDebugMessenger = false;
    bool usePermissions = false;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    std::cout << "Connecting to Host OpenXR Application...\n";
#if COMPILE_REMOTE_OVERLAY_APP
    IPCConnectResult connectResult = IPC_CONNECT_TIMEOUT;
    do {
        connectResult = IPCXrConnectToHost();
        if(connectResult == IPC_CONNECT_TIMEOUT) {
            std::cout << "Connection to Host OpenXR Application timed out.  Attempting again.\n";
        } else if(connectResult == IPC_CONNECT_ERROR) {
            std::cerr << "Connection to Host OpenXR Application Failed other than timing out.  Exiting\n";
            exit(EXIT_FAILURE);
        }
    } while(connectResult == IPC_CONNECT_TIMEOUT);
#endif

    bool createOverlaySession = COMPILE_REMOTE_OVERLAY_APP;

    std::map<std::string, uint32_t> extensions;
    GetInstanceExtensions(extensions);
    if(extensions.size() > 0) {
        std::cout << "Extensions supported:\n";
        for(const auto& p: extensions) {
            std::cout << "    " << p.first << ", version " << p.second << "\n";
#if !COMPILE_REMOTE_OVERLAY_APP
			if(std::string(p.first) == "XR_EXT_debug_utils") {
                useDebugMessenger = true;
            }
#endif
			if(std::string(p.first) == "XR_EXT_permissions_support") {
                usePermissions = true;
            }
        }
    } else {
        std::cout << "No extensions supported.\n";
    }

    XrInstance instance;

    std::vector<const char*> extensionNames;
    extensionNames.push_back("XR_KHR_D3D11_enable");
#if COMPILE_REMOTE_OVERLAY_APP
    extensionNames.push_back(XR_EXT_OVERLAY_PREVIEW_EXTENSION_NAME);
#endif  // COMPILE_REMOTE_OVERLAY_APP
    if(useDebugMessenger) {
        extensionNames.push_back("XR_EXT_debug_utils");
    }
    if(usePermissions) {
        extensionNames.push_back("XR_EXT_permissions_support");
    }

    CreateInstance("Overlay Sample", 0, "none", 0, XR_MAKE_VERSION(1, 0, 0), extensionNames, &instance);
    std::cout << "CreateInstance succeeded!\n";

    XrDebugUtilsMessengerEXT messenger = XR_NULL_HANDLE;
    if(useDebugMessenger) {
        CreateDebugMessenger(instance, &messenger);
    }

    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES, nullptr};
    CHECK_XR(xrGetInstanceProperties(instance, &instanceProperties));
    std::cout << "Runtime \"" << instanceProperties.runtimeName << "\", version " <<
	XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "." <<
	XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "p" <<
	XR_VERSION_PATCH(instanceProperties.runtimeVersion) << "\n";

    XrSystemId systemId;
    XrSystemGetInfo getSystem{XR_TYPE_SYSTEM_GET_INFO, nullptr, XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
    CHECK_XR(xrGetSystem(instance, &getSystem, &systemId));

    PFN_xrGetD3D11GraphicsRequirementsKHR getD3D11GraphicsRequirementsKHR;
    CHECK_XR(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&getD3D11GraphicsRequirementsKHR)));

    XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    CHECK_XR(getD3D11GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));

    ID3D11Device* d3d11Device = GetD3D11DeviceFromAdapter(graphicsRequirements.adapterLuid);

    XrSession session;

    CreateSession(d3d11Device, instance, systemId, &session, createOverlaySession, usePermissions);
    std::cout << "CreateSession with XrSessionCreateInfoOverlayEXT succeeded!\n";

    XrSpace viewSpace;
    CreateSpace(session, XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{{0.0, 0.0, 0.0, 1.0}, {0.0, 0.0, 0.0}}, &viewSpace);
    XrSpace localSpace;
    CreateSpace(session, XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosef{{0.0, 0.0, 0.0, 1.0}, {0.0, 0.0, 0.0}}, &localSpace);
    XrSpace stageSpace;
    CreateSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE, XrPosef{{0.0, 0.0, 0.0, 1.0}, {0.0, 0.0, 0.0}}, &stageSpace);

    uint64_t chosenFormat;
    ChooseSwapchainFormat(session, &chosenFormat);

    int32_t recommendedWidth, recommendedHeight;
    FindRecommendedDimensions(instance, systemId, &recommendedWidth, &recommendedHeight);
    std::cout << "Recommended view image dimensions are " << recommendedWidth << " by " << recommendedHeight << "\n";

    XrSwapchain swapchains[2];
    XrSwapchainImageD3D11KHR *swapchainImages[2];
    CreateSwapchainsAndGetImages(session, chosenFormat, recommendedWidth, recommendedHeight, swapchains, swapchainImages);

    ID3D11DeviceContext* d3dContext;
    d3d11Device->GetImmediateContext(&d3dContext);

    ID3D11Texture2D* sourceImages[2];
    CreateSourceImages(d3d11Device, d3dContext, recommendedWidth, recommendedHeight, sourceImages, (DXGI_FORMAT)chosenFormat);

    std::cout << "Created Swapchain and enumerated SwapchainImages and made local\n";
    std::cout << "    images as texture sources!\n";

    // Spawn a thread to wait for a keypress
    static bool requestExit = false;
    static bool exitRequested = false;
    auto exitPollingThread = std::thread{[] {
        std::cout << "Press ENTER to exit...\n";
        getchar();
        requestExit = true;
    }};
    exitPollingThread.detach();

    XrSystemProperties systemProperties { XR_TYPE_SYSTEM_PROPERTIES };
    CHECK_XR(xrGetSystemProperties(instance, systemId, &systemProperties));
    std::cout << "System \"" << systemProperties.systemName << "\", vendorId " << systemProperties.vendorId << "\n";

    bool useSeparateLeftRightEyes;
    if(systemProperties.graphicsProperties.maxLayerCount >= 2) {
        useSeparateLeftRightEyes = true;
    } else if(systemProperties.graphicsProperties.maxLayerCount >= 1) {
        useSeparateLeftRightEyes = false;
    } else {
        std::cerr << "xrGetSystemProperties reports maxLayerCount 0, no way to display a compositor layer\n";
        abort();
    }

    // OpenXR Frame loop

    int whichImage = 0;
    auto then = std::chrono::steady_clock::now();
    XrSessionState sessionState = XR_SESSION_STATE_IDLE;
    bool runFrameLoop = false;
    XrSpace myspace = localSpace;

    bool quit = false;

    do {
        auto now = std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count() > 1000) {
            whichImage = (whichImage + 1) % 2;
            then = std::chrono::steady_clock::now();
        }

        if(requestExit && !exitRequested) {

            if((sessionState == XR_SESSION_STATE_SYNCHRONIZED) || 
               (sessionState == XR_SESSION_STATE_VISIBLE) || 
               (sessionState == XR_SESSION_STATE_STOPPING) || 
               (sessionState == XR_SESSION_STATE_FOCUSED)) {

                // If we are "running" and haven't yet requested an
                // exit, play nice by requesting exit and perform no other
                // actions, state machine should carry us to EXITING
                // state, which we will get and set quit = true.

                CHECK_XR(xrRequestExitSession(session));
                exitRequested = true;

            } else {

                // If we are not "running", we can't request an exit.
                // But we can just destroy our session and quit.
                // (from any state, actually)

                exitRequested = true;
                quit = true;
            }
        }

        bool getAnotherEvent = true;
        while (getAnotherEvent && !quit) {
            static XrEventDataBuffer event;
            event.type = XR_TYPE_EVENT_DATA_BUFFER;
            event.next = nullptr;
            XrResult result = xrPollEvent(instance, &event);
            if(result == XR_SUCCESS) {
                ProcessEvent(&event, &quit, session, runFrameLoop, sessionState);
            } else {
                getAnotherEvent = false;
            }
        }

        if(!quit) {
            if(runFrameLoop) {

                XrFrameState waitFrameState{ XR_TYPE_FRAME_STATE };
                CHECK_XR(xrWaitFrame(session, nullptr, &waitFrameState));
                if(false) printf("frameState: { %llu, %llu, %s }\n", waitFrameState.predictedDisplayTime, waitFrameState.predictedDisplayPeriod, waitFrameState.shouldRender ? "yes" : "no");

                XrSpaceLocation viewPose { XR_TYPE_SPACE_LOCATION, nullptr };
                CHECK_XR(xrLocateSpace(viewSpace, localSpace, waitFrameState.predictedDisplayTime, &viewPose));
                if(false) printf("pose {{%f, %f, %f, %f}, {%f, %f, %f}}\n",
                        viewPose.pose.orientation.x,
                        viewPose.pose.orientation.y,
                        viewPose.pose.orientation.z,
                        viewPose.pose.orientation.w,
                        viewPose.pose.position.x,
                        viewPose.pose.position.y,
                        viewPose.pose.position.z);

                CHECK_XR(xrBeginFrame(session, nullptr));

                XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO, nullptr};
                frameEndInfo.displayTime = waitFrameState.predictedDisplayTime;
                frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

                if(waitFrameState.shouldRender) {
                    for(int eye = 0; eye < 2; eye++) {
                        FillSwapchainImage(swapchains[eye], swapchainImages[eye], sourceImages[whichImage], d3dContext);
                    }

                    d3dContext->Flush();

                    XrCompositionLayerQuad layers[2];
                    XrCompositionLayerBaseHeader* layerPointers[2];
                    frameEndInfo.layers = layerPointers;

                    XrPosef pose = XrPosef{{0.0, 0.0, 0.0, 1.0}, {-0.25f, 0.125f, -1.5f}};
                    
                    if(useSeparateLeftRightEyes) {

                        for(uint32_t eye = 0; eye < 2; eye++) {
                            XrSwapchainSubImage fullImage {swapchains[eye], {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                            SetLayer(&layers[eye], myspace, (eye == 0) ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT, fullImage, pose, {0.33f, 0.33f});
                            layerPointers[eye] = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layers[eye]);
                        }

                        frameEndInfo.layerCount = 2;

                    } else {

                        XrSwapchainSubImage fullImage {swapchains[0], {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                        SetLayer(&layers[0], myspace, XR_EYE_VISIBILITY_BOTH, fullImage, pose, {0.33f, 0.33f});

                        layerPointers[0] = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layers[0]);
                        frameEndInfo.layerCount = 1;
                    }
                }

                CHECK_XR(xrEndFrame(session, &frameEndInfo));
                
                if(!sawFirstSuccessfulFrame) {
                    sawFirstSuccessfulFrame = true;
                    std::cout << "First Overlay xrEndFrame was successful!  Continuing...\n";
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
    } while(!quit);

    if(false) {
        // These are destroyed by xrDestroySession but written here for clarity
        for(int eye = 0; eye < 2; eye++) {
            CHECK_XR(xrDestroySwapchain(swapchains[eye]));
        }

        CHECK_XR(xrDestroySpace(stageSpace));
        CHECK_XR(xrDestroySpace(localSpace));
        CHECK_XR(xrDestroySpace(viewSpace));
    }

    CHECK_XR(xrDestroySession(session));

    return 0;
}
