#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <memory>
#include "RenderContext/RenderContext.h"
#include <ffx_fsr2.h>
#include <dx11\ffx_fsr2_dx11.h>

// RHI 前方宣言
class IShader;
class IBuffer;
class ITexture;
class IPipelineState;
class FrameBuffer; // 古いFBも一部残っているため

class PostEffect
{
public:
    PostEffect(ID3D11Device* device);
    ~PostEffect();

    // ====================================================
    // ★ 修正：FrameBuffer ではなく、グラフから来た ITexture を受け取る！
    // ====================================================
    void Process(const RenderContext& rc, ITexture* src, ITexture* dst, ITexture* depth, ITexture* velocity);

    void DrawDebugGUI();

private:
    void LuminanceExtraction(const RenderContext& rc, ITexture* src);
    void UberPostProcess(const RenderContext& rc, ITexture* color, ITexture* luminance, ITexture* depth, ITexture* velocity);

private:
    std::unique_ptr<IShader> fullscreenQuadVS;
    std::unique_ptr<IShader> luminanceExtractionPS;
    std::unique_ptr<IShader> uberPostPS;

    std::unique_ptr<IPipelineState> m_psoLuminance;
    std::unique_ptr<IPipelineState> m_psoUber;

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

    std::unique_ptr<IBuffer> constantBuffer;
    CbPostEffect cbPostEffect;

private:
    FfxFsr2Context  m_fsr2Context;
    FfxFsr2Interface m_fsr2Interface;
    bool m_fsr2Initialized = false;
};