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


#define _CRT_SECURE_NO_WARNINGS 1

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <array>

#include <cassert>

// To compile this application as a "Main" OpenXR app (no overlays) set this to 0:
#define COMPILE_REMOTE_OVERLAY_APP 1

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

#if COMPILE_REMOTE_OVERLAY_APP
#include "../XR_overlay_ext/xr_overlay_dll.h"
#else // undef COMPILE_REMOTE_OVERLAY_APP
#include <openxr/openxr.h>
XR_DEFINE_ATOM(XrPermissionIdEXT)
#endif // COMPILE_REMOTE_OVERLAY_APP

#include <openxr/openxr_platform.h>

#include <FreeImagePlus.h>

#include "../include/util.h"


const uint64_t ONE_SECOND_IN_NANOSECONDS = 1000000000;


// OpenXR will give us a LUID.  This function will walk adapters to find
// the adapter matching that LUID, then create an ID3D11Device* from it so
// we can issue D3D11 commands.

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
        outputDebugF("Direct3D Feature Level 11.1 not created\n");
        abort();
    }
    return d3d11Device;
}


// Load FreeImagePlus "fipImage"s from a list of filenames

void LoadImages(const std::vector<std::string>& imageFilenames, std::vector<fipImage>& images)
{
    for(auto filename: imageFilenames) {
        images.push_back(fipImage());
        fipImage& image = images.back();
        // XXX std::move instead
        if (!image.load(filename.c_str())) {
            std::cerr << "failed to load image " << filename << "\n";
            exit(EXIT_FAILURE);
        }
        image.convertTo32Bits();
    }
}


// Create D3D11 textures from existing FreeImagePlus images, with the
// specified format, scaled to the specified size.

void CreateSourceTextures(ID3D11Device* d3d11Device, ID3D11DeviceContext* d3dContext, int32_t width, int32_t height, std::vector<fipImage> images, std::vector<ID3D11Texture2D*>& sourceTextures, DXGI_FORMAT format)
{
    assert(
        (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) ||
        (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) ||
        (format == DXGI_FORMAT_R8G8B8A8_UNORM) ||
        (format == DXGI_FORMAT_B8G8R8A8_UNORM)
    );

    for(int i = 0; i < images.size(); i++) {
        fipImage& image = images[i];

        ID3D11Texture2D* texture;
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = (UINT)D3D11_STANDARD_MULTISAMPLE_PATTERN;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        CHECK(d3d11Device->CreateTexture2D(&desc, NULL, &texture));
        sourceTextures.push_back(texture);

        D3D11_MAPPED_SUBRESOURCE mapped;
        CHECK(d3dContext->Map(sourceTextures[i], 0, D3D11_MAP_WRITE, 0, &mapped));

        image.rescale(width, height, FILTER_BILINEAR);

        for (int32_t y = 0; y < height; y++) {
            unsigned char *src = image.getScanLine(height - 1 - y);
            unsigned char *dst = reinterpret_cast<unsigned char*>(mapped.pData) + mapped.RowPitch * y;

            if (format == DXGI_FORMAT_R8G8B8A8_UNORM || DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
                for (int32_t x = 0; x < width; x++) {
                    unsigned char *srcPixel = src + x * 4;
                    unsigned char *dstPixel = dst + x * 4;
                    dstPixel[0] = srcPixel[2];
                    dstPixel[1] = srcPixel[1];
                    dstPixel[2] = srcPixel[0];
                    dstPixel[3] = srcPixel[3];
                }
            } else if (format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                for (int32_t x = 0; x < width; x++) {
                    unsigned char *srcPixel = src + x * 4;
                    unsigned char *dstPixel = dst + x * 4;
                    dstPixel[0] = srcPixel[0];
                    dstPixel[1] = srcPixel[1];
                    dstPixel[2] = srcPixel[2];
                    dstPixel[3] = srcPixel[3];
                }
            }
        }
        d3dContext->Unmap(sourceTextures[i], 0);
    }
}


