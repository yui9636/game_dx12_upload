#include "EffectMaterial.h"
#include "Graphics.h"
#include "RenderContext/RenderContext.h"
#include "GpuResourceUtils.h"
#include <cassert>
#include "RHI/ICommandList.h"

using namespace DirectX;
using namespace Microsoft::WRL;

// --------------------------------------------------------
// �R���X�g���N�^
// --------------------------------------------------------
EffectMaterial::EffectMaterial()
{
    // �p�����[�^������ (�g����)
    constants.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    constants.emissiveIntensity = 1.0f;
    constants.currentTime = 0.0f;
    constants.mainUvScrollSpeed = { 0.0f, 0.0f };

    constants.distortionStrength = 0.0f;
    constants.distortionUvScrollSpeed = { 0.0f, 0.0f };

    constants.dissolveThreshold = 0.0f;
    constants.dissolveEdgeWidth = 0.05f; // ���悢����
    constants.dissolveEdgeColor = { 1.0f, 0.5f, 0.1f }; // �f�t�H���g�͉��̂悤�ȃI�����W

    // �����[�e�B���O������
    constants.mainTexIndex = 0;       // ���C����Slot0
    constants.distortionTexIndex = -1; // �f�t�H���g����
    constants.dissolveTexIndex = -1;   // �f�t�H���g����
    constants.dissolveEdgeColor = { 1.0f, 0.5f, 0.2f };


    // �萔�o�b�t�@�쐬
    CreateConstantBuffer();
}

// --------------------------------------------------------
// �e�N�X�`���ݒ� (�V�O�l�`���� .h �Ɗ��S�Ɉ�v������)
// --------------------------------------------------------
void EffectMaterial::SetTexture(int slot, const std::string& path, ComPtr<ID3D11ShaderResourceView> texture)
{
    if (slot >= 0 && slot < TEXTURE_SLOT_COUNT)
    {
        textures[slot] = texture;     // GPU���\�[�X��ێ�
        texturePaths[slot] = path;    // �t�@�C���p�X���L�� (�G�f�B�^�\���E�ۑ��p)
    }
}

// --------------------------------------------------------
// �p�X�擾
// --------------------------------------------------------
std::string EffectMaterial::GetTexturePath(int slot) const
{
    if (slot >= 0 && slot < TEXTURE_SLOT_COUNT)
    {
        return texturePaths[slot];
    }
    return "";
}

// --------------------------------------------------------
// �萔�o�b�t�@�쐬 (�����w���p�[)
// --------------------------------------------------------
void EffectMaterial::CreateConstantBuffer()
{
    // DX12 では DX11 定数バッファ直接生成をスキップ
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) return;

    // �O���t�B�b�N�X�N���X�o�R�Ńf�o�C�X���擾
    auto device = Graphics::Instance().GetDevice();

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(EffectMaterialConstants);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&desc, nullptr, constantBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));
}

// --------------------------------------------------------
// �`��K�p (Apply)
// --------------------------------------------------------
void EffectMaterial::Apply(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
    if (!dc) return;

    // 1. �萔�o�b�t�@�̍X�V (CPU -> GPU)
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(dc->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &constants, sizeof(constants));
        dc->Unmap(constantBuffer.Get(), 0);
    }

    // 2. �萔�o�b�t�@���Z�b�g (b1)
    // ���C��: PS�����łȂ�VS�ɂ��Z�b�g���� (���_�ό`��UV�X�N���[���Ŏg���\�������邽��)
    dc->VSSetConstantBuffers(1, 1, constantBuffer.GetAddressOf());
    dc->PSSetConstantBuffers(1, 1, constantBuffer.GetAddressOf());

    // 3. �e�N�X�`���z��̃Z�b�g (t0 ~ t5)
    ID3D11ShaderResourceView* views[TEXTURE_SLOT_COUNT];
    for (int i = 0; i < TEXTURE_SLOT_COUNT; ++i)
    {
        views[i] = textures[i].Get();
    }
    dc->PSSetShaderResources(0, TEXTURE_SLOT_COUNT, views);


    // 4. �u�����h�X�e�[�g�̐ݒ�
    auto renderState = Graphics::Instance().GetRenderState();


    //ID3D11SamplerState* samplers[2] = {
    //    renderState->GetSamplerState(SamplerState::LinearWrap),  // s0: LinearWrap
    //    renderState->GetSamplerState(SamplerState::LinearClamp)  // s1: LinearClamp
    //};
    //dc->PSSetSamplers(0, 2, samplers);




    ID3D11BlendState* bs = nullptr;

    //switch (blendMode)
    //{
    //case EffectBlendMode::Opaque:
    //    bs = renderState->GetBlendState(BlendState::Opaque);
    //    break;
    //case EffectBlendMode::Alpha:
    //    bs = renderState->GetBlendState(BlendState::Transparency);
    //    break;
    //case EffectBlendMode::Additive:
    //    bs = renderState->GetBlendState(BlendState::Additive);
    //    break;
    //case EffectBlendMode::Subtractive:
    //    bs = renderState->GetBlendState(BlendState::Subtraction);
    //    break;
    //}

    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    dc->OMSetBlendState(bs, blendFactor, 0xFFFFFFFF);





}