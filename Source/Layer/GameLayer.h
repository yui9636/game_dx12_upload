#pragma once
#include "Layer.h"
#include "Registry/Registry.h"
#include <Component\EnvironmentComponent.h>
#include <Component\PostEffectComponent.h>

class GameLayer : public Layer
{
public:
    GameLayer() = default;
    ~GameLayer() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update(const EngineTime& time) override;
    void Render(RenderContext& rc, RenderQueue& queue) override;

    // EditorLayerに中身を覗かせるためのアクセス権
    Registry& GetRegistry() { return m_registry; }

    EnvironmentComponent& GetEnvironment() { return m_environment; }
    PostEffectComponent& GetPostEffect() { return m_postEffect; }
private:
    Registry m_registry;

    EnvironmentComponent m_environment;
    PostEffectComponent m_postEffect;
};