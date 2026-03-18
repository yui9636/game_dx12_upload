//#pragma once
//
//#include <memory>
//#include <wrl.h>
//#include <DirectXMath.h>
//#include <vector>
//#include <array>
//#include "Model/Model.h"
//
//// �O���錾
//struct RenderContext;
//class Actor;
//
//class ShadowMap
//{
//public:
//    // �J�X�P�[�h�� (�ߌi�E���i�E���i��3�i)
//    static const int CASCADE_COUNT = 3;
//
//    ShadowMap(ID3D11Device* device);
//    ~ShadowMap() = default;
//
//    // --- CSM�p API ---
//
//    // �S�J�X�P�[�h�̍s����ꊇ�v�Z���� (�t���[���J�n����1��Ă�)
//    // �J������FarZ�Ɠ������Ď�����������܂�
//    void UpdateCascades(const RenderContext& rc);
//
//    // ����̃J�X�P�[�h�ւ̕`����J�n����
//    // index: 0=�ߌi, 1=���i, 2=���i
//    void BeginCascade(const RenderContext& rc, int cascadeIndex);
//
//    // �`��I�� (���\�[�X����)
//    void End(const RenderContext& rc);
//
//    // --- �`��֐� ---
//    void DrawSceneImmediate(const RenderContext& rc, const std::vector<std::shared_ptr<Actor>>& actors);
//    void Draw(const RenderContext& rc, const ModelResource* modelResource, const DirectX::XMFLOAT4X4& worldMatrix);
//
//    // --- �Q�b�^�[ ---
//
//    // �V�F�[�_�[���\�[�X�r���[ (Texture2DArray �Ƃ��ăo�C���h�����)
//    ID3D11ShaderResourceView* GetShaderResourceView() const { return shaderResourceView.Get(); }
//
//    // PCF�p�T���v��
//    ID3D11SamplerState* GetSamplerState() const { return samplerState.Get(); }
//
//    // �w�肵���J�X�P�[�h�̃��C�g�r���[�v���W�F�N�V�����s��
//    const DirectX::XMFLOAT4X4& GetLightViewProjection(int index) const { return shadowMatrices[index]; }
//
//    // �J�X�P�[�h�̕������� (PBRPS�Ŏg��)
//    float GetCascadeEnd(int index) const { return cascadeEndClips[index]; }
//
//    // �e�N�Z���T�C�Y
//    float GetTexelSize() const { return 1.0f / static_cast<float>(textureSize); }
//
//private:
//    // ������̃R�[�i�[���v�Z����w���p�[
//    std::array<DirectX::XMVECTOR, 8> GetFrustumCorners(float fov, float aspect, float nearZ, float farZ, const DirectX::XMFLOAT4X4& viewMat);
//
//    // 1�̃J�X�P�[�h�p�̍s����v�Z����w���p�[
//    DirectX::XMMATRIX CalcCascadeMatrix(const RenderContext& rc, float nearZ, float farZ);
//
//private:
//    // �萔�o�b�t�@�F�V�[�����i�s��X�V�p�j
//    struct CbScene
//    {
//        DirectX::XMFLOAT4X4 lightViewProjection;
//    };
//
//    // �萔�o�b�t�@�F�{�[�����
//    struct CbSkeleton
//    {
//        DirectX::XMFLOAT4X4 boneTransforms[256];
//    };
//
//    const UINT textureSize = 4096; // 4K�𑜓x
//
//    // �r���[�|�[�g�ޔ�p
//    D3D11_VIEWPORT cachedViewport;
//    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cachedRenderTargetView;
//    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cachedDepthStencilView;
//
//    // ���\�[�X
//    // Texture2DArray (3����)
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
//    // �e�X���C�X���Ƃ�DSV
//    std::vector<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>> depthStencilViews;
//
//    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
//
//    // �V�F�[�_�[�֘A
//    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
//    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
//    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
//    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer;
//
//    // --- CSM �f�[�^ ---
//    // �v�Z�ς݂̍s�� (0:��, 1:��, 2:��)
//    std::array<DirectX::XMFLOAT4X4, CASCADE_COUNT> shadowMatrices;
//    // ��������
//    std::array<float, CASCADE_COUNT> cascadeEndClips;
//
//    // �J�X�P�[�h�̕����䗦 (�J����FarZ�ɑ΂��銄��)
//    // 0.1% (��), 1.0% (��), 10.0% (��)
//    // 100km�̏ꍇ: 100m, 1km, 10km �ƂȂ�
//    const std::array<float, CASCADE_COUNT> cascadeSplits = { 0.05f, 0.2f, 1.0f };
//
//};
#pragma once

#include <memory>
#include <DirectXMath.h>
#include <vector>
#include <array>
#include "Model/Model.h"
#include "RHI/ITexture.h" // �� �ǉ�
#include "RHI/ICommandList.h"

class IShader;
class IBuffer;
class ISampler;
class IInputLayout;
class IResourceFactory;
struct RenderContext;
class Actor;
class IPipelineState;

class ShadowMap
{
public:
    static const int CASCADE_COUNT = 3;

    ShadowMap(IResourceFactory* factory);
    ~ShadowMap();

    void UpdateCascades(const RenderContext& rc);
    void BeginCascade(const RenderContext& rc, int cascadeIndex);
    void End(const RenderContext& rc);

    void DrawSceneImmediate(const RenderContext& rc, const std::vector<std::shared_ptr<Actor>>& actors);
    void Draw(const RenderContext& rc, const ModelResource* modelResource, const DirectX::XMFLOAT4X4& worldMatrix);

    // ==========================================
    // �� �C���F���� RHI �����ꂽ ITexture ��Ԃ��悤�ɁI
    // ==========================================
    ITexture* GetTexture() const { return m_shadowTexture.get(); }

    ISampler* GetSamplerState() const { return samplerState.get(); }
    const DirectX::XMFLOAT4X4& GetLightViewProjection(int index) const { return shadowMatrices[index]; }
    float GetCascadeEnd(int index) const { return cascadeEndClips[index]; }
    float GetTexelSize() const { return 1.0f / static_cast<float>(textureSize); }

private:
    std::array<DirectX::XMVECTOR, 8> GetFrustumCorners(float fov, float aspect, float nearZ, float farZ, const DirectX::XMFLOAT4X4& viewMat);
    DirectX::XMMATRIX CalcCascadeMatrix(const RenderContext& rc, float nearZ, float farZ);

private:
    struct CbScene { DirectX::XMFLOAT4X4 lightViewProjection; };
    struct CbSkeleton { DirectX::XMFLOAT4X4 boneTransforms[256]; };

    const UINT textureSize = 4096;

    RhiViewport m_cachedViewport;
    ITexture* m_cachedRT = nullptr;
    ITexture* m_cachedDS = nullptr;

    std::unique_ptr<IPipelineState> m_pso;

    std::shared_ptr<ITexture> m_shadowTexture;
    std::vector<std::shared_ptr<ITexture>> m_cascadeTextures;

    std::unique_ptr<ISampler>       samplerState;
    std::unique_ptr<IShader>        vertexShader;
    std::unique_ptr<IInputLayout>   inputLayout;
    std::unique_ptr<IBuffer>        sceneConstantBuffer;
    std::unique_ptr<IBuffer>        skeletonConstantBuffer;




    std::array<DirectX::XMFLOAT4X4, CASCADE_COUNT> shadowMatrices;
    std::array<float, CASCADE_COUNT> cascadeEndClips;
    const std::array<float, CASCADE_COUNT> cascadeSplits = { 0.05f, 0.2f, 1.0f };
};