#include "ModelUpdateSystem.h"
#include "Model.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "System/Query.h"
#include <DirectXMath.h>

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