//----------------------------------------------------------------------------
// Class for calling OpenXR commands and tracking OpenXR-related State

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

class OpenXRProgram
{
public:
    OpenXRProgram(bool requestOverlaySession) :
        mRequestOverlaySession(requestOverlaySession)
    {
        // nothing
    }
    ~OpenXRProgram()
    {
        if(false) {
            // These are destroyed by xrDestroySession but written here for clarity
            for(int eye = 0; eye < 2; eye++) {
                if(mSwapchains[eye] != XR_NULL_HANDLE) {
                    CHECK_XR(xrDestroySwapchain(mSwapchains[eye]));
                }
            }

            if(mContentSpace != XR_NULL_HANDLE) {
                CHECK_XR(xrDestroySpace(mContentSpace));
            }
        }

        if(mSession != XR_NULL_HANDLE) {
            CHECK_XR(xrDestroySession(mSession));
		}
    }

    void CreateInstance(const std::string& appName, uint32_t appVersion, const std::string& engineName, uint32_t engineVersion);
    void GetSystem();
    void CreateSession(ID3D11Device* d3d11Device, bool requestOverlaySession, bool requestPosePermission);
	LUID GetD3D11AdapterLUID();
    bool ChooseBestPixelFormat(const std::vector<DXGI_FORMAT>& appFormats, uint64_t *chosenFormat);
    void FindRecommendedDimensions();
    void CreateSwapChainsAndGetImages(DXGI_FORMAT format);
    void CreateContentSpace();
    void RequestExitSession();
    void ProcessEvent(XrEventDataBuffer *event, bool& quit, bool& doFrame);
    void ProcessSessionStateChangedEvent(XrEventDataBuffer* event, bool& quit, bool& doFrame);
    void ProcessEvents(bool& quit, bool &doFrame);
    uint32_t AcquireAndWaitSwapchainImage(int eye);
    void ReleaseSwapchainImage(int eye);
    bool WaitFrame();
    void BeginFrame();
    void AddLayer(XrCompositionLayerFlags flags, XrEyeVisibility visibility, const XrSwapchainSubImage& subImage, XrPosef pose, const XrExtent2Df& extent);
    void EndFrame();

    XrInstance GetInstance() const { return mInstance; }
    XrSystemId GetSystemId() const { return mSystemId; }
    XrSession GetSession() const { return mSession; }
    XrSpace GetContentSpace() const { return mContentSpace; }
    uint32_t GetMaxLayerCount() const { return mMaxLayerCount; }
    XrSwapchain GetSwapchain(int swapchain) const { return mSwapchains[swapchain]; }
    const XrSwapchainImageD3D11KHR& GetSwapchainImage(int swapchain, int image) const { return mSwapchainImages[swapchain][image]; }
    std::array<int32_t, 2> GetRecommendedDimensions() const { return mRecommendedDimensions; }

    bool IsDebugUtilsAvailable() const { return mDebugUtilsAvailable; }
    bool IsPermissionsSupportAvailable() const { return mPermissionsSupportAvailable; }
    bool IsRunning();

private:
    bool        mRequestOverlaySession;
    bool        mDebugUtilsAvailable;
    bool        mPermissionsSupportAvailable;
    XrInstance  mInstance;
    XrSystemId  mSystemId;
    XrSession   mSession;
    XrSwapchain mSwapchains[2];
    XrSwapchainImageD3D11KHR    *mSwapchainImages[2];
    std::array<int32_t, 2>      mRecommendedDimensions;
    XrSpace     mContentSpace;
    uint32_t    mMaxLayerCount;
    XrSessionState      mSessionState;
    XrFrameState        mWaitFrameState;
    std::vector<XrCompositionLayerQuad>      mLayers;

};


