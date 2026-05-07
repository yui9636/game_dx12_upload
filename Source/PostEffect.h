#pragma once

#include "RHI/GraphicsAPI.h"

#include <ffx_fsr2.h>
#include <cstdint>
#include <memory>

class IBuffer;
class IPipelineState;
class IResourceFactory;
class IShader;
class ITexture;
struct RenderContext;

class PostEffect
{
public:
    PostEffect(IResourceFactory* factory, GraphicsAPI api, void* nativeDevice);
    ~PostEffect();

    void Process(const RenderContext& rc, ITexture* src, ITexture* dst, ITexture* depth, ITexture* velocity);
    void OnResize(uint32_t displayWidth, uint32_t displayHeight);
    void DrawDebugGUI();

private:
    void EnsureFsr2Context(uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight);
    void DestroyFsr2Context();

    void LuminanceExtraction(const RenderContext& rc, ITexture* luminanceTarget, ITexture* src);
    void UberPostProcess(
        const RenderContext& rc,
        ITexture* workTarget,
        ITexture* color,
        ITexture* luminance,
        ITexture* depth,
        ITexture* velocity);
    void Blit(const RenderContext& rc, ITexture* src, ITexture* dst);
    bool DispatchFsr2(const RenderContext& rc, ITexture* color, ITexture* depth, ITexture* velocity, ITexture* output);

private:
    IResourceFactory* m_factory = nullptr;
    GraphicsAPI m_api = GraphicsAPI::DX11;
    void* m_nativeDevice = nullptr;

    std::unique_ptr<IShader> m_fullscreenQuadVS;
    std::unique_ptr<IShader> m_luminanceExtractionPS;
    std::unique_ptr<IShader> m_uberPostPS;
    std::unique_ptr<IShader> m_blitPS;

    std::unique_ptr<IPipelineState> m_psoLuminance;
    std::unique_ptr<IPipelineState> m_psoUber;
    std::unique_ptr<IPipelineState> m_psoBlitToHdr;
    std::unique_ptr<IPipelineState> m_psoBlitToLdr;

    struct CbPostEffect
    {
        float luminanceExtractionLowerEdge;
        float luminanceExtractionHigherEdge;
        float gaussianSigma;
        float bloomIntensity;

        float exposure;
        float monoBlend;
        float hueShift;
        float flashAmount;

        float vignetteAmount;
        float time;
        float focusDistance;
        float focusRange;

        float bokehRadius;
        float motionBlurIntensity;
        float motionBlurSamples;
        float _pad[1];
    };

    std::unique_ptr<IBuffer> m_constantBuffer;
    CbPostEffect m_cbPostEffect{};

    FfxFsr2Context m_fsr2Context{};
    FfxFsr2Interface m_fsr2Interface{};
    void* m_fsr2Scratch = nullptr;
    bool m_fsr2Initialized = false;
    uint32_t m_fsr2MaxRenderWidth = 0;
    uint32_t m_fsr2MaxRenderHeight = 0;
    uint32_t m_fsr2DisplayWidth = 0;
    uint32_t m_fsr2DisplayHeight = 0;
    bool m_loggedFsr2ContextFailure = false;
    bool m_loggedFsr2Fallback = false;
    bool m_loggedFsr2Success = false;
    bool m_loggedFsr2MissingInput = false;

    std::unique_ptr<ITexture> m_fsr2OutputDX12;
};
