#include "MaterialPreviewStudio.h"
#include "Graphics.h"
#include "System/ResourceManager.h"
#include "Material/MaterialAsset.h"
#include "Model/Model.h"
#include "FrameBuffer.h"

// ECS�Ή�
#include "Registry/Registry.h"
#include "Component/CameraComponent.h"
#include "RenderContext/RenderPipeline.h"
#include "RenderContext/RenderQueue.h"

// �� �����̃f�B�t�@�[�h�p�X�����̂܂܎g�p�I
#include "RenderPass/GBufferPass.h"
#include "RenderPass/DeferredLightingPass.h"
#include "RHI/DX11/DX11Texture.h"

using namespace Microsoft::WRL;
using namespace DirectX;

MaterialPreviewStudio& MaterialPreviewStudio::Instance() {
    static MaterialPreviewStudio instance;
    return instance;
}

void MaterialPreviewStudio::Initialize(ID3D11Device* device) {
    if (m_initialized) return;

    m_previewRegistry = std::make_unique<Registry>();
    m_previewCamera = m_previewRegistry->CreateEntity();
    m_previewRegistry->AddComponent(m_previewCamera, CameraLensComponent{});
    m_previewRegistry->AddComponent(m_previewCamera, CameraMatricesComponent{});
    m_previewRegistry->AddComponent(m_previewCamera, CameraMainTagComponent{});

    // =========================================================
    // �� ���ɂ̐���: �����̃p�X�����̂܂܃p�C�v���C���ɐςށI
    // =========================================================
    m_previewPipeline = std::make_unique<RenderPipeline>();
    m_previewPipeline->AddPass(std::make_shared<GBufferPass>());
    m_previewPipeline->AddPass(std::make_shared<DeferredLightingPass>(Graphics::Instance().GetResourceFactory()));

    m_sphereModel = ResourceManager::Instance().GetModel("Data/Model/sphere/fbx_sphere_001.fbx");

    m_initialized = true;
}

bool MaterialPreviewStudio::IsReady() const {
    return m_initialized && m_sphereModel != nullptr;
}

