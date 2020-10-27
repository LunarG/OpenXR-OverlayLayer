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
// Eigen Headers and typeDefs
//
#include <Eigen/Dense>
#include <Eigen/Geometry>
typedef Eigen::Vector3f Vector3f;
typedef Eigen::Affine3f Affine3f;
typedef Eigen::AngleAxisf AngleAxisf;
typedef Eigen::Quaternionf Quaternionf;
typedef Eigen::ParametrizedLine<float, 3> Ray3f;
typedef Eigen::Hyperplane<float, 3> Plane3f;

//
// OpenXR Headers
//
#undef XR_USE_GRAPHICS_API_D3D12
#undef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_D3D11 1

#include <openxr/openxr.h>
XR_DEFINE_ATOM(XrPermissionIdEXT)

#include <openxr/openxr_platform.h>

#include <FreeImagePlus.h>

#include "../include/util.h"


constexpr uint64_t ONE_SECOND_IN_NANOSECONDS = 1000000000;

constexpr int gFudgeMinAlpha = 128;

int gLayerPlacement = 0;
float gLayerRotationalOffset = 0.f;

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
                    if(srcPixel[3] < gFudgeMinAlpha) {
                        // Alpha fix while waiting on diagnosis from FB
                        dstPixel[0] = 0;
                        dstPixel[1] = 0;
                        dstPixel[2] = 0;
                    } else {
                        dstPixel[0] = srcPixel[2];
                        dstPixel[1] = srcPixel[1];
                        dstPixel[2] = srcPixel[0];
                    }
                    dstPixel[3] = srcPixel[3];
                }
            } else if (format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                for (int32_t x = 0; x < width; x++) {
                    unsigned char *srcPixel = src + x * 4;
                    unsigned char *dstPixel = dst + x * 4;
                    if(srcPixel[3] < gFudgeMinAlpha) {
                        // Alpha fix while waiting on diagnosis from FB
                        dstPixel[0] = 0;
                        dstPixel[1] = 0;
                        dstPixel[2] = 0;
                    } else {
                        dstPixel[0] = srcPixel[0];
                        dstPixel[1] = srcPixel[1];
                        dstPixel[2] = srcPixel[2];
                    }
                    dstPixel[3] = srcPixel[3];
                }
            }
        }
        d3dContext->Unmap(sourceTextures[i], 0);
    }
}

//----------------------------------------------------------------------------
// struct for holding specific actions and poses

