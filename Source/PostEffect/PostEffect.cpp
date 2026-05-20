#include "PostEffect/PostEffect.h"

#include "Render/FrameBuffer.h"
#include "Render/Graphics.h"
#include "Console/Logger.h"
#include "RenderContext/RenderContext.h"
#include "RHI/IBuffer.h"
#include "RHI/ICommandList.h"
#include "RHI/IPipelineState.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IShader.h"
#include "RHI/ITexture.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/DX11/DX11Texture.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Texture.h"
#include "System/ResourceManager.h"

#include "imgui.h"

#include <dx11/ffx_fsr2_dx11.h>
#include <dx12/ffx_fsr2_dx12.h>

#include <algorithm>
#include <cstdlib>
#include <d3d11.h>
#include <d3d12.h>
#include <string>

#pragma comment(lib, "dxguid.lib")

namespace {

float ResolveFrameTimeDelta(float currentTimeSec)
{
    static float s_lastTimeSec = 0.0f;
    const float deltaMs = (currentTimeSec - s_lastTimeSec) * 1000.0f;
    s_lastTimeSec = currentTimeSec;
    return (deltaMs <= 0.0f) ? 16.6f : deltaMs;
}

DX12CommandList::PixelTextureBinding MakeBinding(
    uint32_t slot,
    ITexture* tex,
    DX12CommandList::NullSrvKind nullKind = DX12CommandList::NullSrvKind::Texture2D)
{
    DX12CommandList::PixelTextureBinding binding{};
    binding.slot = slot;
    binding.texture = tex;
    binding.nullKind = nullKind;
    return binding;
}

const char* ApiName(GraphicsAPI api)
{
    return (api == GraphicsAPI::DX12) ? "DX12" : "DX11";
}

std::string WideToAscii(const wchar_t* text)
{
    if (!text) {
        return {};
    }

    std::string result;
    while (*text) {
        const wchar_t c = *text++;
        result.push_back((c >= 0 && c < 128) ? static_cast<char>(c) : '?');
    }
    return result;
}

void Fsr2MessageCallback(FfxFsr2MsgType type, const wchar_t* message)
{
    const std::string text = WideToAscii(message);
    if (type == FFX_FSR2_MESSAGE_TYPE_ERROR) {
        LOG_ERROR("[PostEffect][FSR2] %s", text.c_str());
    } else {
        LOG_WARN("[PostEffect][FSR2] %s", text.c_str());
    }
}

struct DX11ResourceAcq {
    ID3D11Resource* resource = nullptr;
    bool ownsRef = false;
};

DX11ResourceAcq AcquireDX11Resource(ITexture* tex)
{
    DX11ResourceAcq acq{};
    if (!tex) {
        return acq;
    }

    auto* dx11Tex = static_cast<DX11Texture*>(tex);
    if (auto* native = dx11Tex->GetNativeResource()) {
        acq.resource = native;
        return acq;
    }

    if (auto* srv = dx11Tex->GetNativeSRV()) {
        srv->GetResource(&acq.resource);
        acq.ownsRef = acq.resource != nullptr;
        return acq;
    }
    if (auto* rtv = dx11Tex->GetNativeRTV()) {
        rtv->GetResource(&acq.resource);
        acq.ownsRef = acq.resource != nullptr;
        return acq;
    }
    if (auto* dsv = dx11Tex->GetNativeDSV()) {
        dsv->GetResource(&acq.resource);
        acq.ownsRef = acq.resource != nullptr;
        return acq;
    }

    return acq;
}

void ReleaseDX11ResourceAcq(DX11ResourceAcq& acq)
{
    if (acq.ownsRef && acq.resource) {
        acq.resource->Release();
    }
    acq.resource = nullptr;
    acq.ownsRef = false;
}

PipelineStateDesc MakeFullscreenPsoDesc(IShader* vs, IShader* ps, TextureFormat rtvFormat)
{
    auto* renderState = Graphics::Instance().GetRenderState();

    PipelineStateDesc desc{};
    desc.vertexShader = vs;
    desc.pixelShader = ps;
    desc.inputLayout = nullptr;
    desc.depthStencilState = renderState ? renderState->GetDepthStencilState(DepthState::NoTestNoWrite) : nullptr;
    desc.rasterizerState = renderState ? renderState->GetRasterizerState(RasterizerState::SolidCullNone) : nullptr;
    desc.blendState = renderState ? renderState->GetBlendState(BlendState::Opaque) : nullptr;
    desc.primitiveTopology = PrimitiveTopology::TriangleStrip;
    desc.numRenderTargets = 1;
    desc.rtvFormats[0] = rtvFormat;
    desc.dsvFormat = TextureFormat::Unknown;
    return desc;
}

} // namespace 終端。

