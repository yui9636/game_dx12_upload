#include "PlayerModelPreviewStudio.h"

#include "Render/OffscreenRenderer.h"
#include "Render/Graphics.h"
#include "Render/Gizmos.h"
#include "Model/Model.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "RenderContext/RenderContext.h"
#include "Console/Logger.h"
#include "System/ResourceManager.h"
#include "Registry/Registry.h"
#include "Entity/Entity.h"
#include "Collision/CollisionManager.h"
#include "Component/ColliderComponent.h"
#include "Component/TransformComponent.h"
#include "Component/MeshComponent.h"
#include "Transform/NodeAttachmentUtils.h"

using namespace DirectX;
// PlayerModelPreviewStudio::Instance はこのモジュールの実行時処理を構成する補助処理を行う。

namespace
{
    constexpr const char* kTrainingStagePath = "Data/Model/TrainingStage/TrainingStage.gltf";
    constexpr float kTrainingStageScale = 2.0f;
    constexpr float kTrainingStagePositionY = 1.0f;

    DirectX::XMFLOAT4X4 BuildTrainingStageWorld()
    {
        using namespace DirectX;

        XMFLOAT4X4 world{};
        XMStoreFloat4x4(
            &world,
            XMMatrixScaling(kTrainingStageScale, kTrainingStageScale, kTrainingStageScale) *
            XMMatrixTranslation(0.0f, kTrainingStagePositionY, 0.0f));
        return world;
    }

    DirectX::XMFLOAT4X4 BuildViewProjection(
        const DirectX::XMFLOAT3& eye,
        const DirectX::XMFLOAT3& target,
        float fovY,
        float aspect,
        float nearZ,
        float farZ)
    {
        using namespace DirectX;
        XMFLOAT4X4 viewProj{};
        XMStoreFloat4x4(
            &viewProj,
            XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) *
            XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
        return viewProj;
    }

    // プレビューエンティティのコライダ要素を Gizmos キューへ enqueue する。
    // DebugRenderSystem::Render と同じ経路 (registeredId 経由で CollisionManager
    // のランタイムコライダーを参照する、無ければ authoring データへフォールバック)。
    void EnqueuePreviewColliderGizmos(
        Gizmos* gizmos,
        Registry& registry,
        EntityID previewEntity)
    {
        using namespace DirectX;

        if (!gizmos) return;
        if (Entity::IsNull(previewEntity) || !registry.IsAlive(previewEntity)) return;

        auto* collider = registry.GetComponent<ColliderComponent>(previewEntity);
        auto* transform = registry.GetComponent<TransformComponent>(previewEntity);
        if (!collider || !transform || !collider->enabled) return;

        auto& cm = CollisionManager::Instance();

        for (auto& e : collider->elements) {
            if (!e.enabled) continue;
            const XMFLOAT4 color{ e.color.x, e.color.y, e.color.z, e.color.w };

            // 既登録ランタイムコライダがあればそれを使う (本物のコリジョン形状)
            if (e.registeredId != 0) {
                const Collider* rt = cm.Get(e.registeredId);
                if (rt && rt->enabled) {
                    if (rt->shape == ColliderShape::Sphere) {
                        gizmos->DrawSphere(rt->sphere.center, rt->sphere.radius, color);
                        continue;
                    }
                    if (rt->shape == ColliderShape::Box) {
                        gizmos->DrawBox(rt->box.center, { 0,0,0 }, rt->box.size, color);
                        continue;
                    }
                    if (rt->shape == ColliderShape::Capsule) {
                        gizmos->DrawCapsule(rt->capsule.base, { 0,0,0 },
                                            rt->capsule.radius, rt->capsule.height, color);
                        continue;
                    }
                }
            }

            // 未登録 / 取得不能時のフォールバック (authoring データ + bone)
            XMVECTOR vWorldPos;
            XMMATRIX matWorld = XMLoadFloat4x4(&transform->worldMatrix);
            if (e.nodeIndex >= 0) {
                MeshComponent* mesh = registry.GetComponent<MeshComponent>(previewEntity);
                if (mesh && mesh->model) {
                    XMFLOAT3 offset{ e.offsetLocal.x, e.offsetLocal.y, e.offsetLocal.z };
                    XMFLOAT3 posModelSpace = NodeAttachmentUtils::GetWorldPositionNodeLocal(
                        mesh->model.get(), e.nodeIndex, offset);
                    vWorldPos = XMVector3TransformCoord(XMLoadFloat3(&posModelSpace), matWorld);
                } else {
                    XMFLOAT3 offset{ e.offsetLocal.x, e.offsetLocal.y, e.offsetLocal.z };
                    vWorldPos = XMVector3TransformCoord(XMLoadFloat3(&offset), matWorld);
                }
            } else {
                XMFLOAT3 offset{ e.offsetLocal.x, e.offsetLocal.y, e.offsetLocal.z };
                vWorldPos = XMVector3TransformCoord(XMLoadFloat3(&offset), matWorld);
            }
            XMFLOAT3 worldPos;
            XMStoreFloat3(&worldPos, vWorldPos);

            const float sx = std::fabs(transform->worldScale.x);
            const float sy = std::fabs(transform->worldScale.y);
            const float sz = std::fabs(transform->worldScale.z);
            float maxScale = sx; if (sy > maxScale) maxScale = sy; if (sz > maxScale) maxScale = sz;
            if (maxScale <= 0.0001f) maxScale = 1.0f;

            if (e.type == ColliderShape::Sphere) {
                gizmos->DrawSphere(worldPos, e.radius * maxScale, color);
            } else if (e.type == ColliderShape::Capsule) {
                gizmos->DrawCapsule(worldPos, { 0,0,0 }, e.radius * maxScale, e.height * sy, color);
            } else if (e.type == ColliderShape::Box) {
                XMFLOAT3 size{ e.size.x * sx, e.size.y * sy, e.size.z * sz };
                gizmos->DrawBox(worldPos, { 0,0,0 }, size, color);
            }
        }
    }

}

