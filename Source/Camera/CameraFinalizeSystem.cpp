#include "CameraFinalizeSystem.h"
#include "Registry/Registry.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/TransformComponent.h"
#include "Component/CameraComponent.h"

using namespace DirectX;

// =========================================================
// CameraLensComponent の設定と TransformComponent の姿勢から、
// CameraMatricesComponent にビュー行列・射影行列を反映します。
// =========================================================
void CameraFinalizeSystem::Update(Registry& registry) {
    // 対象となるコンポーネントの組み合わせを持つ Archetype だけを処理します。
    auto archetypes = registry.GetAllArchetypes();
    Signature targetSig = CreateSignature<TransformComponent, CameraLensComponent, CameraMatricesComponent>();

    for (auto* archetype : archetypes) {
        // 必要なコンポーネントを持たない Archetype は処理対象外です。
        if (!SignatureMatches(archetype->GetSignature(), targetSig)) continue;

        // 各コンポーネント列を取得します。
        auto* transCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* lensCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraLensComponent>());
        auto* matsCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraMatricesComponent>());

        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            // 同じ行にある Transform / Lens / Matrices を取り出します。
            auto& trans = *static_cast<TransformComponent*>(transCol->Get(i));
            auto& lens = *static_cast<CameraLensComponent*>(lensCol->Get(i));
            auto& mats = *static_cast<CameraMatricesComponent*>(matsCol->Get(i));

            // Transform の worldMatrix からカメラのワールド姿勢を読み取ります。
            XMMATRIX W = XMLoadFloat4x4(&trans.worldMatrix);

            // 行列の平行移動成分をカメラ位置として扱います。
            XMVECTOR eye = W.r[3];

            // worldMatrix の Z 軸を前方向、Y 軸を上方向として使います。
            XMVECTOR forward = W.r[2];
            XMVECTOR up = W.r[1];

            // カメラ位置と前方向からビュー行列を作ります。
            XMMATRIX view = XMMatrixLookToLH(eye, forward, up);
            XMStoreFloat4x4(&mats.view, view);

            // レンズ設定から射影行列を作ります。
            XMMATRIX proj = XMMatrixPerspectiveFovLH(lens.fovY, lens.aspect, lens.nearZ, lens.farZ);
            XMStoreFloat4x4(&mats.projection, proj);

            // シェーダやデバッグ表示で使えるように、カメラ位置と前方向も保存します。
            XMStoreFloat3(&mats.worldPos, eye);
            XMStoreFloat3(&mats.cameraFront, forward);
        }
    }
}
