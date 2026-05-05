#include "LightSystem.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"

using namespace DirectX;

// シーン内のライトコンポーネントを描画用データへ変換します。
// Point は RenderContext::pointLights に追加し、Directional は RenderContext::directionalLight に設定します。
void LightSystem::ExtractLights(Registry& registry, RenderContext& rc)
{
    // ライト情報とワールド変換を両方持つエンティティだけを対象にします。
    Query<LightComponent, TransformComponent> query(registry);

    // 各ライトを種類ごとに RenderContext へ詰め替えます。
    query.ForEach([&](const LightComponent& light, const TransformComponent& trans) {
        if (light.type == LightType::Point) {
            // 点光源は位置・色・強さ・範囲をそのまま描画用構造体へ変換します。
            PointLight data;
            data.position = trans.worldPosition;
            data.color = light.color;
            data.intensity = light.intensity;
            data.range = light.range;

            rc.pointLights.push_back(data);
        }
        else if (light.type == LightType::Directional) {
            // 平行光源は Transform の回転からライト方向を作ります。
            DirectionalLight data;

            // ローカル +Z 方向をライトの基準向きとして扱います。
            XMVECTOR forward = XMVectorSet(0, 0, 1, 0);

            // ワールド回転を読み込み、基準方向を回転させます。
            XMVECTOR rotQuat = XMLoadFloat4(&trans.worldRotation);
            XMVECTOR dir = XMVector3Rotate(forward, rotQuat);

            // シェーダ側で扱いやすいよう、方向ベクトルを正規化して保存します。
            XMStoreFloat3(&data.direction, XMVector3Normalize(dir));

            // 平行光源は色に強度を掛けた値を RenderContext に渡します。
            data.color.x = light.color.x * light.intensity;
            data.color.y = light.color.y * light.intensity;
            data.color.z = light.color.z * light.intensity;

            rc.directionalLight = data;
        }
        });
}
