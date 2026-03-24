#include "EnvironmentExtractSystem.h"
#include "Component/EnvironmentComponent.h"
#include "System/Query.h"

void EnvironmentExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<EnvironmentComponent> query(registry);

    query.ForEach([&](EnvironmentComponent& env) {
        rc.environment.diffuseIBLPath = env.diffuseIBLPath;
        rc.environment.specularIBLPath = env.specularIBLPath;

        if (env.enableSkybox) {
            rc.environment.skyboxPath = env.skyboxPath;
        }
        else {
            rc.environment.skyboxPath = "";
        }
        });
}