struct action_state_t {
    XrActionSet actionSet;
    XrAction poseAction;
    XrAction selectAction;
    XrAction vibrateAction;
    XrPath handSubactionPath[2];
    XrSpace handSpace[2];
    XrPosef handPose[2];
    XrBool32 handActive[2];
    XrBool32 handSelect[2];
};

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
        if(true) {
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
    void CreateActions(action_state_t& actionState);
    LUID GetD3D11AdapterLUID();
    bool ChooseBestPixelFormat(const std::vector<DXGI_FORMAT>& appFormats, uint64_t *chosenFormat);
    void FindRecommendedDimensions();
    void CreateSwapChainsAndGetImages(DXGI_FORMAT format);
    void CreateContentSpace();
    void RequestExitSession();
    void ProcessEvent(XrEventDataBuffer *event, bool& quit, bool& doFrame, action_state_t& actionState);
    void ProcessSessionStateChangedEvent(XrEventDataBuffer* event, bool& quit, bool& doFrame);
    void ProcessEvents(bool& quit, bool &doFrame, action_state_t& actionState);
    void GetActionState(action_state_t& actionState);
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

    XrFrameState GetWaitFrameState() { return mWaitFrameState; }

private:
    bool        mRequestOverlaySession = false;
    bool        mDebugUtilsAvailable = false;
    bool        mPermissionsSupportAvailable = false;
    XrInstance  mInstance;
    XrSystemId  mSystemId;
    XrSession   mSession;
    XrSwapchain mSwapchains[2];
    XrSwapchainImageD3D11KHR    *mSwapchainImages[2];
    std::array<int32_t, 2>      mRecommendedDimensions;
    XrSpace     mContentSpace;
    uint32_t    mMaxLayerCount;
    XrSessionState      mSessionState = XR_SESSION_STATE_IDLE;
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
            if(std::string(p.first) == "XR_EXT_debug_utils") {
                mDebugUtilsAvailable = true;
            }
            if(std::string(p.first) == "XR_EXT_permissions_support") {
                mPermissionsSupportAvailable = true;
            }
        }
    } else {
        std::cout << "Warning: No extensions supported.\n";
    }

    std::vector<const char*> requiredExtensionNames;
    requiredExtensionNames.push_back("XR_KHR_D3D11_enable");

    if (mRequestOverlaySession) {
        requiredExtensionNames.push_back(XR_EXTX_OVERLAY_EXTENSION_NAME);
    }

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
        XrSessionCreateInfoOverlayEXTX sessionCreateInfoOverlay{ XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX };
        sessionCreateInfoOverlay.next = chain;
        sessionCreateInfoOverlay.createFlags = 0;
        sessionCreateInfoOverlay.sessionLayersPlacement = gLayerPlacement;
        chain = &sessionCreateInfoOverlay;
    }

    if(requestPosePermission) {
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

void OpenXRProgram::CreateActions(action_state_t& actionState)
{
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionSetInfo.actionSetName, "test_interactions");
    strcpy_s(actionSetInfo.localizedActionSetName, "Test Interactions");
    CHECK_XR(xrCreateActionSet(mInstance, &actionSetInfo, &actionState.actionSet));
    CHECK_XR(xrStringToPath(mInstance, "/user/hand/left", &actionState.handSubactionPath[0]));
    CHECK_XR(xrStringToPath(mInstance, "/user/hand/right", &actionState.handSubactionPath[1]));

    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.countSubactionPaths = _countof(actionState.handSubactionPath);
    actionInfo.subactionPaths = actionState.handSubactionPath;
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(actionInfo.actionName, "hand_aim");
    strcpy_s(actionInfo.localizedActionName, "Hand Pose");
    CHECK_XR(xrCreateAction(actionState.actionSet, &actionInfo, &actionState.poseAction));

    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy_s(actionInfo.actionName, "select");
    strcpy_s(actionInfo.localizedActionName, "Select");
    CHECK_XR(xrCreateAction(actionState.actionSet, &actionInfo, &actionState.selectAction));

    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    strcpy_s(actionInfo.actionName, "vibrate");
    strcpy_s(actionInfo.localizedActionName, "Vibrate");
    CHECK_XR(xrCreateAction(actionState.actionSet, &actionInfo, &actionState.vibrateAction));

    XrPath profilePath;
    XrPath posePath[2];
    XrPath selectPath[2];
    XrPath vibrationPath[2];
    if(true) {
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/input/aim/pose", &posePath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/input/aim/pose", &posePath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/input/select/click", &selectPath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/input/select/click", &selectPath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/output/haptic", &vibrationPath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/output/haptic", &vibrationPath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/interaction_profiles/khr/simple_controller", &profilePath));

        XrActionSuggestedBinding khr_bindings[] = {
            {actionState.poseAction, posePath[0]},
            {actionState.poseAction, posePath[1]},
            {actionState.selectAction, selectPath[0]},
            {actionState.selectAction, selectPath[1]},
            {actionState.vibrateAction, vibrationPath[0]},
            {actionState.vibrateAction, vibrationPath[1]},
        };

        XrInteractionProfileSuggestedBinding suggestedBinding_khr{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBinding_khr.interactionProfile = profilePath;
        suggestedBinding_khr.suggestedBindings = &khr_bindings[0];
        suggestedBinding_khr.countSuggestedBindings = _countof(khr_bindings);
        CHECK_XR(xrSuggestInteractionProfileBindings(mInstance, &suggestedBinding_khr));
    }

    if(true) {
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/input/aim/pose", &posePath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/input/aim/pose", &posePath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/input/x/click", &selectPath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/input/a/click", &selectPath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/output/haptic", &vibrationPath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/output/haptic", &vibrationPath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/interaction_profiles/oculus/touch_controller", &profilePath));

        XrActionSuggestedBinding ovr_bindings[] = {
            {actionState.poseAction, posePath[0]},
            {actionState.poseAction, posePath[1]},
            {actionState.selectAction, selectPath[0]},
            {actionState.selectAction, selectPath[1]},
            {actionState.vibrateAction, vibrationPath[0]},
            {actionState.vibrateAction, vibrationPath[1]},
        };

        XrInteractionProfileSuggestedBinding suggestedBinding_ovr{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBinding_ovr.interactionProfile = profilePath;
        suggestedBinding_ovr.suggestedBindings = &ovr_bindings[0];
        suggestedBinding_ovr.countSuggestedBindings = _countof(ovr_bindings);
        CHECK_XR(xrSuggestInteractionProfileBindings(mInstance, &suggestedBinding_ovr));
    }

    if (true) {
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/input/aim/pose", &posePath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/input/aim/pose", &posePath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/input/thumbstick/click", &selectPath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/input/trackpad/click", &selectPath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/left/output/haptic", &vibrationPath[0]));
        CHECK_XR(xrStringToPath(mInstance, "/user/hand/right/output/haptic", &vibrationPath[1]));
        CHECK_XR(xrStringToPath(mInstance, "/interaction_profiles/microsoft/motion_controller", &profilePath));

        XrActionSuggestedBinding wmr_bindings[] = {
            {actionState.poseAction, posePath[0]},
            {actionState.poseAction, posePath[1]},
            {actionState.selectAction, selectPath[0]},
            {actionState.selectAction, selectPath[1]},
            {actionState.vibrateAction, vibrationPath[0]},
            {actionState.vibrateAction, vibrationPath[1]},
        };

        XrInteractionProfileSuggestedBinding suggestedBinding_wmr{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBinding_wmr.interactionProfile = profilePath;
        suggestedBinding_wmr.suggestedBindings = &wmr_bindings[0];
        suggestedBinding_wmr.countSuggestedBindings = _countof(wmr_bindings);
        CHECK_XR(xrSuggestInteractionProfileBindings(mInstance, &suggestedBinding_wmr));
    }

    for (int32_t i = 0; i < 2; i++) {
        XrActionSpaceCreateInfo actionSpaceInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
        actionSpaceInfo.action = actionState.poseAction;
        actionSpaceInfo.poseInActionSpace = {{0,0,0,1}, {0,0,0}};
        actionSpaceInfo.subactionPath = actionState.handSubactionPath[i];
        CHECK_XR(xrCreateActionSpace(mSession, &actionSpaceInfo, &actionState.handSpace[i]));
    }

    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionState.actionSet;
    CHECK_XR(xrAttachSessionActionSets(mSession, &attachInfo));
}

void OpenXRProgram::GetActionState(action_state_t& actionState)
{
    if (mSessionState != XR_SESSION_STATE_FOCUSED) {
        return;
    }

    XrActiveActionSet actionSet{};
    actionSet.actionSet = actionState.actionSet;
    actionSet.subactionPath = XR_NULL_PATH;

    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &actionSet;
    XrResult result = xrSyncActions(mSession, &syncInfo);

    if(result == XR_SESSION_NOT_FOCUSED) {
        return;
    }

    for (uint32_t hand = 0; hand < 2; hand++) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.subactionPath = actionState.handSubactionPath[hand];

        XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
        getInfo.action = actionState.poseAction;
        CHECK_XR(xrGetActionStatePose(mSession, &getInfo, &poseState));
        XrSpaceLocation spaceLocation = { XR_TYPE_SPACE_LOCATION };
        XrResult res = xrLocateSpace(actionState.handSpace[hand], mContentSpace, mWaitFrameState.predictedDisplayTime, &spaceLocation);

        if (XR_UNQUALIFIED_SUCCESS(res) &&
            (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
            actionState.handPose[hand] = spaceLocation.pose;
        }
        actionState.handActive[hand] = poseState.isActive;

        XrActionStateBoolean selectState{XR_TYPE_ACTION_STATE_BOOLEAN};
        getInfo.action = actionState.selectAction;
        CHECK_XR(xrGetActionStateBoolean(mSession, &getInfo, &selectState));

        actionState.handSelect[hand] = selectState.currentState && selectState.changedSinceLastSync;
        static XrBool32 oldstate = false;
        if(false) {
            if((hand == 1) && (oldstate != selectState.currentState)) {
                printf("%s, %schanged since last sync\n", selectState.currentState ? "pressed" : "released", selectState.changedSinceLastSync ? "" : "not ");
                oldstate = selectState.currentState;
            }
        }
    }
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

void OpenXRProgram::ProcessEvent(XrEventDataBuffer *event, bool& quit, bool& doFrame, action_state_t& actionState)
{
    switch(event->type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            // const auto& e = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
            quit = true;
            break;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            ProcessSessionStateChangedEvent(event, quit, doFrame);
            break;
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
            const auto& e = *reinterpret_cast<const XrEventDataInteractionProfileChanged*>(event);
            if(e.session == mSession) {
                std::cout << "Interaction profile Changed for our session.\n";

                if (true) {
                    std::vector<std::pair<std::string, XrAction>> actions = {
                        {"select", actionState.selectAction,},
                        {"pose", actionState.poseAction,},
                    };

                    for (auto a : actions) {
                        XrBoundSourcesForActionEnumerateInfo enumerateInfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO, nullptr,
                            a.second};
                        uint32_t capacity;
                        CHECK_XR(xrEnumerateBoundSourcesForAction(mSession, &enumerateInfo, 0, &capacity, nullptr));
                        std::vector<XrPath> sources(capacity);
                        CHECK_XR(xrEnumerateBoundSourcesForAction(mSession, &enumerateInfo, capacity, &capacity, sources.data()));
                        static char str1[512], str2[512];
                        uint32_t str1cap = 512, str2cap;
                        printf("bound \"%s\" to %d sources... \n", a.first.c_str(), capacity);
                        for (unsigned int i = 0; i < capacity; i++) {
                            XrResult result = xrPathToString(mInstance, sources[i], 0, &str1cap, nullptr);
                            if (result != XR_SUCCESS) {
                                sprintf(str1, "(null path)");
                            } else {
                                xrPathToString(mInstance, sources[i], str1cap, &str1cap, str1);
                            }
                            XrInputSourceLocalizedNameGetInfo getInfo = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO, nullptr,
                                sources[i],
                                XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                    XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                    XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT};
                            CHECK_XR(xrGetInputSourceLocalizedName(mSession, &getInfo, 0, &str2cap, nullptr));
                            CHECK_XR(xrGetInputSourceLocalizedName(mSession, &getInfo, str2cap, &str2cap, str2));
                            printf("    \"%s\"(\"%s\")\n", str1, str2);
                        }
                    }

                    fflush(stdout);
        }
            }
            break;
        }
        default: {
            std::cout << "Warning: ignoring event type " << event->type << "\n";
            break;
        }
    }
}

void OpenXRProgram::ProcessEvents(bool& quit, bool &doFrame, action_state_t& actionState)
{
    bool getAnotherEvent = true;
    while (getAnotherEvent && !quit) {
        XrEventDataBuffer event {XR_TYPE_EVENT_DATA_BUFFER, nullptr};
        XrResult result = xrPollEvent(mInstance, &event);
        if(result == XR_SUCCESS) {
            ProcessEvent(&event, quit, doFrame, actionState);
        } else {
            if (result != XR_EVENT_UNAVAILABLE) {
                CHECK_XR(result);
        } else {
            getAnotherEvent = false;
        }
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

bool Intersects(const XrPosef& source, const XrPosef& planePose, const XrExtent2Df& planeExtents)
{
    auto handOrientation = Quaternionf(
        source.orientation.w, 
        source.orientation.x, 
        source.orientation.y, 
        source.orientation.z
    );

    auto handForward = handOrientation * Vector3f(0, 0, -1);
    auto handRay = Ray3f(
        Vector3f(source.position.x, source.position.y, source.position.z),
        handForward
    );

    auto planeOrientation = Quaternionf(
        planePose.orientation.w, 
        planePose.orientation.x, 
        planePose.orientation.y, 
        planePose.orientation.z
    );

    auto planeNormal = planeOrientation * Vector3f(0, 0, -1);
    auto planeOrigin = Vector3f(planePose.position.x, planePose.position.y, planePose.position.z);
    auto plane = Plane3f(
        planeNormal,
        planeOrigin
    );

    // return false for lines parallel to the plane
    if (std::abs(handRay.direction().normalized().dot(plane.normal().normalized())) <= 0.0001f)
        return false;

    Vector3f intersection = handRay.intersectionPoint(plane);

    // if the distance to center is less than the minimal extent count this as a hit
    float distance = (intersection - planeOrigin).stableNorm();
    return (planeExtents.width / 2.f) >= distance && (planeExtents.height / 2.f) >= distance;
}

XrPosef ApplyContentSpaceRotationalOffsetToPose(const XrPosef& Pose, const Vector3f& Axis, float Radians) {
    Vector3f Position(Pose.position.x, Pose.position.y, Pose.position.z);
    Quaternionf Orientation(Pose.orientation.w, Pose.orientation.x, Pose.orientation.y, Pose.orientation.z);
    AngleAxisf Offset(Radians, Axis);
    Vector3f OffsetPosition = Offset * Position;
    Quaternionf OffsetOrientation = Quaternionf(Offset) * Orientation;
    return XrPosef{
        {OffsetOrientation.x(), OffsetOrientation.y(), OffsetOrientation.z(), OffsetOrientation.w()},
        {OffsetPosition.x(), OffsetPosition.y(), OffsetPosition.z()}
    };
}

//----------------------------------------------------------------------------
// Main
//
void usage(const char *programName)
{
    std::cerr << "usage: overlay-sample [options]\n";
    std::cerr << "options:\n";
    std::cerr << "    --placement N          Set overlay layer level to N    [default 0]\n";
    std::cerr << "    --rotational_offset N  Angle in radians to offset the layer clockwise about\n";
    std::cerr << "                           the stage space world up vector [default 0]\n";
}

int main( int argc, char **argv )
{
    for (int arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "--placement") == 0) {
            if (arg + 1 >= argc) {
                std::cerr << "expected level for --placement option\n";
                usage(argv[0]);
                exit(1);
            }
            gLayerPlacement = atoi(argv[arg + 1]);
            arg += 2;
        } else if (strcmp(argv[arg], "--rotational_offset") == 0) {
            if (arg + 1 >= argc) {
                std::cerr << "expected radians for --rotational_offset option\n";
                usage(argv[0]);
                exit(1);
            }
            gLayerRotationalOffset = atof(argv[arg + 1]);
            arg += 2;
        }
        else {
            std::cerr << "unknown option\n";
            usage(argv[0]);
            exit(1);
        }
    }
    // Set console unbuffered so we don't miss any output
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    bool sawFirstSuccessfulFrame = false;
    action_state_t actionState = {};    // Holds actionSet state for left and right hands

    //----------------------------------------------------------------------
    // Image flipbook stuff

    // Which images to display
    std::vector<std::string> imageFilenames;
    imageFilenames.push_back("avatar1.png");
    imageFilenames.push_back("avatar2.png");
    imageFilenames.push_back("highlighted1.png");
    imageFilenames.push_back("selected1.png");

    FreeImage_Initialise();

    std::vector<fipImage> images;
    LoadImages(imageFilenames, images);

    std::vector<float> imageAspectRatios;
    for(auto& image: images) {
        imageAspectRatios.push_back(image.getWidth() / (float)image.getHeight());
    }

    //----------------------------------------------------------------------
    // Create OpenXR Instance and Session

    bool createOverlaySession = true;
    OpenXRProgram program(createOverlaySession);

    program.CreateInstance("Overlay Sample", 0, "none", 0);

    XrDebugUtilsMessengerEXT messenger = XR_NULL_HANDLE;
    if(program.IsDebugUtilsAvailable()) {
        CreateDebugMessenger(program.GetInstance(), &messenger);
    }

    program.GetSystem();

    bool useSeparateLeftRightEyes = false;
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

    //----------------------------------------------------------------------
    // Create Images to copy into SwapchainImages

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

    program.CreateContentSpace();
    program.CreateActions(actionState);



    //----------------------------------------------------------------------
    // Spawn a thread to wait for a keypress to exit

    static bool requestExit = false;            // User hit ENTER to exit
    auto exitPollingThread = std::thread{[] {
        std::cout << "Press ENTER to exit...\n";
        getchar();
        requestExit = true;
    }};
    exitPollingThread.detach();

    //----------------------------------------------------------------------
    // OpenXR Frame loop

    int whichImage = 0;                 // Which Image should we display
    auto then = std::chrono::steady_clock::now();

    bool doFrame = false;               // Only do Frame commands if we are in OpenXR "running" mode

    bool hoverActive = false;           // Whether a controller is intersecting the frame or not
    bool toggledImage = false;          // Whether a click has been toggled
    int hoverImageIdx = 2;              // the idx of the hovered image
    int toggledImageIdx = 3;             // the idx of the click toggled image
    int specialImageCount = 2;          // how many "special images" are appended to the carosel and aspect ratio array

    bool exitRequested = false;         // We requested that OpenXR exit
    bool quit = false;                  // We set this to true when we may exit out of our frame loop

    XrPosef layerImagePose = ApplyContentSpaceRotationalOffsetToPose(
        {{0.0, 0.0, 0.0, 1.0}, {0.0f, 0.0f, -2.0f}}, // Pose
        {0.0, 1.0, 0.0}, // Up
        gLayerRotationalOffset
    );
    
    XrExtent2Df layerImageExtent {1.0f * imageAspectRatios[hoverImageIdx], 1.0f};

    bool hadVibrated = false;

    do {
        //----------------------------------------------------------------------
        // OpenXR Event Management

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
        program.ProcessEvents(quit, doFrame, actionState);

        //----------------------------------------------------------------------
        // OpenXR frame loop

        if(!quit) {
            if(doFrame) {

                // The OpenXR runtime can give us guidance on whether
                // we should do or avoid doing heavy GPU work.  That's
                // represented by the shouldRender variable in XrFrameState,
                // which our utility class returns here.
                bool shouldRender = program.WaitFrame();

                //----------------------------------------------------------------------
                // Input handling

                program.GetActionState(actionState);

                //----------------------------------------------------------------------
                // Intersection test

                hoverActive = false;
                for (uint32_t hand = 0; hand < 2; hand++) {
                    if (actionState.handActive[hand]) {
                        if (Intersects(actionState.handPose[hand], layerImagePose, layerImageExtent)) {
                            hoverActive = true;
                            if (actionState.handSelect[hand]) {
                                toggledImage = !toggledImage;
                            }
                        }
                    }
                }

                //----------------------------------------------------------------------
                // Image selection

                auto now = std::chrono::steady_clock::now();
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count() > 2000) {
                    whichImage = (whichImage + 1) % (sourceTextures.size() - specialImageCount);
                    then = std::chrono::steady_clock::now();
                }

                if (hoverActive) { // display hovered image if there is an intersection
                    if(!hadVibrated) {
                        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
                        vibration.amplitude = 1;
                        vibration.duration = 500000000;
                        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

                        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                        hapticActionInfo.action = actionState.vibrateAction;
                        hapticActionInfo.subactionPath = actionState.handSubactionPath[1];

                        CHECK_XR(xrApplyHapticFeedback(program.GetSession(), &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
                    }
                    hadVibrated = true;
                    whichImage = hoverImageIdx;
                } else {
                    hadVibrated = false;
                }

                if (toggledImage) { // display toggled image if there is an active select (only valid during intersection)
                    whichImage = toggledImageIdx;
                }

                //----------------------------------------------------------------------
                // Start rendering work

                program.BeginFrame();

                if(shouldRender) {


                    for(int eye = 0; eye < 2; eye++) {
                        uint32_t index;
                        index = program.AcquireAndWaitSwapchainImage(eye);

                        // We don't need to do this every frame because our images are static.
                        // But we do it anyway because a "real" program would render something new
                        // every time through the frame loop.
                        d3dContext->CopyResource(program.GetSwapchainImage(eye, index).texture, sourceTextures[whichImage]);

                        program.ReleaseSwapchainImage(eye);

                    }
                    d3dContext->Flush();

                    XrCompositionLayerFlags flags = XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT |
                        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    XrExtent2Df extent {1.0f * imageAspectRatios[whichImage], 1.0f};

                    if(useSeparateLeftRightEyes) {

                        XrSwapchainSubImage leftImage {program.GetSwapchain(0), {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                        program.AddLayer(flags, XR_EYE_VISIBILITY_LEFT, leftImage, layerImagePose, extent);

                        XrSwapchainSubImage rightImage {program.GetSwapchain(1), {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                        program.AddLayer(flags, XR_EYE_VISIBILITY_RIGHT, rightImage, layerImagePose, extent);

                    } else {

                        XrSwapchainSubImage fullImage {program.GetSwapchain(0), {{0, 0}, {recommendedWidth, recommendedHeight}}, 0};
                        program.AddLayer(flags, XR_EYE_VISIBILITY_BOTH, fullImage, layerImagePose, extent);
                    }
                }

                //----------------------------------------------------------------------
                // End rendering work

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

    CHECK_XR(xrDestroyAction(actionState.poseAction));
    CHECK_XR(xrDestroyAction(actionState.selectAction));
    CHECK_XR(xrDestroyActionSet(actionState.actionSet));

    return 0;
}