PlayerModelPreviewStudio& PlayerModelPreviewStudio::Instance()
{
    static PlayerModelPreviewStudio instance;
    return instance;
}

void PlayerModelPreviewStudio::Initialize(OffscreenRenderer* offscreen)
{
    m_offscreen = offscreen;
    m_previewTexture.reset();
    m_previewDepth.reset();
    m_trainingStageModel.reset();
    m_trainingStageLoadAttempted = false;

    if (!m_offscreen || !m_offscreen->IsReady()) {
        LOG_ERROR("[PlayerModelPreviewStudio] OffscreenRenderer unavailable.");
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        return;
    }

    TextureDesc colorDesc{};
    colorDesc.width = PREVIEW_SIZE;
    colorDesc.height = PREVIEW_SIZE;
    colorDesc.format = TextureFormat::RGBA8_UNORM;
    colorDesc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
    colorDesc.clearColor[0] = 0.12f;
    colorDesc.clearColor[1] = 0.12f;
    colorDesc.clearColor[2] = 0.12f;
    colorDesc.clearColor[3] = 1.0f;
    m_previewTexture = std::shared_ptr<ITexture>(factory->CreateTexture("PlayerModelPreview", colorDesc));

    TextureDesc depthDesc{};
    depthDesc.width = PREVIEW_SIZE;
    depthDesc.height = PREVIEW_SIZE;
    depthDesc.format = TextureFormat::D24_UNORM_S8_UINT;
    depthDesc.bindFlags = TextureBindFlags::DepthStencil;
    depthDesc.clearDepth = 1.0f;
    m_previewDepth = factory->CreateTexture("PlayerModelPreviewDepth", depthDesc);

    if (!m_previewTexture || !m_previewDepth) {
        LOG_ERROR("[PlayerModelPreviewStudio] Failed to create preview textures.");
        m_previewTexture.reset();
        m_previewDepth.reset();
        return;
    }

    LOG_INFO("[PlayerModelPreviewStudio] Initialized.");
}

bool PlayerModelPreviewStudio::IsReady() const
{
    return m_offscreen && m_offscreen->IsReady() && m_previewTexture && m_previewDepth;
}

Model* PlayerModelPreviewStudio::EnsureTrainingStageModel()
{
    if (m_trainingStageModel) {
        return m_trainingStageModel.get();
    }
    if (m_trainingStageLoadAttempted) {
        return nullptr;
    }

    m_trainingStageLoadAttempted = true;
    m_trainingStageModel = ResourceManager::Instance().CreateModelInstance(kTrainingStagePath);
    if (!m_trainingStageModel) {
        LOG_WARN("[PlayerModelPreviewStudio] Training stage load failed: %s", kTrainingStagePath);
        return nullptr;
    }

    LOG_INFO("[PlayerModelPreviewStudio] Training stage loaded: %s", kTrainingStagePath);
    return m_trainingStageModel.get();
}