void OpenXRProgram::CreateInstance(const std::string& appName, uint32_t appVersion, const std::string& engineName, uint32_t engineVersion)
{
    std::map<std::string, uint32_t> extensions;

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
            extensions.insert({p.extensionName, p.extensionVersion});
        }
    }

    if(extensions.size() > 0) {
        std::cout << "Extensions supported:\n";
        for(const auto& p: extensions) {
            std::cout << "    " << p.first << ", version " << p.second << "\n";
#if !COMPILE_REMOTE_OVERLAY_APP
            if(std::string(p.first) == "XR_EXT_debug_utils") {
                mDebugUtilsAvailable = true;
            }
#endif
            if(std::string(p.first) == "XR_EXT_permissions_support") {
                mPermissionsSupportAvailable = true;
            }
        }
    } else {
        std::cout << "Warning: No extensions supported.\n";
    }

    std::vector<const char*> requiredExtensionNames;
    requiredExtensionNames.push_back("XR_KHR_D3D11_enable");
#if COMPILE_REMOTE_OVERLAY_APP
    if(mRequestOverlaySession) {
        requiredExtensionNames.push_back(XR_EXT_OVERLAY_PREVIEW_EXTENSION_NAME);
    }
#endif  // COMPILE_REMOTE_OVERLAY_APP
    if(mDebugUtilsAvailable) {
        requiredExtensionNames.push_back("XR_EXT_debug_utils");
    }
    if(mPermissionsSupportAvailable) {
        requiredExtensionNames.push_back("XR_EXT_permissions_support");
    }

    XrInstanceCreateInfo createInstance{XR_TYPE_INSTANCE_CREATE_INFO};
    createInstance.next = nullptr;
    createInstance.createFlags = 0;
    strncpy_s(createInstance.applicationInfo.applicationName, appName.c_str(), appName.size() + 1);
    createInstance.applicationInfo.applicationVersion = appVersion;
    strncpy_s(createInstance.applicationInfo.engineName, engineName.c_str(), engineName.size() + 1);
    createInstance.applicationInfo.engineVersion = engineVersion;
    createInstance.applicationInfo.apiVersion = XR_MAKE_VERSION(1 ,0, 0);
    createInstance.enabledExtensionNames = requiredExtensionNames.data();
    createInstance.enabledExtensionCount = (uint32_t) requiredExtensionNames.size();
    createInstance.enabledApiLayerCount = 0;
    createInstance.enabledApiLayerNames = nullptr;
    CHECK_XR(xrCreateInstance(&createInstance, &mInstance));

    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES, nullptr};
    CHECK_XR(xrGetInstanceProperties(mInstance, &instanceProperties));
    std::cout << "Runtime \"" << instanceProperties.runtimeName << "\", version " <<
	XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "." <<
	XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "p" <<
	XR_VERSION_PATCH(instanceProperties.runtimeVersion) << "\n";
}