PostEffect::PostEffect(IResourceFactory* factory, GraphicsAPI api, void* nativeDevice)
    : m_factory(factory)
    , m_api(api)
    , m_nativeDevice(nativeDevice)
{
    if (!m_factory) {
        return;
    }

    m_fullscreenQuadVS = m_factory->CreateShader(ShaderType::Vertex, "Data/Shader/FullScreenQuadVS.cso");
    m_luminanceExtractionPS = m_factory->CreateShader(ShaderType::Pixel, "Data/Shader/LuminanceExtractionPS.cso");
    m_uberPostPS = m_factory->CreateShader(ShaderType::Pixel, "Data/Shader/BloomPS.cso");
    m_blitPS = m_factory->CreateShader(ShaderType::Pixel, "Data/Shader/PostEffectBlitPS.cso");

    m_constantBuffer = m_factory->CreateBuffer(sizeof(CbPostEffect), BufferType::Constant);

    m_psoLuminance = m_factory->CreatePipelineState(MakeFullscreenPsoDesc(
        m_fullscreenQuadVS.get(),
        m_luminanceExtractionPS.get(),
        TextureFormat::R16G16B16A16_FLOAT));
    m_psoUber = m_factory->CreatePipelineState(MakeFullscreenPsoDesc(
        m_fullscreenQuadVS.get(),
        m_uberPostPS.get(),
        TextureFormat::R16G16B16A16_FLOAT));
    m_psoBlitToHdr = m_factory->CreatePipelineState(MakeFullscreenPsoDesc(
        m_fullscreenQuadVS.get(),
        m_blitPS.get(),
        TextureFormat::R16G16B16A16_FLOAT));
    m_psoBlitToLdr = m_factory->CreatePipelineState(MakeFullscreenPsoDesc(
        m_fullscreenQuadVS.get(),
        m_blitPS.get(),
        TextureFormat::RGBA8_UNORM));

}

PostEffect::~PostEffect()
{
    DestroyFsr2Context();
}

void PostEffect::DestroyFsr2Context()
{
    if (m_fsr2Initialized) {
        ffxFsr2ContextDestroy(&m_fsr2Context);
        m_fsr2Initialized = false;
    }

    if (m_fsr2Scratch) {
        std::free(m_fsr2Scratch);
        m_fsr2Scratch = nullptr;
    }

    m_fsr2DisplayWidth = 0;
    m_fsr2DisplayHeight = 0;
    m_fsr2MaxRenderWidth = 0;
    m_fsr2MaxRenderHeight = 0;
    m_fsr2OutputDX12.reset();
}

void PostEffect::OnResize(uint32_t, uint32_t)
{
    DestroyFsr2Context();
}

