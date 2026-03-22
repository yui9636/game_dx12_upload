#pragma once

//#include "Camera/Camera.h"
#include "RenderState.h"
#include "RenderQueue.h"
#include "Light/Light.h"
#include <string>
#include <functional>
#include <memory>
#include <DirectXMath.h>
#include "RHI/ICommandList.h"
#include "RHI/IBuffer.h"
#include "RHI/ITexture.h"
#include "IndirectDrawCommon.h"

class ShadowMap;
class Skybox;
class ModelResource;


// �u���[���ݒ�
struct BloomData {
    float luminanceLowerEdge = 0.6f;
    float luminanceHigherEdge = 0.8f;
    float bloomIntensity = 1.0f;
    float gaussianSigma = 1.0f;
};

// �J���[�t�B���^�[�ݒ�
struct ColorFilterData {
    float exposure = 1.2f;
    float monoBlend = 0.0f;
    float hueShift = 0.0f;
    float flashAmount = 0.0f;
    float vignetteAmount = 0.0f;
};


// DoF�i��ʊE�[�x�j�ݒ�
struct DepthOfFieldData
{
    bool  enable = false;         // �L��/����
    float focusDistance = 10.0f;  // �s���g���������� (m)
    float focusRange = 5.0f;      // �s���g�������͈� (m)
    float bokehRadius = 4.0f;     // �{�P�̋���
};

struct MotionBlurData
{
    float intensity = 0.0f;
    float samples = 8.0f;
};

struct RenderEnvironment
{
    std::string skyboxPath = "";
    std::string diffuseIBLPath = "";
    std::string specularIBLPath = "";
};

// UV �X�N���[�����
struct UVScrollData
{
    DirectX::XMFLOAT2 uvScrollValue;
};

// �}�X�N�f�[�^
struct MaskData
{
    ID3D11ShaderResourceView* maskTexture;
    float dissolveThreshold;
    float edgeThreshold;
    DirectX::XMFLOAT4 edgeColor;
};

// ���W�A���u���[���
struct RadialBlurData
{
    float radius = 10.0f;
    int samplingCount = 10;
    DirectX::XMFLOAT2 center = { 0.5f, 0.5f };
    float mask_radius = 0;
};

// �K�E�X�t�B���^�[�v�Z���
struct GaussianFilterData {
    int kernelSize = 8;
    float deviation = 10.0f;
    DirectX::XMFLOAT2 textureSize;
};

static const int MaxKernelSize = 16;

struct RenderPipelineSettings
{
    bool enableShadow = true;
    bool enableSkybox = true;
    bool enablePostProcess = true;
    bool enableGrid = true;
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};


struct RenderContext
{
    struct PreparedInstanceBatch
    {
        DrawBatchKey key;
        std::shared_ptr<ModelResource> modelResource;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
    };

    struct PreparedIndirectCommand
    {
        DrawBatchKey key;
        std::shared_ptr<ModelResource> modelResource;
        uint32_t meshIndex = 0;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
        uint32_t argumentOffsetBytes = 0;
        bool supportsInstancing = false;
    };

    struct GpuDrivenCommandMetadata
    {
        uint32_t meshIndex = 0;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
        uint32_t argumentOffsetBytes = 0;
        uint32_t supportsInstancing = 0;
    };

    bool HasPreparedOpaqueCommands() const
    {
        return !activeDrawCommands.empty() || !activeSkinnedCommands.empty();
    }

    //ID3D11DeviceContext* deviceContext;
    ICommandList* commandList;
    const RenderState* renderState;

    RhiViewport mainViewport;
    ITexture* mainRenderTarget = nullptr;
    ITexture* mainDepthStencil = nullptr;

    DirectionalLight        directionalLight;
    std::vector<PointLight> pointLights;

    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 viewProjectionUnjittered;
    DirectX::XMFLOAT4X4 prevViewProjectionMatrix;
    DirectX::XMFLOAT3   cameraPosition;
    DirectX::XMFLOAT3   cameraDirection;

    float fovY;
    float aspect;
    float nearZ;
    float farZ;
    DirectX::XMFLOAT2 jitterOffset = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 prevJitterOffset = { 0.0f, 0.0f };

    // �e�����N���X�ւ̃A�N�Z�X�i�e�`��p�X�Ŏg�p�j
    const ShadowMap* shadowMap = nullptr;

    // �e�̐F�i���ݒ肩�痈��j
    DirectX::XMFLOAT3 shadowColor = { 0.1f, 0.1f, 0.1f };

    ITexture* sceneColorTexture = nullptr;
    ITexture* sceneDepthTexture = nullptr;

    ITexture* reflectionProbeTexture = nullptr;

    ITexture* debugGBuffer0 = nullptr;
    ITexture* debugGBuffer1 = nullptr;
    ITexture* debugGBuffer2 = nullptr;

    RenderEnvironment environment;

    std::vector<InstanceBatch> visibleOpaqueInstanceBatches;
    std::vector<InstanceData> preparedInstanceData;
    std::shared_ptr<IBuffer> preparedInstanceBuffer;
    std::shared_ptr<IBuffer> preparedVisibleInstanceStructuredBuffer;
    uint32_t preparedInstanceStride = 0;
    uint32_t preparedInstanceCapacity = 0;
    uint32_t preparedVisibleInstanceCount = 0;
    std::shared_ptr<IBuffer> preparedIndirectArgumentBuffer;
    std::shared_ptr<IBuffer> preparedIndirectCommandMetadataBuffer;
    uint32_t preparedIndirectArgumentCapacity = 0;
    uint32_t preparedIndirectCommandMetadataCapacity = 0;
    std::vector<PreparedInstanceBatch> preparedOpaqueInstanceBatches;
    std::vector<PreparedIndirectCommand> preparedIndirectCommands;
    std::vector<PreparedIndirectCommand> preparedSkinnedCommands;

    // Active draw state (set by BuildIndirectCommandPass, overridden by ComputeCullingPass)
    IBuffer*  activeInstanceBuffer   = nullptr;   // VB slot1
    uint32_t  activeInstanceStride   = INSTANCE_DATA_STRIDE;
    IBuffer*  activeDrawArgsBuffer   = nullptr;   // ExecuteIndirect args
    std::vector<IndirectDrawCommand> activeDrawCommands;
    std::vector<IndirectDrawCommand> activeSkinnedCommands;

    // GPU culling state (set by ComputeCullingPass)
    bool useGpuCulling = false;
    IBuffer* activeCountBuffer = nullptr;      // count buffer for multi-draw
    uint32_t activeCountBufferOffset = 0;
    uint32_t activeMaxDrawCount = 0;           // max commands for multi-draw

    // �|�X�g�v���Z�X�p�f�[�^
    BloomData       bloomData;      // ���ǉ�
    ColorFilterData colorFilterData; // ���ǉ�
    DepthOfFieldData dofData;       //
    MotionBlurData  motionBlurData;

    // ----------------------------------------------------

    // ���������o
    float time = 0.0f; // �A�j���[�V�����p�^�C�}�[�Ȃ�

    // �ȉ��A�K�v�Ȑݒ�f�[�^��ێ�
    UVScrollData uvScrollData;
    MaskData maskData;
    RadialBlurData radialBlurData;
    GaussianFilterData gaussianFilterData;

   
};
