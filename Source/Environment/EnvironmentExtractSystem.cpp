#include "EnvironmentExtractSystem.h"
#include "Component/EnvironmentComponent.h"
#include "System/Query.h"

// EnvironmentComponent の設定を描画用の RenderContext に抽出する。
void EnvironmentExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    // Registry 内に存在する EnvironmentComponent をすべて走査するためのクエリを作成する。
    Query<EnvironmentComponent> query(registry);

    // 見つかった環境設定を RenderContext へ反映する。
    query.ForEach([&](EnvironmentComponent& env) {
        // IBL 用の拡散反射・鏡面反射テクスチャパスを描画コンテキストへ渡す。
        rc.environment.diffuseIBLPath = env.diffuseIBLPath;
        rc.environment.specularIBLPath = env.specularIBLPath;

        // Skybox が有効な場合だけパスを渡し、無効な場合は空文字にして描画を止める。
        if (env.enableSkybox) {
            rc.environment.skyboxPath = env.skyboxPath;
        }
        else {
            rc.environment.skyboxPath = "";
        }
        });
}
