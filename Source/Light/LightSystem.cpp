#include "LightSystem.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"

using namespace DirectX;

void LightSystem::ExtractLights(Registry& registry, RenderContext& rc)
{
    Query<LightComponent, TransformComponent> query(registry);

    query.ForEach([&](const LightComponent& light, const TransformComponent& trans) {
        if (light.type == LightType::Point) {
            PointLight data;
            data.position = trans.worldPosition;
            data.color = light.color;
            data.intensity = light.intensity;
            data.range = light.range;

            rc.pointLights.push_back(data);
        }
        else if (light.type == LightType::Directional) {
            DirectionalLight data;

            XMVECTOR forward = XMVectorSet(0, 0, 1, 0);
            XMVECTOR rotQuat = XMLoadFloat4(&trans.worldRotation);
            XMVECTOR dir = XMVector3Rotate(forward, rotQuat);
            XMStoreFloat3(&data.direction, XMVector3Normalize(dir));

            data.color.x = light.color.x * light.intensity;
            data.color.y = light.color.y * light.intensity;
            data.color.z = light.color.z * light.intensity;

            rc.directionalLight = data;
        }
        });
}
