#pragma once
#include "Layer.h"
#include "Registry/Registry.h"
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

    Registry& GetRegistry() { return m_registry; }

    PostEffectComponent& GetPostEffect() { return m_postEffect; }
private:
    Registry m_registry;
    PostEffectComponent m_postEffect;
};
