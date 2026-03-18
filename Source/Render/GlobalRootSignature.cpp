#include "GlobalRootSignature.h"
#include "RHI/ICommandList.h"
#include "RHI/IBuffer.h"
#include "RHI/DX11/DX11Buffer.h"
#include "RHI/DX12/DX12Buffer.h"
#include "RHI/DX12/DX12Device.h"
#include "GpuResourceUtils.h"
#include "RHI/ISampler.h"
#include "RHI/ITexture.h"

// Instance()
GlobalRootSignature& GlobalRootSignature::Instance() {
    static GlobalRootSignature instance;
    return instance;
}

IBuffer* GlobalRootSignature::GetSceneBuffer() const { return m_cbScene.get(); }
IBuffer* GlobalRootSignature::GetShadowBuffer() const { return m_cbShadow.get(); }

GlobalRootSignature::~GlobalRootSignature() = default;

void GlobalRootSignature::Initialize(ID3D11Device* device)
{
    m_cbScene = std::make_unique<DX11Buffer>(device, sizeof(CbScene), BufferType::Constant);
    m_cbShadow = std::make_unique<DX11Buffer>(device, sizeof(CbShadowMap), BufferType::Constant);
    m_isDX12 = false;
}

void GlobalRootSignature::Initialize(DX12Device* device)
{
    m_cbScene = std::make_unique<DX12Buffer>(device, sizeof(CbScene), BufferType::Constant);
    m_cbShadow = std::make_unique<DX12Buffer>(device, sizeof(CbShadowMap), BufferType::Constant);
    m_isDX12 = true;
}

void GlobalRootSignature::BindAll(ICommandList* commandList, const RenderState* renderState, const ShadowMap* shadowMap)
{
    // 1. 螳壽焚繝舌ャ繝輔ぃ縺ｮ繝舌う繝ｳ繝・(Slot 7: Scene, Slot 4: Shadow)
    commandList->VSSetConstantBuffer(7, m_cbScene.get());
    commandList->PSSetConstantBuffer(7, m_cbScene.get());
    if (!m_isDX12) commandList->CSSetConstantBuffer(7, m_cbScene.get());

    if (shadowMap) {
        commandList->VSSetConstantBuffer(4, m_cbShadow.get());
        commandList->PSSetConstantBuffer(4, m_cbShadow.get());
        if (!m_isDX12) commandList->CSSetConstantBuffer(4, m_cbShadow.get());
    }

    if (m_isDX12) return;

    // 2. IBL texture binding (DX11 only: t33=DiffIBL, t34=SpecIBL)
    ITexture* ibls[] = { m_diffIBL, m_specIBL };
    commandList->PSSetTextures(33, 2, ibls);

    // ---------------------------------------------------------
    // 3. 蜈ｱ騾壹し繝ｳ繝励Λ繝ｼ縺ｮ荳諡ｬ繝舌う繝ｳ繝会ｼ・X11 RHI迚茨ｼ・
    // ---------------------------------------------------------

    // Slot 0: LinearWrap
    ISampler* linearSampler = renderState->GetSamplerState(SamplerState::LinearWrap);
    commandList->PSSetSampler(0, linearSampler);

    // Slot 1: ShadowCompare
    ISampler* shadowSampler = shadowMap ? shadowMap->GetSamplerState() : nullptr;
    commandList->PSSetSampler(1, shadowSampler);

    // Slot 2: PointClamp
    ISampler* pointSampler = renderState->GetSamplerState(SamplerState::PointClamp);
    if (!pointSampler) pointSampler = linearSampler;
    commandList->PSSetSampler(2, pointSampler);
}

