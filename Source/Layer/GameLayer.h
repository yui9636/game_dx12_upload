#pragma once
#include "Layer.h"
#include "Registry/Registry.h"
#include <Component\PostEffectComponent.h>
#include <memory>
#include <unordered_map>

class HeadUpDisplay;
class Sprite;
class UIHPNumber;
class UIHPText2D;
class UIProgressBar2D;
class UIProgressBar3D;

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
    void ApplyHUDState();

    struct WorldHUDItem
    {
        std::shared_ptr<UIProgressBar3D> bar;
        std::shared_ptr<UIHPNumber> text;
    };

    Registry m_registry;
    PostEffectComponent m_postEffect;

    std::shared_ptr<Sprite> m_hudWhiteSprite;
    std::shared_ptr<UIProgressBar2D> m_playerBarBackground;
    std::shared_ptr<UIProgressBar2D> m_playerBarFill;
    std::shared_ptr<UIHPText2D> m_playerHPText;
    std::shared_ptr<UIProgressBar2D> m_bossBarBackground;
    std::shared_ptr<UIProgressBar2D> m_bossBarFill;
    std::shared_ptr<UIHPText2D> m_bossHPText;
    std::shared_ptr<HeadUpDisplay> m_headUpDisplay;
    std::unordered_map<EntityID, WorldHUDItem> m_worldHUD;
};