void PostEffect::EnsureFsr2Context(uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    if (!m_nativeDevice || renderWidth == 0 || renderHeight == 0 || displayWidth == 0 || displayHeight == 0) {
        if (!m_loggedFsr2ContextFailure) {
            LOG_WARN(
                "[PostEffect][FSR2] context skipped api=%s native=%p render=%ux%u display=%ux%u",
                ApiName(m_api),
                m_nativeDevice,
                renderWidth,
                renderHeight,
                displayWidth,
                displayHeight);
            m_loggedFsr2ContextFailure = true;
        }
        return;
    }

    if (m_fsr2Initialized &&
        m_fsr2MaxRenderWidth == renderWidth &&
        m_fsr2MaxRenderHeight == renderHeight &&
        m_fsr2DisplayWidth == displayWidth &&
        m_fsr2DisplayHeight == displayHeight) {
        return;
    }

    DestroyFsr2Context();

    const uint32_t maxRenderW = std::max<uint32_t>(1u, renderWidth);
    const uint32_t maxRenderH = std::max<uint32_t>(1u, renderHeight);

    FfxFsr2ContextDescription contextDesc{};
    contextDesc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_AUTO_EXPOSURE;
#if defined(_DEBUG)
    contextDesc.flags |= FFX_FSR2_ENABLE_DEBUG_CHECKING;
    contextDesc.fpMessage = Fsr2MessageCallback;
#endif
    contextDesc.maxRenderSize.width = maxRenderW;
    contextDesc.maxRenderSize.height = maxRenderH;
    contextDesc.displaySize.width = displayWidth;
    contextDesc.displaySize.height = displayHeight;

    if (m_api == GraphicsAPI::DX11) {
        const size_t scratchSize = ffxFsr2GetScratchMemorySizeDX11();
        m_fsr2Scratch = std::malloc(scratchSize);
        if (!m_fsr2Scratch) {
            if (!m_loggedFsr2ContextFailure) {
                LOG_ERROR("[PostEffect][FSR2] scratch allocation failed api=DX11 size=%zu", scratchSize);
                m_loggedFsr2ContextFailure = true;
            }
            return;
        }

        auto* device = static_cast<ID3D11Device*>(m_nativeDevice);
        if (ffxFsr2GetInterfaceDX11(&m_fsr2Interface, device, m_fsr2Scratch, scratchSize) != FFX_OK) {
            std::free(m_fsr2Scratch);
            m_fsr2Scratch = nullptr;
            if (!m_loggedFsr2ContextFailure) {
                LOG_ERROR("[PostEffect][FSR2] ffxFsr2GetInterfaceDX11 failed");
                m_loggedFsr2ContextFailure = true;
            }
            return;
        }

        contextDesc.callbacks = m_fsr2Interface;
        contextDesc.device = ffxGetDeviceDX11(device);
        const FfxErrorCode err = ffxFsr2ContextCreate(&m_fsr2Context, &contextDesc);
        m_fsr2Initialized = (err == FFX_OK);
        if (!m_fsr2Initialized && !m_loggedFsr2ContextFailure) {
            LOG_ERROR("[PostEffect][FSR2] context create failed api=DX11 err=%d", static_cast<int>(err));
            m_loggedFsr2ContextFailure = true;
        }
    } else {
        const size_t scratchSize = ffxFsr2GetScratchMemorySizeDX12();
        m_fsr2Scratch = std::malloc(scratchSize);
        if (!m_fsr2Scratch) {
            if (!m_loggedFsr2ContextFailure) {
                LOG_ERROR("[PostEffect][FSR2] scratch allocation failed api=DX12 size=%zu", scratchSize);
                m_loggedFsr2ContextFailure = true;
            }
            return;
        }

        auto* device = static_cast<ID3D12Device*>(m_nativeDevice);
        if (ffxFsr2GetInterfaceDX12(&m_fsr2Interface, device, m_fsr2Scratch, scratchSize) != FFX_OK) {
            std::free(m_fsr2Scratch);
            m_fsr2Scratch = nullptr;
            if (!m_loggedFsr2ContextFailure) {
                LOG_ERROR("[PostEffect][FSR2] ffxFsr2GetInterfaceDX12 failed");
                m_loggedFsr2ContextFailure = true;
            }
            return;
        }

        contextDesc.callbacks = m_fsr2Interface;
        contextDesc.device = ffxGetDeviceDX12(device);
        const FfxErrorCode err = ffxFsr2ContextCreate(&m_fsr2Context, &contextDesc);
        m_fsr2Initialized = (err == FFX_OK);
        if (m_fsr2Initialized && m_factory) {
            TextureDesc desc{};
            desc.width = displayWidth;
            desc.height = displayHeight;
            desc.format = TextureFormat::R16G16B16A16_FLOAT;
            desc.bindFlags = TextureBindFlags::ShaderResource | TextureBindFlags::UnorderedAccess;
            m_fsr2OutputDX12 = m_factory->CreateTexture("PostEffect_FSR2Output", desc);
        }
        if (!m_fsr2Initialized && !m_loggedFsr2ContextFailure) {
            LOG_ERROR("[PostEffect][FSR2] context create failed api=DX12 err=%d", static_cast<int>(err));
            m_loggedFsr2ContextFailure = true;
        }
    }

    if (m_fsr2Initialized) {
        m_fsr2MaxRenderWidth = maxRenderW;
        m_fsr2MaxRenderHeight = maxRenderH;
        m_fsr2DisplayWidth = displayWidth;
        m_fsr2DisplayHeight = displayHeight;
        m_loggedFsr2ContextFailure = false;
        LOG_INFO(
            "[PostEffect][FSR2] context ready api=%s render=%ux%u display=%ux%u",
            ApiName(m_api),
            maxRenderW,
            maxRenderH,
            displayWidth,
            displayHeight);
    } else {
        std::free(m_fsr2Scratch);
        m_fsr2Scratch = nullptr;
        m_fsr2OutputDX12.reset();
    }
}

