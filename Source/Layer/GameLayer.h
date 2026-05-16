#pragma once
#include "Layer.h"
#include "Registry/Registry.h"
#include <Component\PostEffectComponent.h>
#include <memory>
#include "Terrain/TerrainBuildSystem.h"
#include "Terrain/TerrainLODSystem.h"
#include "Terrain/TerrainPhysicsSystem.h"
#include "Vegetation/GrassBuildSystem.h"
// HeadUpDisplay はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class HeadUpDisplay;

class GameLayer : public Layer
{
public:
    GameLayer() = default;
    ~GameLayer() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update(const EngineTime& time) override;
    void Render(RenderContext& rc, RenderQueue& queue) override;

    Registry& GetRegistry() { return m_registry; }

    PostEffectComponent& GetPostEffect() { return m_postEffect; }
private:
    void InitializeHUD();
    void ShutdownHUD();

    Registry m_registry;
    PostEffectComponent m_postEffect;

    std::shared_ptr<HeadUpDisplay> m_headUpDisplay;

    TerrainBuildSystem   m_terrainBuildSystem;
    TerrainLODSystem     m_terrainLODSystem;
    TerrainPhysicsSystem m_terrainPhysicsSystem;
    GrassBuildSystem     m_grassBuildSystem;
};
