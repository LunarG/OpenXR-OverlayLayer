// Remote.cpp 

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

#define CHECK_NOT_NULL(a) CheckResultWithLastError(((a) != NULL), #a, __FILE__, __LINE__)

#define CHECK_LAST_ERROR(a) CheckResultWithLastError((a), #a, __FILE__, __LINE__)

#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)

namespace Math {
namespace Pose {
XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

XrPosef Translation(const XrVector3f& translation) {
    XrPosef t = Identity();
    t.position = translation;
    return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
    XrPosef t = Identity();
    t.orientation.x = 0.f;
    t.orientation.y = std::sin(radians * 0.5f);
    t.orientation.z = 0.f;
    t.orientation.w = std::cos(radians * 0.5f);
    t.position = translation;
    return t;
}
}  // namespace Pose
}  // namespace Math

XrResult ipcxrHandshake(
	XrInstance *instance,
	XrSystemId *systemId,
	LUID *luid,
	DWORD *hostProcessId);

const uint64_t ONE_SECOND_IN_NANOSECONDS = 1000000000;

int main( void )
{
    bool sawFirstSuccessfulFrame = false;

    // RPC Initialization not generic to OpenXR
    XrInstance instance;
    XrSystemId systemId;
    LUID adapterLUID;
	DWORD hostProcessId;
    CHECK_XR(ipcxrHandshake(&instance, &systemId, &adapterLUID, &hostProcessId));
    std::cout << "Remote process handshake succeeded!\n";

    // Give us our best chance of success of sharing our Remote
    // swapchainImages by creating our D3D device on the same adapter as
    // the Host application's device
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

    // From here should be fairly generic OpenXR code

    XrSessionCreateInfoOverlayEXT sessionCreateInfoOverlay{(XrStructureType)XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXT};
    sessionCreateInfoOverlay.next = nullptr;
    sessionCreateInfoOverlay.overlaySession = XR_TRUE;
    sessionCreateInfoOverlay.sessionLayersPlacement = 1;

    XrGraphicsBindingD3D11KHR d3dBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    d3dBinding.device = d3d11Device;
    d3dBinding.next = &sessionCreateInfoOverlay;

    XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.systemId = systemId;
    sessionCreateInfo.next = &d3dBinding;

    XrSession session;
    CHECK_XR(xrCreateSession(instance, &sessionCreateInfo, &session));
    std::cout << "CreateSession with XrSessionCreateInfoOverlayEXT succeeded!\n";

    XrSpace viewSpace;
    XrReferenceSpaceCreateInfo createSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    createSpaceInfo.next = nullptr;
    createSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    // Render head-locked 1.5m in front of device.
    createSpaceInfo.poseInReferenceSpace = Math::Pose::Translation({-1.0f, 0.5f, -1.5f});
    CHECK_XR(xrCreateReferenceSpace(session, &createSpaceInfo, &viewSpace));

    uint64_t chosenFormat;
    {
        uint32_t count;
        CHECK_XR(xrEnumerateSwapchainFormats(session, 0, &count, nullptr));
        std::vector<int64_t> runtimeFormats(count);
        CHECK_XR(xrEnumerateSwapchainFormats(session, (uint32_t)count, &count, runtimeFormats.data()));
        std::vector<DXGI_FORMAT> appFormats { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
        auto formatFound = std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), appFormats.begin(), appFormats.end());
        if(formatFound == runtimeFormats.end()) {
            OutputDebugStringA("No supported swapchain format found\n");
            // XXX Do something smarter here
            DebugBreak();
        }
        chosenFormat = *formatFound;
    }

    XrSwapchain swapchains[2];
    XrSwapchainImageD3D11KHR *swapchainImages[2];
    for(int eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = chosenFormat;
        swapchainCreateInfo.width = 512;
        swapchainCreateInfo.height = 512;
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

    ID3D11DeviceContext* d3dContext;
    d3d11Device->GetImmediateContext(&d3dContext);

    ID3D11Texture2D* sourceImages[2];
    for(int i = 0; i < 2; i++) {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = 512;
        desc.Height = 512;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN ;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        CHECK(d3d11Device->CreateTexture2D(&desc, NULL, &sourceImages[i]));

        D3D11_MAPPED_SUBRESOURCE mapped;
        CHECK(d3dContext->Map(sourceImages[i], 0, D3D11_MAP_WRITE, 0, &mapped));
        if(i == 0)
            memcpy(mapped.pData, Image2Bytes, 512 * 512 * 4);
        else
            memcpy(mapped.pData, Image1Bytes, 512 * 512 * 4);
        d3dContext->Unmap(sourceImages[i], 0);
    }

    std::cout << "Created Swapchain and enumerated SwapchainImages and made local\n";
    std::cout << "    images as texture sources!\n";

    // Spawn a thread to wait for a keypress
    static bool quit = false;
    auto exitPollingThread = std::thread{[] {
        std::cout << "Press ENTER to exit...\n";
        getchar();
        quit = true;
    }};
    exitPollingThread.detach();

    XrSessionBeginInfo beginInfo = {XR_TYPE_SESSION_BEGIN_INFO, nullptr, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
    CHECK_XR(xrBeginSession(session, &beginInfo));

    int whichImage = 0;
    auto then = std::chrono::steady_clock::now();
    while(!quit) {
        auto now = std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count() > 1000) {
            whichImage = (whichImage + 1) % 2;
            then = std::chrono::steady_clock::now();
        }

        XrFrameState waitFrameState;
        xrWaitFrame(session, nullptr, &waitFrameState);
        xrBeginFrame(session, nullptr);
        for(int eye = 0; eye < 2; eye++) {
            uint32_t index;
            XrSwapchain sc = reinterpret_cast<XrSwapchain>(swapchains[eye]);
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            acquireInfo.next = nullptr;
            // TODO - these should be layered
            CHECK_XR(xrAcquireSwapchainImage(sc, &acquireInfo, &index));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.next = nullptr;
            waitInfo.timeout = ONE_SECOND_IN_NANOSECONDS;
            CHECK_XR(xrWaitSwapchainImage(sc, &waitInfo));

            d3dContext->CopyResource(swapchainImages[eye][index].texture, sourceImages[whichImage]);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            releaseInfo.next = nullptr;
            CHECK_XR(xrReleaseSwapchainImage(sc, &releaseInfo));
        }

        d3dContext->Flush();

        XrCompositionLayerQuad layers[2];
        XrCompositionLayerBaseHeader* layerPointers[2];
        for(uint32_t eye = 0; eye < 2; eye++) {
            XrSwapchainSubImage fullImage = {swapchains[eye], {{0, 0}, {512, 512}}, 0};
            layerPointers[eye] = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layers[eye]);
            layers[eye].type = XR_TYPE_COMPOSITION_LAYER_QUAD;
            layers[eye].next = nullptr;
            layers[eye].layerFlags = 0;
            layers[eye].space = viewSpace;
            layers[eye].eyeVisibility = (eye == 0) ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            layers[eye].subImage = fullImage;
            layers[eye].pose = Math::Pose::Identity();
            layers[eye].size = {0.33f, 0.33f};
        }
        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.next = nullptr;
        frameEndInfo.layers = layerPointers;
        frameEndInfo.layerCount = 2;
        frameEndInfo.displayTime = waitFrameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE; // XXX ignored
        CHECK_XR(xrEndFrame(session, &frameEndInfo));
        
        if(!sawFirstSuccessfulFrame) {
            sawFirstSuccessfulFrame = true;
            std::cout << "First Overlay xrEndFrame was successful!  Continuing...\n";
        }
    }

    CHECK_XR(xrEndSession(session));

    for(int eye = 0; eye < 2; eye++) {
        CHECK_XR(xrDestroySwapchain(swapchains[eye]));
    }

    CHECK_XR(xrDestroySpace(viewSpace));

    CHECK_XR(xrDestroySession(session));

    return 0;
}