void PostEffect::LuminanceExtraction(const RenderContext& rc, ITexture* luminanceTarget, ITexture* src)
{
    if (!rc.commandList || !luminanceTarget || !src || !m_psoLuminance) {
        return;
    }

    if (m_api == GraphicsAPI::DX12) {
        rc.commandList->TransitionBarrier(src, ResourceState::ShaderResource);
        rc.commandList->TransitionBarrier(luminanceTarget, ResourceState::RenderTarget);
    }

    rc.commandList->SetRenderTarget(luminanceTarget, nullptr);
    rc.commandList->SetViewport(
        0.0f,
        0.0f,
        static_cast<float>(luminanceTarget->GetWidth()),
        static_cast<float>(luminanceTarget->GetHeight()));
    rc.commandList->SetPipelineState(m_psoLuminance.get());
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
    rc.commandList->PSSetConstantBuffer(0, m_constantBuffer.get());

    if (m_api == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        DX12CommandList::PixelTextureBinding bindings[] = {
            MakeBinding(0, src),
        };
        dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
    } else {
        rc.commandList->PSSetTexture(0, src);
        if (rc.renderState) {
            rc.commandList->PSSetSampler(0, rc.renderState->GetSamplerState(SamplerState::LinearClamp));
        }
    }

    rc.commandList->Draw(4, 0);

    if (m_api != GraphicsAPI::DX12) {
        ITexture* nullTexture = nullptr;
        rc.commandList->PSSetTextures(0, 1, &nullTexture);
        rc.commandList->PSSetSampler(0, nullptr);
    }
}

void PostEffect::UberPostProcess(
    const RenderContext& rc,
    ITexture* workTarget,
    ITexture* color,
    ITexture* luminance,
    ITexture* depth,
    ITexture* velocity)
{
    if (!rc.commandList || !workTarget || !color || !luminance || !m_psoUber) {
        return;
    }

    if (m_api == GraphicsAPI::DX12) {
        rc.commandList->TransitionBarrier(color, ResourceState::ShaderResource);
        rc.commandList->TransitionBarrier(luminance, ResourceState::ShaderResource);
        if (depth) {
            rc.commandList->TransitionBarrier(depth, ResourceState::ShaderResource);
        }
        if (velocity) {
            rc.commandList->TransitionBarrier(velocity, ResourceState::ShaderResource);
        }
        rc.commandList->TransitionBarrier(workTarget, ResourceState::RenderTarget);
    }

    rc.commandList->SetRenderTarget(workTarget, nullptr);
    rc.commandList->SetViewport(
        0.0f,
        0.0f,
        static_cast<float>(workTarget->GetWidth()),
        static_cast<float>(workTarget->GetHeight()));
    rc.commandList->SetPipelineState(m_psoUber.get());
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
    rc.commandList->PSSetConstantBuffer(0, m_constantBuffer.get());

    ITexture* lut = m_lutTexture.get();
    if (m_api == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        if (lut) {
            rc.commandList->TransitionBarrier(lut, ResourceState::ShaderResource);
        }
        DX12CommandList::PixelTextureBinding bindings[] = {
            MakeBinding(0, color),
            MakeBinding(1, luminance),
            MakeBinding(2, depth),
            MakeBinding(3, velocity),
            MakeBinding(4, lut),
        };
        dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
    } else {
        ITexture* textures[] = { color, luminance, depth, velocity, lut };
        rc.commandList->PSSetTextures(0, _countof(textures), textures);
        if (rc.renderState) {
            rc.commandList->PSSetSampler(0, rc.renderState->GetSamplerState(SamplerState::LinearClamp));
        }
    }

    rc.commandList->Draw(4, 0);

    if (m_api != GraphicsAPI::DX12) {
        ITexture* nullTextures[5] = {};
        rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
        rc.commandList->PSSetSampler(0, nullptr);
    }
}