void OpenXRProgram::GetSystem()
{
    XrSystemGetInfo getSystem{XR_TYPE_SYSTEM_GET_INFO, nullptr, XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
    CHECK_XR(xrGetSystem(mInstance, &getSystem, &mSystemId));

    XrSystemProperties systemProperties { XR_TYPE_SYSTEM_PROPERTIES };
    CHECK_XR(xrGetSystemProperties(mInstance, mSystemId, &systemProperties));
    std::cout << "System \"" << systemProperties.systemName << "\", vendorId " << systemProperties.vendorId << "\n";

    mMaxLayerCount = systemProperties.graphicsProperties.maxLayerCount;
}

void OpenXRProgram::CreateSession(ID3D11Device* d3d11Device, bool requestOverlaySession, bool requestPosePermission)
{

    void* chain = nullptr;

    XrGraphicsBindingD3D11KHR d3dBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    d3dBinding.device = d3d11Device;
    chain = &d3dBinding;

    if(requestOverlaySession) {
#if COMPILE_REMOTE_OVERLAY_APP

        XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
        sessionCreateInfoOverlay.next = chain;
        sessionCreateInfoOverlay.createFlags = 0;
        sessionCreateInfoOverlay.sessionLayersPlacement = 1; 
        chain = &sessionCreateInfoOverlay;

#else // !COMPILER_REMOTE_OVERLAY_APP

        std::cerr << "An overlay session was requested but the overlay extension is not available.\n";
        exit(EXIT_FAILURE);

#endif  // COMPILE_REMOTE_OVERLAY_APP
    }  

    if(requestPosePermission) {
#if COMPILE_REMOTE_OVERLAY_APP
        bool unfocusedPosePermissionAvailable = false;
        XrPermissionIdEXT unfocusedPosePermissionId = 0;
        PFN_xrEnumerateInstancePermissionsEXT enumerateInstancePermissionsEXT;
        CHECK_XR(xrGetInstanceProcAddr(mInstance, "xrEnumerateInstancePermissionsEXT",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&enumerateInstancePermissionsEXT)));
        uint32_t count;
        CHECK_XR(enumerateInstancePermissionsEXT(mInstance, 0, &count, nullptr));
        std::vector<XrPermissionPropertiesEXT> properties(count);
        CHECK_XR(enumerateInstancePermissionsEXT(mInstance, count, &count, properties.data()));
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
    sessionCreateInfo.systemId = mSystemId;
    sessionCreateInfo.next = chain;

    CHECK_XR(xrCreateSession(mInstance, &sessionCreateInfo, &mSession));
}

bool OpenXRProgram::ChooseBestPixelFormat(const std::vector<DXGI_FORMAT>& appFormats, uint64_t *chosenFormat)
{
    uint32_t count;
    CHECK_XR(xrEnumerateSwapchainFormats(mSession, 0, &count, nullptr));

    std::vector<int64_t> runtimeFormats(count);
    CHECK_XR(xrEnumerateSwapchainFormats(mSession, (uint32_t)count, &count, runtimeFormats.data()));

    auto formatFound = std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), appFormats.begin(), appFormats.end());

    if(formatFound == runtimeFormats.end()) {
        return false;
    }
    *chosenFormat = *formatFound;
    return true;
}