void MaterialPreviewStudio::RenderPreview(MaterialAsset* material, float scaleMult, float rotY) {
    if (!IsReady() || !material) return;

    Graphics& g = Graphics::Instance();

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    m_sphereModel->UpdateTransform(identity);

    // =========================================================
    // �}�e���A���̔��f
    // =========================================================
    auto& materials = m_sphereModel->GetMaterialss();
    for (auto& mat : materials) {
        mat.color = material->baseColor;
        mat.metallicFactor = material->metallic;
        mat.roughnessFactor = material->roughness;

        mat.diffuseTextureFileName = material->diffuseTexturePath;
        mat.normalTextureFileName = material->normalTexturePath;
        mat.metallicTextureFileName = material->metallicRoughnessTexturePath;
        mat.roughnessTextureFileName = material->metallicRoughnessTexturePath;
        mat.emissiveTextureFileName = material->emissiveTexturePath;

        // �� �C���FModel::Material �� ITexture �����ꂽ���߁A���ڑ�����\�ɂȂ�܂����B
        // ResourceManager::GetTexture() �� std::shared_ptr<ITexture> ��Ԃ�����
        // �ʓ|�ȃL���X�g�� Reset() �͈�ؕs�v�ł��B

        mat.diffuseMap = ResourceManager::Instance().GetTexture(mat.diffuseTextureFileName);
        mat.normalMap = ResourceManager::Instance().GetTexture(mat.normalTextureFileName);

        if (!mat.metallicTextureFileName.empty()) {
            mat.metallicMap = ResourceManager::Instance().GetTexture(mat.metallicTextureFileName);
            mat.roughnessMap = mat.metallicMap; // �����x�Ƒe���͓����e�N�X�`���i�̕ʃ`�����l���j���Q�Ƃ��邱�Ƃ���������
        }
        else {
            mat.metallicMap = nullptr;
            mat.roughnessMap = nullptr;
        }

        mat.emissiveMap = ResourceManager::Instance().GetTexture(mat.emissiveTextureFileName);
    }


    // =========================================================
    // �J�����ݒ�
    // =========================================================
    BoundingBox aabb = m_sphereModel->GetWorldBounds();
    XMFLOAT3 center = aabb.Center;
    XMFLOAT3 ex = aabb.Extents;

    XMVECTOR extentsVec = XMLoadFloat3(&ex);
    float radius = XMVectorGetX(XMVector3Length(extentsVec));
    if (radius < 0.01f) radius = 1.0f;

    float fov = XMConvertToRadians(45.0f);
    float distance = (radius / sinf(fov * 0.5f)) * 1.3f * (1.0f / scaleMult);
    float pitch = XMConvertToRadians(20.0f);
    float yaw = XMConvertToRadians(rotY);

    XMFLOAT3 camPos = {
        center.x + distance * cosf(pitch) * sinf(yaw),
        center.y + distance * sinf(pitch),
        center.z - distance * cosf(pitch) * cosf(yaw)
    };

    auto* lens = m_previewRegistry->GetComponent<CameraLensComponent>(m_previewCamera);
    auto* mats = m_previewRegistry->GetComponent<CameraMatricesComponent>(m_previewCamera);

    lens->fovY = fov;
    // �� �`����Scene�o�b�t�@�Ɠ����A�X�y�N�g��ɐݒ肵�Ęc�݂�h��
    lens->aspect = g.GetScreenWidth() / g.GetScreenHeight();
    lens->nearZ = 0.01f;
    lens->farZ = distance * 10.0f;

    XMVECTOR eye = XMLoadFloat3(&camPos);
    XMVECTOR at = XMLoadFloat3(&center);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMStoreFloat4x4(&mats->view, XMMatrixLookAtLH(eye, at, up));
    XMStoreFloat4x4(&mats->projection, XMMatrixPerspectiveFovLH(lens->fovY, lens->aspect, lens->nearZ, lens->farZ));

    mats->worldPos = camPos;
    XMStoreFloat3(&mats->cameraFront, XMVector3Normalize(at - eye));

    // =========================================================
    // �`�[�̍쐬
    // =========================================================
    RenderQueue queue;
    RenderPacket packet;
    packet.model = m_sphereModel.get();

    XMMATRIX S = XMMatrixScaling(scaleMult, scaleMult, scaleMult);
    XMMATRIX R = XMMatrixRotationY(XMConvertToRadians(rotY));
    XMStoreFloat4x4(&packet.worldMatrix, S * R);
    packet.prevWorldMatrix = packet.worldMatrix;

    // �C���X�y�N�^�[��ShaderId�����̂܂ܓn�� (PBR�Ȃ�1)
    packet.shaderId = material->shaderId;
    packet.baseColor = material->baseColor;
    packet.metallic = material->metallic;
    packet.roughness = material->roughness;
    packet.emissive = material->emissive;
    packet.castShadow = false;
    packet.rasterizerState = RasterizerState::SolidCullNone;

    queue.opaquePackets.push_back(packet);

    // =========================================================
    // �� �S�Ă������̃p�C�v���C���Ɉς˂�I
    // =========================================================
    RenderContext rc = m_previewPipeline->BeginFrame(*m_previewRegistry, nullptr);

    rc.directionalLight.direction = { -0.577f, -0.577f, 0.577f };
    rc.directionalLight.color = { 3.0f, 3.0f, 3.0f };
    rc.environment.diffuseIBLPath = "";
    rc.environment.specularIBLPath = "";

    // ������GBufferPass��DeferredLightingPass�����ɑ���A���C����Scene�o�b�t�@�ɕ`�悳���
    m_previewPipeline->Execute(queue, rc);
    m_previewPipeline->EndFrame(rc);
}

ID3D11ShaderResourceView* MaterialPreviewStudio::GetPreviewSRV() const {
    // �� �`�挋�ʂ��Ă��t�������C����Scene�o�b�t�@���A����ImGui�ɓn���I
    return Graphics::Instance().GetFrameBuffer(FrameBufferId::Scene)->GetColorMap(0);
}