void PlayerModelPreviewStudio::RenderPreview(
    const Model* model,
    const DirectX::XMFLOAT3& cameraPosition,
    const DirectX::XMFLOAT3& cameraTarget,
    float aspect,
    float fovY,
    float nearZ,
    float farZ,
    const DirectX::XMFLOAT4& clearColor,
    float previewScale,
    Registry* registry,
    uint64_t previewEntity)
{
    if (!IsReady()) {
        return;
    }
    if (!m_offscreen->IsGpuIdle()) {
        return;
    }

    // ステージモデルは廃止 (背景は単色グレーのみ)
    auto modelResource = model ? model->GetModelResource() : nullptr;
    if (!modelResource) {
        // モデル無しでもクリアして RT は更新する
    }

    XMFLOAT4X4 identity{};
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    if (model) {
        const_cast<Model*>(model)->UpdateTransform(identity);
    }
    XMFLOAT4X4 scaledWorld{};
    XMStoreFloat4x4(&scaledWorld, XMMatrixScaling(
        (std::max)(previewScale, 0.01f),
        (std::max)(previewScale, 0.01f),
        (std::max)(previewScale, 0.01f)));

    const float safeAspect = aspect > 0.01f ? aspect : 1.0f;
    const float safeNearZ = nearZ > 0.0001f ? nearZ : 0.03f;
    const float safeFarZ = farZ > safeNearZ ? farZ : (safeNearZ + 500.0f);
    const float safeFovY = fovY > 0.01f ? fovY : 0.785398f;

    const XMFLOAT4X4 playerViewProj = BuildViewProjection(
        cameraPosition,
        cameraTarget,
        safeFovY,
        safeAspect,
        safeNearZ,
        safeFarZ);
    const XMFLOAT3 lightDir = { -0.5f, -0.7f, 0.5f };
    const XMFLOAT3 lightColor = { 3.0f, 3.0f, 3.0f };
    const XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    m_offscreen->BeginJob();
    m_offscreen->ClearExternalRT(
        m_previewTexture.get(),
        m_previewDepth.get(),
        clearColor.x,
        clearColor.y,
        clearColor.z,
        clearColor.w);
    m_offscreen->SetExternalRenderTarget(m_previewTexture.get(), m_previewDepth.get());
    m_offscreen->SetViewport(static_cast<float>(PREVIEW_SIZE), static_cast<float>(PREVIEW_SIZE));
    m_offscreen->BindSampler();

    if (modelResource) {
        m_offscreen->UploadScene(
            playerViewProj,
            cameraPosition,
            lightDir,
            lightColor,
            static_cast<float>(PREVIEW_SIZE),
            static_cast<float>(PREVIEW_SIZE));
        m_offscreen->BindScene();
        m_offscreen->GetModelRenderer().Draw(
            ShaderId::PBR,
            modelResource,
            scaledWorld,
            scaledWorld,
            white,
            0.0f,
            1.0f,
            0.0f,
            nullptr,
            BlendState::Opaque,
            DepthState::TestAndWrite,
            RasterizerState::SolidCullNone);
        m_offscreen->RenderQueuedDirect(m_previewTexture.get(), m_previewDepth.get());
    }

    // コライダ Gizmo を本物の Gizmos 経路で描画 (3D ワイヤメッシュを RT に焼き込む)。
    // メインシーンと同じ手順: RT/Depth barrier + 再バインド + viewport 設定 → render。
    if (registry && previewEntity != Entity::NULL_ID) {
        Gizmos* gizmos = Graphics::Instance().GetGizmos();
        RenderState* renderState = Graphics::Instance().GetRenderState();
        ICommandList* cmdList = m_offscreen->GetCommandList();
        if (gizmos && renderState && cmdList) {
            EnqueuePreviewColliderGizmos(gizmos, *registry, previewEntity);

            // depth を DepthRead に遷移し、RT を明示再バインド (gizmos は depth read-only test)
            cmdList->TransitionBarrier(m_previewTexture.get(), ResourceState::RenderTarget);
            cmdList->TransitionBarrier(m_previewDepth.get(), ResourceState::DepthRead);
            ITexture* rts[] = { m_previewTexture.get() };
            cmdList->SetRenderTargets(1, rts, m_previewDepth.get());
            cmdList->SetViewport(RhiViewport(0.0f, 0.0f,
                static_cast<float>(PREVIEW_SIZE), static_cast<float>(PREVIEW_SIZE)));

            RenderContext gizmoRc{};
            gizmoRc.commandList = cmdList;
            gizmoRc.renderState = renderState;
            XMStoreFloat4x4(&gizmoRc.viewMatrix,
                XMMatrixLookAtLH(XMLoadFloat3(&cameraPosition),
                                 XMLoadFloat3(&cameraTarget),
                                 XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
            XMStoreFloat4x4(&gizmoRc.projectionMatrix,
                XMMatrixPerspectiveFovLH(safeFovY, safeAspect, safeNearZ, safeFarZ));

            gizmos->Render(gizmoRc);
        }
    }

    m_offscreen->FinishDirect(m_previewTexture.get());
}
