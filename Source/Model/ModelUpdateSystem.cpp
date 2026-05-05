// ECS 上の MeshComponent に含まれる Model の更新を行うシステム実装です。
#include "ModelUpdateSystem.h"
#include "Model.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "System/Query.h"
#include <DirectXMath.h>

// Registry 内の Model を毎フレーム更新し、Transform 情報を反映します。
void ModelUpdateSystem::Update(Registry& registry)
{
    Query<MeshComponent, TransformComponent> query(registry);
    query.ForEach([](MeshComponent& mesh, const TransformComponent&) {
        if (mesh.model) {
            DirectX::XMFLOAT4X4 identity{};
            DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
            mesh.model->UpdateTransform(identity);
        }
    });
}