void PostEffect::Blit(const RenderContext& rc, ITexture* src, ITexture* dst)
{
    if (!rc.commandList || !src || !dst || !m_psoBlitToHdr || !m_psoBlitToLdr) {
        return;
    }

    if (m_api == GraphicsAPI::DX12) {
        rc.commandList->TransitionBarrier(src, ResourceState::ShaderResource);
        rc.commandList->TransitionBarrier(dst, ResourceState::RenderTarget);
    }

    rc.commandList->SetRenderTarget(dst, nullptr);
    rc.commandList->SetViewport(
        0.0f,
        0.0f,
        static_cast<float>(dst->GetWidth()),
        static_cast<float>(dst->GetHeight()));

    IPipelineState* pso = (dst->GetFormat() == TextureFormat::RGBA8_UNORM)
        ? m_psoBlitToLdr.get()
        : m_psoBlitToHdr.get();
    rc.commandList->SetPipelineState(pso);
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);

    if (m_api == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        DX12CommandList::PixelTextureBinding bindings[] = {
            MakeBinding(0, src),
        };
        dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
    } else {
        rc.commandList->PSSetTexture(0, src);
        if (rc.renderState) {
            rc.commandList->PSSetSampler(0, rc.renderState->GetSamplerState(SamplerState::LinearClamp));
        }
    }

    rc.commandList->Draw(4, 0);

    if (m_api != GraphicsAPI::DX12) {
        ITexture* nullTexture = nullptr;
        rc.commandList->PSSetTextures(0, 1, &nullTexture);
        rc.commandList->PSSetSampler(0, nullptr);
    }
}

