#pragma once

#include <memory>
#include <DirectXMath.h>
#include "EffectRuntime/EffectMeshVariant.h"

class IResourceFactory;
class IShader;
class IInputLayout;
class IPipelineState;
class ITexture;
class ICommandList;

// Phase B Route B の mesh effect 用 ubershader。
// 単一 PSO で gVariantFlags により runtime 分岐する。
// ModelRenderer ではなく EffectMeshPass が所有し、draw は queue せず inline で行う。
class EffectMeshShader
{
public:
    explicit EffectMeshShader(IResourceFactory* factory);
    ~EffectMeshShader();

    IPipelineState* GetPipelineState() const { return m_pso.get(); }
    IInputLayout*   GetInputLayout()   const { return m_inputLayout.get(); }

    // 現在の packet 用 CbMeshEffect(b3) を作成して upload する。
    struct CbMeshEffect
    {
        float              dissolveAmount;
        float              dissolveEdge;
        DirectX::XMFLOAT2  flowSpeed;
        DirectX::XMFLOAT4  dissolveGlowColor;
        float              fresnelPower;
        DirectX::XMFLOAT3  _pad0;
        DirectX::XMFLOAT4  fresnelColor;
        float              flowStrength;
        float              alphaFade;
        DirectX::XMFLOAT2  scrollSpeed;
        float              distortStrength;
        DirectX::XMFLOAT3  _pad1;
        DirectX::XMFLOAT4  rimColor;
        float              rimPower;
        DirectX::XMFLOAT3  _pad2;
        DirectX::XMFLOAT4  emissionColor;
        float              emissionIntensity;
        float              effectTime;
        DirectX::XMFLOAT2  _pad3;
        DirectX::XMFLOAT4  baseColor;
        uint32_t           variantFlags;
        DirectX::XMFLOAT3  _pad4;
        DirectX::XMFLOAT4  lightDirection;
        DirectX::XMFLOAT4  cameraPosition;
    };
    static_assert(sizeof(CbMeshEffect) % 16 == 0, "CbMeshEffect must be 16-byte aligned");

    void UploadConstants(ICommandList* cmd, const CbMeshEffect& cb) const;

    // 6 個の variant texture を t0..t5 へ bind する。未使用 slot は nullptr を渡す。
    void BindTextures(ICommandList* cmd,
        ITexture* base, ITexture* mask, ITexture* normal,
        ITexture* flow, ITexture* sub,  ITexture* emission) const;
    void UnbindTextures(ICommandList* cmd) const;

private:
    std::unique_ptr<IShader>        m_vs;
    std::unique_ptr<IShader>        m_ps;
    std::unique_ptr<IInputLayout>   m_inputLayout;
    std::unique_ptr<IPipelineState> m_pso;
};
