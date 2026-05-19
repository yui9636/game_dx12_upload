#pragma once

#include "Shader.h"
#include <memory>
#include <DirectXMath.h>

class IResourceFactory;
class IShader;
class IInputLayout;
class IBuffer;
class IPipelineState;
class ITexture;
class MaterialAsset;

// Toon (cel-shaded) shader.
// Per mesh: outline pass (cull-front, vertex expansion along normal)
// then body pass (cull-back, banded NdotL + rim + ramp + shadow PCF).
class ToonShader : public Shader
{
public:
    ToonShader(IResourceFactory* factory);
    ~ToonShader() override = default;

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const ModelResource::MeshResource& mesh) override;
    void BeginInstanced(const RenderContext& rc) override;
    bool SupportsInstancing(const ModelResource::MeshResource& mesh) const override;
    void End(const RenderContext& rc) override;

    void SetMaterialAssetOverride(const MaterialAsset* material) { m_materialOverride = material; }

private:
    struct CbMesh
    {
        DirectX::XMFLOAT4 materialColor;
    };
    // Must match Toon.hlsli CbToon layout (10 float4 = 160 bytes).
    struct CbToon
    {
        DirectX::XMFLOAT4 shadowMid;      // rgb=mid color, a=unused
        DirectX::XMFLOAT4 shadowDeep;     // rgb=deep color, a=unused
        DirectX::XMFLOAT4 shadowParams;   // x=midThreshold, y=deepThreshold, z=bandLevels, w=shadingMode
        DirectX::XMFLOAT4 rimColor;       // rgb=color, a=power
        DirectX::XMFLOAT4 rimAux;         // x=strength, y=outlineDistanceScale, z/w=unused
        DirectX::XMFLOAT4 outlineColor;   // rgb=color, a=width
        DirectX::XMFLOAT4 specColor;      // rgb=spec color, a=strength
        DirectX::XMFLOAT4 specParams;     // x=sharpness, y=threshold, z=useFixedLight, w=useAniso
        DirectX::XMFLOAT4 fixedLight;     // xyz=dir, w=anisoOffset
        DirectX::XMFLOAT4 anisoAux;       // x=anisoSharpness, others unused
    };

    std::shared_ptr<ITexture> m_whiteTexture;
    std::shared_ptr<ITexture> m_neutralNormal;
    std::shared_ptr<ITexture> m_defaultRamp;

    std::unique_ptr<IShader>  m_vs;
    std::unique_ptr<IShader>  m_ps;
    std::unique_ptr<IShader>  m_outlineVs;
    std::unique_ptr<IShader>  m_outlinePs;

    std::unique_ptr<IInputLayout> m_inputLayout;

    std::unique_ptr<IBuffer> m_meshConstantBuffer;
    std::unique_ptr<IBuffer> m_toonConstantBuffer;

    std::unique_ptr<IPipelineState> m_pso;
    std::unique_ptr<IPipelineState> m_outlinePso;

    const MaterialAsset* m_materialOverride = nullptr;
};
