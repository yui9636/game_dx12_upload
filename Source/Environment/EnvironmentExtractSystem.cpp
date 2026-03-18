#include "EnvironmentExtractSystem.h"
#include "Component/EnvironmentComponent.h"
#include "System/Query.h"

void EnvironmentExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<EnvironmentComponent> query(registry);

    query.ForEach([&](EnvironmentComponent& env) {
        // IBLのパスをコンテキストに書き込む
        rc.environment.diffuseIBLPath = env.diffuseIBLPath;
        rc.environment.specularIBLPath = env.specularIBLPath;

        // Skyboxのパスを書き込む（無効なら空文字にして描画させないようにする）
        if (env.enableSkybox) {
            rc.environment.skyboxPath = env.skyboxPath;
        }
        else {
            rc.environment.skyboxPath = "";
        }
        });
}