bool PostEffect::DispatchFsr2(
    const RenderContext& rc,
    ITexture* color,
    ITexture* depth,
    ITexture* velocity,
    ITexture* output)
{
    if (!m_fsr2Initialized || !rc.commandList || !color || !output) {
        if (!m_loggedFsr2MissingInput) {
            LOG_WARN(
                "[PostEffect][FSR2] dispatch skipped api=%s initialized=%d cmd=%p color=%p output=%p",
                ApiName(m_api),
                m_fsr2Initialized ? 1 : 0,
                rc.commandList,
                color,
                output);
            m_loggedFsr2MissingInput = true;
        }
        return false;
    }
    if ((!depth || !velocity) && !m_loggedFsr2MissingInput) {
        LOG_WARN(
            "[PostEffect][FSR2] dispatch has missing input depth=%p velocity=%p",
            depth,
            velocity);
        m_loggedFsr2MissingInput = true;
    }

    auto logDispatchResult = [&](FfxErrorCode err) {
        if (err == FFX_OK) {
            if (!m_loggedFsr2Success) {
                LOG_INFO(
                    "[PostEffect][FSR2] dispatch ok api=%s render=%ux%u display=%ux%u jitter=(%.4f, %.4f)",
                    ApiName(m_api),
                    color->GetWidth(),
                    color->GetHeight(),
                    m_fsr2DisplayWidth,
                    m_fsr2DisplayHeight,
                    rc.jitterOffset.x,
                    rc.jitterOffset.y);
                m_loggedFsr2Success = true;
            }
            return;
        }

        if (!m_loggedFsr2Fallback) {
            LOG_ERROR(
                "[PostEffect][FSR2] dispatch failed api=%s err=%d render=%ux%u display=%ux%u depth=%p velocity=%p",
                ApiName(m_api),
                static_cast<int>(err),
                color->GetWidth(),
                color->GetHeight(),
                m_fsr2DisplayWidth,
                m_fsr2DisplayHeight,
                depth,
                velocity);
            m_loggedFsr2Fallback = true;
        }
    };

    FfxFsr2DispatchDescription dispatchDesc{};
    dispatchDesc.motionVectorScale.x = static_cast<float>(color->GetWidth());
    dispatchDesc.motionVectorScale.y = static_cast<float>(color->GetHeight());
    dispatchDesc.jitterOffset.x = rc.jitterOffset.x;
    dispatchDesc.jitterOffset.y = rc.jitterOffset.y;
    dispatchDesc.reset = false;
    dispatchDesc.enableSharpening = true;
    dispatchDesc.sharpness = 0.5f;
    dispatchDesc.frameTimeDelta = ResolveFrameTimeDelta(rc.time);
    dispatchDesc.renderSize.width = color->GetWidth();
    dispatchDesc.renderSize.height = color->GetHeight();
    dispatchDesc.preExposure = 1.0f;
    dispatchDesc.cameraNear = (rc.nearZ > 0.0f) ? rc.nearZ : 0.1f;
    dispatchDesc.cameraFar = (rc.farZ > 0.0f) ? rc.farZ : 1000.0f;
    dispatchDesc.cameraFovAngleVertical = (rc.fovY > 0.0f) ? rc.fovY : 1.047f;

    if (m_api == GraphicsAPI::DX11) {
        DX11ResourceAcq acqColor = AcquireDX11Resource(color);
        DX11ResourceAcq acqDepth = AcquireDX11Resource(depth);
        DX11ResourceAcq acqVelocity = AcquireDX11Resource(velocity);
        DX11ResourceAcq acqOutput = AcquireDX11Resource(output);

        bool dispatched = false;
        FfxErrorCode err = FFX_ERROR_INVALID_ARGUMENT;
        if (acqColor.resource && acqOutput.resource) {
            dispatchDesc.commandList = reinterpret_cast<FfxCommandList>(rc.commandList->GetNativeContext());
            dispatchDesc.color = ffxGetResourceDX11(
                &m_fsr2Context,
                acqColor.resource,
                L"FSR2_InputColor",
                FFX_RESOURCE_STATE_COMPUTE_READ);
            if (acqDepth.resource) {
                dispatchDesc.depth = ffxGetResourceDX11(
                    &m_fsr2Context,
                    acqDepth.resource,
                    L"FSR2_InputDepth",
                    FFX_RESOURCE_STATE_COMPUTE_READ);
            }
            if (acqVelocity.resource) {
                dispatchDesc.motionVectors = ffxGetResourceDX11(
                    &m_fsr2Context,
                    acqVelocity.resource,
                    L"FSR2_InputVelocity",
                    FFX_RESOURCE_STATE_COMPUTE_READ);
            }
            dispatchDesc.output = ffxGetResourceDX11(
                &m_fsr2Context,
                acqOutput.resource,
                L"FSR2_Output",
                FFX_RESOURCE_STATE_UNORDERED_ACCESS);

            err = ffxFsr2ContextDispatch(&m_fsr2Context, &dispatchDesc);
            dispatched = (err == FFX_OK);
        } else if (!m_loggedFsr2MissingInput) {
            LOG_WARN(
                "[PostEffect][FSR2] DX11 native resource missing color=%p output=%p depth=%p velocity=%p",
                acqColor.resource,
                acqOutput.resource,
                acqDepth.resource,
                acqVelocity.resource);
            m_loggedFsr2MissingInput = true;
        }

        ReleaseDX11ResourceAcq(acqColor);
        ReleaseDX11ResourceAcq(acqDepth);
        ReleaseDX11ResourceAcq(acqVelocity);
        ReleaseDX11ResourceAcq(acqOutput);
        logDispatchResult(err);
        return dispatched;
    }

    auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
    rc.commandList->TransitionBarrier(color, ResourceState::ShaderResource);
    if (depth) {
        rc.commandList->TransitionBarrier(depth, ResourceState::ShaderResource);
    }
    if (velocity) {
        rc.commandList->TransitionBarrier(velocity, ResourceState::ShaderResource);
    }
    rc.commandList->TransitionBarrier(output, ResourceState::UnorderedAccess);
    dx12Cmd->FlushResourceBarriers();

    auto getDx12Resource = [](ITexture* tex) -> ID3D12Resource* {
        return tex ? static_cast<DX12Texture*>(tex)->GetNativeResource() : nullptr;
    };

    ID3D12Resource* resColor = getDx12Resource(color);
    ID3D12Resource* resDepth = getDx12Resource(depth);
    ID3D12Resource* resVelocity = getDx12Resource(velocity);
    ID3D12Resource* resOutput = getDx12Resource(output);
    if (!resColor || !resOutput) {
        if (!m_loggedFsr2MissingInput) {
            LOG_WARN(
                "[PostEffect][FSR2] DX12 native resource missing color=%p output=%p depth=%p velocity=%p",
                resColor,
                resOutput,
                resDepth,
                resVelocity);
            m_loggedFsr2MissingInput = true;
        }
        return false;
    }

    dispatchDesc.commandList = ffxGetCommandListDX12(dx12Cmd->GetNativeCommandList());
    dispatchDesc.color = ffxGetResourceDX12(
        &m_fsr2Context,
        resColor,
        L"FSR2_InputColor",
        FFX_RESOURCE_STATE_COMPUTE_READ);
    if (resDepth) {
        dispatchDesc.depth = ffxGetResourceDX12(
            &m_fsr2Context,
            resDepth,
            L"FSR2_InputDepth",
            FFX_RESOURCE_STATE_COMPUTE_READ);
    }
    if (resVelocity) {
        dispatchDesc.motionVectors = ffxGetResourceDX12(
            &m_fsr2Context,
            resVelocity,
            L"FSR2_InputVelocity",
            FFX_RESOURCE_STATE_COMPUTE_READ);
    }
    dispatchDesc.output = ffxGetResourceDX12(
        &m_fsr2Context,
        resOutput,
        L"FSR2_Output",
        FFX_RESOURCE_STATE_UNORDERED_ACCESS);

    const FfxErrorCode err = ffxFsr2ContextDispatch(&m_fsr2Context, &dispatchDesc);
    dx12Cmd->RestoreDescriptorHeap();
    output->SetCurrentState(ResourceState::UnorderedAccess);
    logDispatchResult(err);
    return err == FFX_OK;
}

