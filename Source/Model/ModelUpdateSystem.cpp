#include "ModelUpdateSystem.h"
#include "Model.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "System/Query.h"

void ModelUpdateSystem::Update(Registry& registry)
{
    Query<MeshComponent, TransformComponent> query(registry);
    query.ForEach([](MeshComponent& mesh, const TransformComponent& transform) {
        if (mesh.model) {
            mesh.model->UpdateTransform(transform.worldMatrix);
        }
    });
}
