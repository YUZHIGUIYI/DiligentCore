/*
 *  Copyright 2019-2023 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "pch.h"

#include "EngineWebGPUImplTraits.hpp"
#include "SwapChainWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "DeviceContextWebGPUImpl.hpp"
#include "RenderPassWebGPUImpl.hpp"
#include "FramebufferWebGPUImpl.hpp"
#include "TextureViewWebGPUImpl.hpp"
#include "PipelineStateWebGPUImpl.hpp"
#include "PipelineResourceAttribsWebGPU.hpp"
#include "ShaderResourceCacheWebGPU.hpp"
#include "BufferWebGPUImpl.hpp"
#include "BufferViewWebGPUImpl.hpp"
#include "SamplerWebGPUImpl.hpp"
#include "WebGPUTypeConversions.hpp"

#ifdef PLATFORM_WIN32
#    include <Windows.h>
#endif

namespace Diligent
{

namespace
{
constexpr char ShaderSource[] = R"(
@group(0) @binding(0) var TextureSrc: texture_2d<f32>;
@group(0) @binding(1) var SamplerPoint: sampler;

struct VertexOutput 
{
    @builtin(position) Position: vec4f;
    @location(0)       Texcoord: vec2f;
};

@vertex
fn VSMain(@builtin(vertex_index) VertexId: u32) -> VertexOutput 
{
    let Texcoord: vec2f = vec2f((VertexId << 1u) & 2.0f, VertexId & 2.0f);
    let Position: vec4f = vec4f(Texcoord * vec2f(2.0f, -2.0f) + vec2f(-1.0f, 1.0f), 1.0f, 1.0f);
    
    var Output : VertexOutput;
    output.Position = Position;
    output.Texcoord = Texcoord;
    return Output;
}

@fragment
fn PSMain(Input: VertexOutput) -> @location(0) vec4f 
{
    return textureSample(TextureSrc, SamplerPoint, Input.Texcoord, 0);
}
)";

}

class WebGPUSwapChainPresentCommand
{
public:
    WebGPUSwapChainPresentCommand(IRenderDeviceWebGPU* pRenderDevice) :
        m_pRenderDevice{pRenderDevice}
    {
        WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
        wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgpuShaderCodeDesc.code        = ShaderSource;

        WGPUShaderModuleDescriptor wgpuShaderModuleDesc{};
        wgpuShaderModuleDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
        WebGPUShaderModuleWrapper wgpuShaderModule{wgpuDeviceCreateShaderModule(m_pRenderDevice->GetWebGPUDevice(), &wgpuShaderModuleDesc)};
        if (!wgpuShaderModule)
            LOG_ERROR_AND_THROW("Failed to create shader module");

        WGPUBindGroupLayoutEntry wgpuBindGroupLayoutEntries[2]{};
        wgpuBindGroupLayoutEntries[0].binding               = 0;
        wgpuBindGroupLayoutEntries[0].visibility            = WGPUShaderStage_Fragment;
        wgpuBindGroupLayoutEntries[0].texture.sampleType    = WGPUTextureSampleType_Float;
        wgpuBindGroupLayoutEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

        wgpuBindGroupLayoutEntries[1].binding      = 1;
        wgpuBindGroupLayoutEntries[1].visibility   = WGPUShaderStage_Fragment;
        wgpuBindGroupLayoutEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;

        WGPUBindGroupLayoutDescriptor wgpuBindGroupLayoutDesc{};
        wgpuBindGroupLayoutDesc.entryCount = _countof(wgpuBindGroupLayoutEntries);
        wgpuBindGroupLayoutDesc.entries    = wgpuBindGroupLayoutEntries;
        m_wgpuBindGroupLayout.Reset(wgpuDeviceCreateBindGroupLayout(m_pRenderDevice->GetWebGPUDevice(), &wgpuBindGroupLayoutDesc));
        if (!m_wgpuBindGroupLayout)
            LOG_ERROR_AND_THROW("Failed to create bind group layout");

        WGPUPipelineLayoutDescriptor wgpuPipelineLayoutDesc{};
        wgpuPipelineLayoutDesc.bindGroupLayoutCount = 1;
        wgpuPipelineLayoutDesc.bindGroupLayouts     = &m_wgpuBindGroupLayout.Get();
        m_wgpuPipelineLayout.Reset(wgpuDeviceCreatePipelineLayout(m_pRenderDevice->GetWebGPUDevice(), &wgpuPipelineLayoutDesc));
        if (!m_wgpuPipelineLayout)
            LOG_ERROR_AND_THROW("Failed to create pipeline layout");

        WGPUFragmentState wgpuFragmentState;
        wgpuFragmentState.module     = wgpuShaderModule.Get();
        wgpuFragmentState.entryPoint = "PMain";

        WGPURenderPipelineDescriptor wgpuRenderPipelineDesc{};
        wgpuRenderPipelineDesc.label              = "SwapChainPresentPSO";
        wgpuRenderPipelineDesc.layout             = m_wgpuPipelineLayout.Get();
        wgpuRenderPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        wgpuRenderPipelineDesc.vertex.module      = wgpuShaderModule.Get();
        wgpuRenderPipelineDesc.vertex.entryPoint  = "VSMain";
        wgpuRenderPipelineDesc.fragment           = &wgpuFragmentState;
        m_wgpuRenderPipeline.Reset(wgpuDeviceCreateRenderPipeline(m_pRenderDevice->GetWebGPUDevice(), &wgpuRenderPipelineDesc));
        if (!m_wgpuPipelineLayout)
            LOG_ERROR_AND_THROW("Failed to create render pipeline");

        SamplerDesc Desc{};
        Desc.Name      = "Sampler SwapChainPresent";
        Desc.MinFilter = FILTER_TYPE_POINT;
        Desc.MagFilter = FILTER_TYPE_POINT;
        Desc.MipFilter = FILTER_TYPE_POINT;
        m_pRenderDevice->CreateSampler(Desc, &m_pPointSampler);
    }

    void Execute(ITextureViewWebGPU* pTexture, ISwapChainWebGPU* pSwapChain)
    {
        WGPUSurfaceTexture wgpuSurfaceTexture;
        wgpuSurfaceGetCurrentTexture(pSwapChain->GetWebGPUSurface(), &wgpuSurfaceTexture);

        switch (wgpuSurfaceTexture.status)
        {
            case WGPUSurfaceGetCurrentTextureStatus_Success:
                break;

            case WGPUSurfaceGetCurrentTextureStatus_Timeout:
            case WGPUSurfaceGetCurrentTextureStatus_Outdated:
            case WGPUSurfaceGetCurrentTextureStatus_Lost:
                break;

            case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
            case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
                LOG_ERROR_MESSAGE("Failed to acquire next frame");
                break;
            default:
                break;
        }


        WebGPUTextureViewWrapper wgpuTextureView{wgpuSwapChainGetCurrentTextureView(pSwapChain->GetWebGPUSwapChain())};
        if (!wgpuTextureView)
            LOG_ERROR_MESSAGE("Failed to acquire next frame");

        WGPUBindGroupEntry wgpuBindGroupEntries[2]{};
        wgpuBindGroupEntries[0].binding     = 0;
        wgpuBindGroupEntries[0].textureView = pTexture->GetWebGPUTextureView();

        wgpuBindGroupEntries[1].binding = 1;
        wgpuBindGroupEntries[1].sampler = m_pPointSampler.RawPtr<SamplerWebGPUImpl>()->GetWebGPUSampler();

        WGPUBindGroupDescriptor wgpuBindGroupDesc{};
        wgpuBindGroupDesc.entries    = wgpuBindGroupEntries;
        wgpuBindGroupDesc.entryCount = _countof(wgpuBindGroupEntries);
        wgpuBindGroupDesc.layout     = m_wgpuBindGroupLayout.Get();

        WebGPUBindGroupWrapper wgpuBindGroup{wgpuDeviceCreateBindGroup(m_pRenderDevice->GetWebGPUDevice(), &wgpuBindGroupDesc)};

        WGPUCommandEncoderDescriptor wgpuCmdEncoderDesc{};
        WGPUCommandEncoder           wgpuCmdEncoder = wgpuDeviceCreateCommandEncoder(m_pRenderDevice->GetWebGPUDevice(), &wgpuCmdEncoderDesc);

        WGPURenderPassColorAttachment Attachments[] = {
            {wgpuTextureView.Get(), nullptr, WGPULoadOp_Undefined, WGPUStoreOp_Store, {}}};

        WGPURenderPassDescriptor wgpuRenderPassDesc{};
        wgpuRenderPassDesc.colorAttachmentCount     = _countof(Attachments);
        wgpuRenderPassDesc.colorAttachments         = Attachments;
        WGPURenderPassEncoder wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(wgpuCmdEncoder, &wgpuRenderPassDesc);

        wgpuRenderPassEncoderSetPipeline(wgpuRenderPassEncoder, m_wgpuRenderPipeline.Get());
        wgpuRenderPassEncoderSetBindGroup(wgpuRenderPassEncoder, 0, wgpuBindGroup.Get(), 0, nullptr);
        wgpuRenderPassEncoderDraw(wgpuRenderPassEncoder, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(wgpuRenderPassEncoder);

        WGPUCommandBufferDescriptor wgpuCmdBufferDesc{};
        WGPUCommandBuffer           wgpuCmdBuffer = wgpuCommandEncoderFinish(wgpuCmdEncoder, &wgpuCmdBufferDesc);
        wgpuQueueSubmit(wgpuDeviceGetQueue(m_pRenderDevice->GetWebGPUDevice()), 1, &wgpuCmdBuffer);
        wgpuSurfacePresent(pSwapChain->GetWebGPUSurface());
    }

private:
    RefCntAutoPtr<IRenderDeviceWebGPU> m_pRenderDevice;
    RefCntAutoPtr<ISampler>            m_pPointSampler;
    WebGPUBindGroupLayoutWrapper       m_wgpuBindGroupLayout;
    WebGPUPipelineLayoutWrapper        m_wgpuPipelineLayout;
    WebGPURenderPipelineWrapper        m_wgpuRenderPipeline;
};

SwapChainWebGPUImpl::SwapChainWebGPUImpl(IReferenceCounters*      pRefCounters,
                                         const SwapChainDesc&     SCDesc,
                                         RenderDeviceWebGPUImpl*  pRenderDevice,
                                         DeviceContextWebGPUImpl* pDeviceContext,
                                         const NativeWindow&      Window) :
    // clang-format off
    TSwapChainBase
    {
        pRefCounters,
        pRenderDevice,
        pDeviceContext,
        SCDesc
    },
    m_NativeWindow(Window),
    m_pCmdPresent(std::make_unique<WebGPUSwapChainPresentCommand>(pRenderDevice))
// clang-format on
{
    CreateSurface();
    CreateSwapChain();
    CreateBuffersAndViews();
}

SwapChainWebGPUImpl::~SwapChainWebGPUImpl() = default;

IMPLEMENT_QUERY_INTERFACE(SwapChainWebGPUImpl, IID_SwapChainWebGPU, TSwapChainBase)

void SwapChainWebGPUImpl::Present(Uint32 SyncInterval)
{
    if (SyncInterval != 0 && SyncInterval != 1)
        LOG_WARNING_MESSAGE_ONCE("WebGPU only supports 0 and 1 present intervals");

    auto  pDeviceContext = m_wpDeviceContext.Lock();
    auto* pRenderDevice  = m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>();
    if (!pDeviceContext)
    {
        LOG_ERROR_MESSAGE("Immediate context has been released");
        return;
    }

    auto* pImmediateCtxWebGPU = pDeviceContext.RawPtr<DeviceContextWebGPUImpl>();

    pImmediateCtxWebGPU->Flush();
    m_pCmdPresent->Execute(m_pBackBufferSRV, this);

    if (m_SwapChainDesc.IsPrimary)
    {
        pImmediateCtxWebGPU->FinishFrame();
        pRenderDevice->ReleaseStaleResources();
    }

    const bool EnableVSync = SyncInterval != 0;
    if (m_VSyncEnabled != EnableVSync)
    {
        m_VSyncEnabled = EnableVSync;
        RecreateSwapChain();
    }
}

void SwapChainWebGPUImpl::Resize(Uint32            NewWidth,
                                 Uint32            NewHeight,
                                 SURFACE_TRANSFORM NewPreTransform)
{
    if (TSwapChainBase::Resize(NewWidth, NewHeight, NewPreTransform))
        RecreateSwapChain();
}

void SwapChainWebGPUImpl::SetFullscreenMode(const DisplayModeAttribs& DisplayMode)
{
    UNSUPPORTED("WebGPU does not support switching to the fullscreen mode");
}

void SwapChainWebGPUImpl::SetWindowedMode()
{
    UNSUPPORTED("WebGPU does not support switching to the windowed mode");
}

void SwapChainWebGPUImpl::CreateSurface()
{
    const auto* pRenderDeviceWebGPU = m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>();

#if PLATFORM_WIN32
    WGPUSurfaceDescriptorFromWindowsHWND wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain     = {nullptr, WGPUSType_SurfaceDescriptorFromWindowsHWND};
    wgpuSurfaceNativeDesc.hwnd      = m_NativeWindow.hWnd;
    wgpuSurfaceNativeDesc.hinstance = GetModuleHandle(nullptr);
#elif PLATFORM_LINUX
    WGPUSurfaceDescriptorFromXcbWindow wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain      = {nullptr, WGPUSType_SurfaceDescriptorFromXcbWindow};
    wgpuSurfaceNativeDesc.connection = m_NativeWindow.pXCBConnection;
    wgpuSurfaceNativeDesc.window     = m_NativeWindow.WindowId;
#elif PLATFROM_MACOS
    WGPUSurfaceDescriptorFromMetalLayer wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain  = {nullptr, WGPUSType_SurfaceDescriptorFromMetalLayer};
    wgpuSurfaceNativeDesc.window = m_NativeWindow.MetalLayer;
#elif PLATFORM_EMSCRIPTEN
    WGPUSurfaceDescriptorFromCanvasHTMLSelector wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain    = {nullptr, WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector};
    wgpuSurfaceNativeDesc.selector = m_NativeWindow.Selector;
#endif

    WGPUSurfaceDescriptor wgpuSurfaceDesc{};
    wgpuSurfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuSurfaceNativeDesc);

    m_wgpuSurface.Reset(wgpuInstanceCreateSurface(pRenderDeviceWebGPU->GetWebGPUInstance(), &wgpuSurfaceDesc));
    if (!m_wgpuSurface)
        LOG_ERROR_AND_THROW("Failed to create OS-specific surface");
}

void SwapChainWebGPUImpl::CreateSwapChain()
{
    const auto* pRenderDeviceWebGPU = m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>();

    WGPUSurfaceCapabilities wgpuSurfaceCapabilities{};
    wgpuSurfaceGetCapabilities(m_wgpuSurface.Get(), pRenderDeviceWebGPU->GetWebGPUAdapter(), &wgpuSurfaceCapabilities);

    std::vector<WGPUTextureFormat>      Formats(wgpuSurfaceCapabilities.formatCount);
    std::vector<WGPUPresentMode>        PresentModes(wgpuSurfaceCapabilities.presentModeCount);
    std::vector<WGPUCompositeAlphaMode> CompositeAlphaModes(wgpuSurfaceCapabilities.alphaModeCount);
    wgpuSurfaceCapabilities.formats      = Formats.data();
    wgpuSurfaceCapabilities.presentModes = PresentModes.data();
    wgpuSurfaceCapabilities.alphaModes   = CompositeAlphaModes.data();
    wgpuSurfaceGetCapabilities(m_wgpuSurface.Get(), pRenderDeviceWebGPU->GetWebGPUAdapter(), &wgpuSurfaceCapabilities);

    auto SelectPresentMode = [&]() -> WGPUPresentMode {
        WGPUPresentMode Result = WGPUPresentMode_Fifo;

        std::vector<WGPUPresentMode> PreferredPresentModes;
        if (m_VSyncEnabled)
        {
            PreferredPresentModes.push_back(WGPUPresentMode_Fifo);
        }
        else
        {
            PreferredPresentModes.push_back(WGPUPresentMode_Mailbox);
            PreferredPresentModes.push_back(WGPUPresentMode_Immediate);
            PreferredPresentModes.push_back(WGPUPresentMode_Fifo);
        }

        for (auto PreferredMode : PreferredPresentModes)
        {
            if (std::find(PresentModes.begin(), PresentModes.end(), PreferredMode) != PresentModes.end())
            {
                Result = PreferredMode;
                break;
            }
        }

        return Result;
    };

    //  TODO
    auto SelectUsage = [&]() -> WGPUTextureUsageFlags {
        WGPUTextureUsageFlags Result = {};

        DEV_CHECK_ERR(m_SwapChainDesc.Usage != 0, "No swap chain usage flags defined");
        static_assert(SWAP_CHAIN_USAGE_LAST == 8, "Please update this function to handle the new swap chain usage");

        if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_RENDER_TARGET)
            Result |= WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
        if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_SHADER_RESOURCE)
            Result |= WGPUTextureUsage_TextureBinding;
        if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_COPY_SOURCE)
            Result |= WGPUTextureUsage_CopySrc;

        return Result;
    };

    /*

    WGPUSwapChainDescriptor wgpuSwapChainDesc{};
    wgpuSwapChainDesc.width       = m_SwapChainDesc.Width;
    wgpuSwapChainDesc.height      = m_SwapChainDesc.Height;
    wgpuSwapChainDesc.format      = wgpuSurfaceGetPreferredFormat(m_wgpuSurface.Get(), pRenderDeviceWebGPU->GetWebGPUAdapter());
    wgpuSwapChainDesc.presentMode = SelectPresentMode();
    wgpuSwapChainDesc.usage       = SelectUsage();

    m_wgpuSwapChain.Reset(wgpuDeviceCreateSwapChain(pRenderDeviceWebGPU->GetWebGPUDevice(), m_wgpuSurface.Get(), &wgpuSwapChainDesc));
    if (!m_wgpuSwapChain)
        LOG_ERROR_AND_THROW("Failed to create WebGPU swap chain");
    */
}