void PostEffect::Process(const RenderContext& rc, ITexture* src, ITexture* dst, ITexture* depth, ITexture* velocity)
{
    if (!rc.commandList || !src || !dst || !m_constantBuffer) {
        return;
    }

    m_cbPostEffect.time = rc.time;
    m_cbPostEffect.luminanceExtractionLowerEdge = rc.bloomData.luminanceLowerEdge;
    m_cbPostEffect.luminanceExtractionHigherEdge = rc.bloomData.luminanceHigherEdge;
    m_cbPostEffect.bloomIntensity = rc.bloomData.bloomIntensity;
    m_cbPostEffect.gaussianSigma = rc.bloomData.gaussianSigma;
    m_cbPostEffect.exposure = rc.colorFilterData.exposure;
    m_cbPostEffect.monoBlend = rc.colorFilterData.monoBlend;
    m_cbPostEffect.hueShift = rc.colorFilterData.hueShift;
    m_cbPostEffect.flashAmount = rc.colorFilterData.flashAmount;
    m_cbPostEffect.vignetteAmount = rc.colorFilterData.vignetteAmount;
    m_cbPostEffect.focusDistance = rc.dofData.focusDistance;
    m_cbPostEffect.focusRange = rc.dofData.focusRange;
    m_cbPostEffect.bokehRadius = rc.dofData.enable ? rc.dofData.bokehRadius : 0.0f;
    m_cbPostEffect.motionBlurIntensity = rc.motionBlurData.intensity;
    m_cbPostEffect.motionBlurSamples = rc.motionBlurData.samples;

    // カラーグレーディング LUT。パスが変わったときだけ読み込み直す。
    if (rc.colorFilterData.lutPath != m_lutLoadedPath) {
        m_lutLoadedPath = rc.colorFilterData.lutPath;
        m_lutTexture = m_lutLoadedPath.empty()
            ? nullptr
            : ResourceManager::Instance().GetTexture(m_lutLoadedPath);
    }
    if (m_lutTexture && rc.colorFilterData.lutAmount > 0.0001f) {
        m_cbPostEffect.lutAmount = rc.colorFilterData.lutAmount;
        m_cbPostEffect.lutSize = static_cast<float>(m_lutTexture->GetHeight());
    } else {
        m_cbPostEffect.lutAmount = 0.0f;
        m_cbPostEffect.lutSize = 0.0f;
    }
    rc.commandList->UpdateBuffer(m_constantBuffer.get(), &m_cbPostEffect, sizeof(m_cbPostEffect));

    Graphics& graphics = Graphics::Instance();
    FrameBuffer* editorDisplayFB = graphics.GetFrameBuffer(FrameBufferId::EditorDisplay);
    const bool isEditorView = editorDisplayFB && dst == editorDisplayFB->GetColorTexture(0);

    FrameBuffer* luminanceFB = graphics.GetFrameBuffer(
        isEditorView ? FrameBufferId::EditorLuminance : FrameBufferId::Luminance);
    FrameBuffer* workFB = graphics.GetFrameBuffer(
        isEditorView ? FrameBufferId::EditorPostProcess : FrameBufferId::PostProcess);
    if (!luminanceFB || !workFB) {
        return;
    }

    ITexture* luminanceTex = luminanceFB->GetColorTexture(0);
    ITexture* workTex = workFB->GetColorTexture(0);
    if (!luminanceTex || !workTex) {
        return;
    }

    luminanceFB->Clear(rc.commandList, 0.0f, 0.0f, 0.0f, 0.0f);
    LuminanceExtraction(rc, luminanceTex, src);
    UberPostProcess(rc, workTex, src, luminanceTex, depth, velocity);

    EnsureFsr2Context(workTex->GetWidth(), workTex->GetHeight(), dst->GetWidth(), dst->GetHeight());

    rc.commandList->SetRenderTarget(nullptr, nullptr);

    if (m_api == GraphicsAPI::DX11) {
        if (!DispatchFsr2(rc, workTex, depth, velocity, dst)) {
            if (!m_loggedFsr2Fallback) {
                LOG_WARN("[PostEffect][FSR2] fallback blit active api=DX11");
                m_loggedFsr2Fallback = true;
            }
            Blit(rc, workTex, dst);
        }
        return;
    }

    if (m_fsr2Initialized && m_fsr2OutputDX12) {
        if (DispatchFsr2(rc, workTex, depth, velocity, m_fsr2OutputDX12.get())) {
            Blit(rc, m_fsr2OutputDX12.get(), dst);
            return;
        }
    } else if (!m_loggedFsr2Fallback) {
        LOG_WARN(
            "[PostEffect][FSR2] fallback blit active api=DX12 initialized=%d output=%p",
            m_fsr2Initialized ? 1 : 0,
            m_fsr2OutputDX12.get());
        m_loggedFsr2Fallback = true;
    }

    Blit(rc, workTex, dst);
}

void PostEffect::DrawDebugGUI()
{
    ImGui::DragFloat("LuminanceLowerEdge", &m_cbPostEffect.luminanceExtractionLowerEdge, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("LuminanceHigherEdge", &m_cbPostEffect.luminanceExtractionHigherEdge, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("GaussianSigma", &m_cbPostEffect.gaussianSigma, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("BloomIntensity", &m_cbPostEffect.bloomIntensity, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat("monoBlend", &m_cbPostEffect.monoBlend, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("hueShift", &m_cbPostEffect.hueShift, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("flashAmount", &m_cbPostEffect.flashAmount, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("vignetteAmount", &m_cbPostEffect.vignetteAmount, 0.01f, 0.0f, 1.0f);
}
