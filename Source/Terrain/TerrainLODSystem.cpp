#include "TerrainLODSystem.h"
#include "TerrainBuildSystem.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include "Component/CameraComponent.h"

void TerrainLODSystem::Update(Registry& registry, TerrainBuildSystem& buildSystem)
{
    DirectX::XMFLOAT3 cameraPos = { 0.0f, 0.0f, 0.0f };
    bool foundCamera = false;

    Query<CameraMainTagComponent, TransformComponent> mainCamQ(registry);
    mainCamQ.ForEach([&](const CameraMainTagComponent&, const TransformComponent& transform) {
        cameraPos = transform.worldPosition;
        foundCamera = true;
    });

    if (!foundCamera) {
        Query<CameraLensComponent, TransformComponent> camQ(registry);
        camQ.ForEach([&](const CameraLensComponent&, const TransformComponent& transform) {
            if (!foundCamera) {
                cameraPos = transform.worldPosition;
                foundCamera = true;
            }
        });
    }

    buildSystem.UpdateLod(registry, cameraPos, lodDistances);
}