void SwapChainWebGPUImpl::CreateBuffersAndViews()
{
    TextureDesc BackBufferDesc{};
    BackBufferDesc.Type        = RESOURCE_DIM_TEX_2D;
    BackBufferDesc.Width       = m_SwapChainDesc.Width;
    BackBufferDesc.Height      = m_SwapChainDesc.Height;
    BackBufferDesc.Format      = m_SwapChainDesc.ColorBufferFormat;
    BackBufferDesc.SampleCount = 1;
    BackBufferDesc.Usage       = USAGE_DEFAULT;
    BackBufferDesc.BindFlags   = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    BackBufferDesc.Name        = "Main back buffer";

    RefCntAutoPtr<ITexture> pBackBufferTex;
    m_pRenderDevice->CreateTexture(BackBufferDesc, nullptr, &pBackBufferTex);
    m_pBackBufferRTV = RefCntAutoPtr<ITextureViewWebGPU>(pBackBufferTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET), IID_TextureWebGPU);
    m_pBackBufferSRV = RefCntAutoPtr<ITextureViewWebGPU>(pBackBufferTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE), IID_TextureWebGPU);

    if (m_SwapChainDesc.DepthBufferFormat != TEX_FORMAT_UNKNOWN)
    {
        TextureDesc DepthBufferDesc{};
        DepthBufferDesc.Type        = RESOURCE_DIM_TEX_2D;
        DepthBufferDesc.Width       = m_SwapChainDesc.Width;
        DepthBufferDesc.Height      = m_SwapChainDesc.Height;
        DepthBufferDesc.Format      = m_SwapChainDesc.DepthBufferFormat;
        DepthBufferDesc.SampleCount = 1;
        DepthBufferDesc.Usage       = USAGE_DEFAULT;
        DepthBufferDesc.BindFlags   = BIND_DEPTH_STENCIL;

        DepthBufferDesc.ClearValue.Format               = DepthBufferDesc.Format;
        DepthBufferDesc.ClearValue.DepthStencil.Depth   = m_SwapChainDesc.DefaultDepthValue;
        DepthBufferDesc.ClearValue.DepthStencil.Stencil = m_SwapChainDesc.DefaultStencilValue;
        DepthBufferDesc.Name                            = "Main depth buffer";
        RefCntAutoPtr<ITexture> pDepthBufferTex;
        m_pRenderDevice->CreateTexture(DepthBufferDesc, nullptr, &pDepthBufferTex);
        m_pDepthBufferDSV = RefCntAutoPtr<ITextureViewWebGPU>(pDepthBufferTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL), IID_TextureWebGPU);
    }
}

void SwapChainWebGPUImpl::ReleaseSwapChainResources()
{
    // if (!m_wgpuSwapChain)
    //     return;

    m_pBackBufferSRV.Release();
    m_pBackBufferRTV.Release();
    m_pDepthBufferDSV.Release();
    // m_wgpuSwapChain.Reset();
}

void SwapChainWebGPUImpl::RecreateSwapChain()
{
    try
    {
        ReleaseSwapChainResources();
        CreateSwapChain();
        CreateBuffersAndViews();
    }
    catch (const std::runtime_error&)
    {
        LOG_ERROR("Failed to recreate the swap chain");
    }
}

} // namespace Diligent
