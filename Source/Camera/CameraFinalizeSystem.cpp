#include "CameraFinalizeSystem.h"
#include "Registry/Registry.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/TransformComponent.h"
#include "Component/CameraComponent.h"

using namespace DirectX;

void CameraFinalizeSystem::Update(Registry& registry) {
    auto archetypes = registry.GetAllArchetypes();
    Signature targetSig = CreateSignature<TransformComponent, CameraLensComponent, CameraMatricesComponent>();

    for (auto* archetype : archetypes) {
        if (!SignatureMatches(archetype->GetSignature(), targetSig)) continue;

        auto* transCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* lensCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraLensComponent>());
        auto* matsCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraMatricesComponent>());

        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            auto& trans = *static_cast<TransformComponent*>(transCol->Get(i));
            auto& lens = *static_cast<CameraLensComponent*>(lensCol->Get(i));
            auto& mats = *static_cast<CameraMatricesComponent*>(matsCol->Get(i));

            // ★ 修正：TransformSystemが計算した「worldMatrix」を直接使う！
            XMMATRIX W = XMLoadFloat4x4(&trans.worldMatrix);

            // 行列から直接、ワールド座標・前方向・上方向を抜き出す（これが一番ズレない）
            XMVECTOR eye = W.r[3];        // 4行目が座標
            XMVECTOR forward = W.r[2];    // 3行目が前方向 (Z軸)
            XMVECTOR up = W.r[1];         // 2行目が上方向 (Y軸)

            // View行列の作成 (LookToを使うのがミソ)
            XMMATRIX view = XMMatrixLookToLH(eye, forward, up);
            XMStoreFloat4x4(&mats.view, view);

            // Projection行列
            XMMATRIX proj = XMMatrixPerspectiveFovLH(lens.fovY, lens.aspect, lens.nearZ, lens.farZ);
            XMStoreFloat4x4(&mats.projection, proj);

            // パイプライン用データの更新
            XMStoreFloat3(&mats.worldPos, eye);
            XMStoreFloat3(&mats.cameraFront, forward);
        }
    }
}