LUID OpenXRProgram::GetD3D11AdapterLUID()
{
    PFN_xrGetD3D11GraphicsRequirementsKHR getD3D11GraphicsRequirementsKHR;
    CHECK_XR(xrGetInstanceProcAddr(mInstance, "xrGetD3D11GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&getD3D11GraphicsRequirementsKHR)));

    XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    CHECK_XR(getD3D11GraphicsRequirementsKHR(mInstance, mSystemId, &graphicsRequirements));

    return graphicsRequirements.adapterLuid;
}

void OpenXRProgram::FindRecommendedDimensions()
{
    uint32_t count;
    CHECK_XR(xrEnumerateViewConfigurations(mInstance, mSystemId, 0, &count, nullptr));
    std::vector<XrViewConfigurationType> viewConfigurations(count);
    CHECK_XR(xrEnumerateViewConfigurations(mInstance, mSystemId, count, &count, viewConfigurations.data()));

    bool found = false;
    for(uint32_t i = 0; i < count; i++) {
        if(viewConfigurations[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
            found = true;
        }
    }
    if(!found) {
        std::cerr << "Failed to find XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO in " << count << " view configurations\n";
        DebugBreak();
    }

    XrViewConfigurationProperties configurationProperties {XR_TYPE_VIEW_CONFIGURATION_PROPERTIES, nullptr};
    CHECK_XR(xrGetViewConfigurationProperties(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &configurationProperties));

    CHECK_XR(xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr));
    std::vector<XrViewConfigurationView> viewConfigurationViews(count);
    for(uint32_t i = 0; i < count; i++) {
        viewConfigurationViews[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        viewConfigurationViews[i].next = nullptr;
    }
    CHECK_XR(xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count, viewConfigurationViews.data()));

    // XXX Be lazy and set recommended sizes to left eye
    mRecommendedDimensions[0] = viewConfigurationViews[0].recommendedImageRectWidth;
    mRecommendedDimensions[1] = viewConfigurationViews[0].recommendedImageRectHeight;
}

void OpenXRProgram::CreateSwapChainsAndGetImages(DXGI_FORMAT format)
{
    FindRecommendedDimensions();
    std::cout << "Recommended view image dimensions are " << mRecommendedDimensions[0] << " by " << mRecommendedDimensions[1] << "\n";

    for(int eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = format;
        swapchainCreateInfo.width = mRecommendedDimensions[0];
        swapchainCreateInfo.height = mRecommendedDimensions[1];
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = 1;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.next = nullptr;
        CHECK_XR(xrCreateSwapchain(mSession, &swapchainCreateInfo, &mSwapchains[eye]));

        uint32_t count;
        CHECK_XR(xrEnumerateSwapchainImages(mSwapchains[eye], 0, &count, nullptr));
        mSwapchainImages[eye] = new XrSwapchainImageD3D11KHR[count];
        for(uint32_t i = 0; i < count; i++) {
            mSwapchainImages[eye][i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            mSwapchainImages[eye][i].next = nullptr;
        }
        CHECK_XR(xrEnumerateSwapchainImages(mSwapchains[eye], count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(mSwapchainImages[eye])));
    }
}

void OpenXRProgram::CreateContentSpace()
{
    XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};

    createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    createSpaceInfo.poseInReferenceSpace = XrPosef{{0.0, 0.0, 0.0, 1.0}, {0.0, 0.0, 0.0}};

    CHECK_XR(xrCreateReferenceSpace(mSession, &createSpaceInfo, &mContentSpace));
}

bool OpenXRProgram::IsRunning()
{
    return (mSessionState == XR_SESSION_STATE_SYNCHRONIZED) || 
       (mSessionState == XR_SESSION_STATE_VISIBLE) || 
       (mSessionState == XR_SESSION_STATE_STOPPING) || 
       (mSessionState == XR_SESSION_STATE_FOCUSED);
}

void OpenXRProgram::RequestExitSession()
{
    CHECK_XR(xrRequestExitSession(mSession));
}

void OpenXRProgram::ProcessSessionStateChangedEvent(XrEventDataBuffer* event, bool& quit, bool& doFrame)
{
    const auto& e = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
    if(e.session == mSession) {

        // Handle state change of our session
        if((e.state == XR_SESSION_STATE_EXITING) ||
           (e.state == XR_SESSION_STATE_LOSS_PENDING)) {

            quit = true;
            doFrame = false;

        } else {

            switch(mSessionState) {
                case XR_SESSION_STATE_IDLE: {
                    if(e.state == XR_SESSION_STATE_READY) {
                        XrSessionBeginInfo beginInfo {XR_TYPE_SESSION_BEGIN_INFO, nullptr, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
                        CHECK_XR(xrBeginSession(mSession, &beginInfo));
                        doFrame = true;
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
                        CHECK_XR(xrEndSession(mSession));
                        doFrame = false;
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
        mSessionState = e.state;
    }
}

void OpenXRProgram::ProcessEvent(XrEventDataBuffer *event, bool& quit, bool& doFrame)
{
    switch(event->type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            // const auto& e = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
            quit = true;
            break;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            ProcessSessionStateChangedEvent(event, quit, doFrame);
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

void OpenXRProgram::ProcessEvents(bool& quit, bool &doFrame)
{
    bool getAnotherEvent = true;
    while (getAnotherEvent && !quit) {
        static XrEventDataBuffer event {XR_TYPE_EVENT_DATA_BUFFER, nullptr};
        XrResult result = xrPollEvent(mInstance, &event);
        if(result == XR_SUCCESS) {
            ProcessEvent(&event, quit, doFrame);
        } else {
            getAnotherEvent = false;
        }
    }
}

bool OpenXRProgram::WaitFrame()
{
    mWaitFrameState = { XR_TYPE_FRAME_STATE, nullptr };
    CHECK_XR(xrWaitFrame(mSession, nullptr, &mWaitFrameState));
	return mWaitFrameState.shouldRender;
}

void OpenXRProgram::BeginFrame()
{
    CHECK_XR(xrBeginFrame(mSession, nullptr));
}

void OpenXRProgram::AddLayer(XrCompositionLayerFlags flags, XrEyeVisibility visibility, const XrSwapchainSubImage& subImage, XrPosef pose, const XrExtent2Df& extent)
{
    XrCompositionLayerQuad layer {XR_TYPE_COMPOSITION_LAYER_QUAD, nullptr};

    layer.layerFlags = flags;
    layer.space = mContentSpace;
    layer.eyeVisibility = visibility;
    layer.subImage = subImage;
    layer.pose = pose;
    layer.size = extent;

    mLayers.push_back(layer);
}

void OpenXRProgram::EndFrame()
{
    std::vector<XrCompositionLayerBaseHeader*> layerPointers;
    for(auto& layer: mLayers) {
        layerPointers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
    }
    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO, nullptr, mWaitFrameState.predictedDisplayTime, XR_ENVIRONMENT_BLEND_MODE_OPAQUE, (uint32_t)layerPointers.size(), layerPointers.data()};
    CHECK_XR(xrEndFrame(mSession, &frameEndInfo));
    mLayers.clear();
}


//----------------------------------------------------------------------------
// Debug utils functions

XrBool32 processDebugMessage(
    XrDebugUtilsMessageSeverityFlagsEXT /* messageSeverity */,
    XrDebugUtilsMessageTypeFlagsEXT /* messageTypes */,
    const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /* userData */)
{
    std::cout << "XR: " << callbackData->message << "\n";

    // XXX Other fields we could honor:
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

uint32_t OpenXRProgram::AcquireAndWaitSwapchainImage(int eye)
{
    uint32_t index;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, nullptr};
    CHECK_XR(xrAcquireSwapchainImage(GetSwapchain(eye), &acquireInfo, &index));

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.next = nullptr;
    waitInfo.timeout = ONE_SECOND_IN_NANOSECONDS;
    CHECK_XR(xrWaitSwapchainImage(GetSwapchain(eye), &waitInfo));
    return index;
}

void OpenXRProgram::ReleaseSwapchainImage(int eye)
{
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr};
    CHECK_XR(xrReleaseSwapchainImage(GetSwapchain(eye), &releaseInfo));
}


//----------------------------------------------------------------------------
// Main

int main( void )
{
    // Set console unbuffered so we don't miss any output
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    bool sawFirstSuccessfulFrame = false;

    // Which images to display
    std::vector<std::string> imageFilenames;
    imageFilenames.push_back("avatar1.png");
    imageFilenames.push_back("avatar2.png");

    FreeImage_Initialise();

    std::vector<fipImage> images;
    LoadImages(imageFilenames, images);

    std::vector<float> imageAspectRatios;
    for(auto& image: images) {
        imageAspectRatios.push_back(image.getWidth() / (float)image.getHeight());
    }

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

    OpenXRProgram program(createOverlaySession);

    program.CreateInstance("Overlay Sample", 0, "none", 0);

    XrDebugUtilsMessengerEXT messenger = XR_NULL_HANDLE;
    if(program.IsDebugUtilsAvailable()) {
        CreateDebugMessenger(program.GetInstance(), &messenger);
    }

    program.GetSystem();

    bool useSeparateLeftRightEyes;
    if(program.GetMaxLayerCount() >= 2) {
        useSeparateLeftRightEyes = true;
    } else if(program.GetMaxLayerCount() >= 1) {
        useSeparateLeftRightEyes = false;
    } else {
        std::cerr << "xrGetSystemProperties reports maxLayerCount 0, no way to display a compositor layer\n";
        DebugBreak();
    }

    ID3D11Device* d3d11Device = GetD3D11DeviceFromAdapter(program.GetD3D11AdapterLUID());

    bool requestPermission = program.IsPermissionsSupportAvailable();
    program.CreateSession(d3d11Device, createOverlaySession, requestPermission);

    uint64_t chosenFormat;
    if(!program.ChooseBestPixelFormat({ DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB }, &chosenFormat)) {
        outputDebugF("No supported swapchain format found\n");
        DebugBreak();
    }

    program.CreateSwapChainsAndGetImages((DXGI_FORMAT)chosenFormat);

    ID3D11DeviceContext* d3dContext;
    d3d11Device->GetImmediateContext(&d3dContext);

    std::vector<ID3D11Texture2D*> sourceTextures;
    int32_t recommendedWidth = program.GetRecommendedDimensions()[0];
    int32_t recommendedHeight = program.GetRecommendedDimensions()[1];
    CreateSourceTextures(d3d11Device, d3dContext, recommendedWidth, recommendedHeight, images, sourceTextures, (DXGI_FORMAT)chosenFormat);

    // Spawn a thread to wait for a keypress
    static bool requestExit = false;
    static bool exitRequested = false;
    auto exitPollingThread = std::thread{[] {
        std::cout << "Press ENTER to exit...\n";
        getchar();
        requestExit = true;
    }};
    exitPollingThread.detach();

    program.CreateContentSpace();

    // OpenXR Frame loop

    int whichImage = 0;
    auto then = std::chrono::steady_clock::now();
    bool doFrame = false;

    bool quit = false;

    do {
        auto now = std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count() > 2000) {
            whichImage = (whichImage + 1) % sourceTextures.size();
            then = std::chrono::steady_clock::now();
        }

        if(requestExit) {
            if(!exitRequested) {
                if(program.IsRunning()) {
                    // Play nice
                    program.RequestExitSession();
                } else {
                    // Just quit
                    quit = true;
                }
                exitRequested = true;
            }
        }
        program.ProcessEvents(quit, doFrame);

        if(!quit) {
            if(doFrame) {

                bool shouldRender = program.WaitFrame();

                program.BeginFrame();

                if(shouldRender) {
                    for(int eye = 0; eye < 2; eye++) {
                        uint32_t index;
                        index = program.AcquireAndWaitSwapchainImage(eye);

                        d3dContext->CopyResource(program.GetSwapchainImage(eye, index).texture, sourceTextures[whichImage]);
                        program.ReleaseSwapchainImage(eye);

                    }
                    d3dContext->Flush();

                    XrPosef pose = XrPosef{{0.0, 0.0, 0.0, 1.0}, {0.0f, 0.0f, -2.0f}};
                    XrCompositionLayerFlags flags = XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT |
                        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    XrExtent2Df extent {1.0f * imageAspectRatios[whichImage], 1.0f};

                    if(useSeparateLeftRightEyes) {

                        XrSwapchainSubImage leftImage {program.GetSwapchain(0), {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                        program.AddLayer(flags, XR_EYE_VISIBILITY_LEFT, leftImage, pose, extent);

                        XrSwapchainSubImage rightImage {program.GetSwapchain(1), {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                        program.AddLayer(flags, XR_EYE_VISIBILITY_RIGHT, rightImage, pose, extent);

                    } else {

                        XrSwapchainSubImage fullImage {program.GetSwapchain(0), {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                        program.AddLayer(flags, XR_EYE_VISIBILITY_BOTH, fullImage, pose, extent);
                    }
                }

                program.EndFrame();

                if(!sawFirstSuccessfulFrame) {
                    sawFirstSuccessfulFrame = true;
                    std::cout << "First Overlay xrEndFrame was successful!  Continuing...\n";
                }

            } else {

                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
    } while(!quit);

    return 0;
}
