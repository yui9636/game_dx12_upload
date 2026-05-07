#include "HPGaugeSystem.h"

#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HPGaugeComponent.h"
#include "Gameplay/HUDLinkComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    float Clamp01(float value)
    {
        return (std::max)(0.0f, (std::min)(1.0f, value));
    }

    float Approach(float current, float target, float speed, float dt)
    {
        if (speed <= 0.0f || dt <= 0.0f) {
            return target;
        }
        const float alpha = Clamp01(speed * dt);
        return current + (target - current) * alpha;
    }

    DirectX::XMFLOAT4 LerpColor(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t)
    {
        t = Clamp01(t);
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        };
    }

    DirectX::XMFLOAT4 ResolveColor(const HPGaugeFillComponent& fill, float ratio)
    {
        ratio = Clamp01(ratio);
        if (fill.colorMode == HPGaugeColorMode::Fixed) {
            return fill.fixedColor;
        }

        const float low = (std::max)(0.0f, fill.lowThreshold);
        const float mid = (std::max)(low + 0.0001f, fill.midThreshold);

        if (fill.colorMode == HPGaugeColorMode::Gradient) {
            if (ratio <= low) {
                return fill.lowColor;
            }
            if (ratio <= mid) {
                return LerpColor(fill.lowColor, fill.midColor, (ratio - low) / (mid - low));
            }
            return LerpColor(fill.midColor, fill.highColor, (ratio - mid) / ((std::max)(0.0001f, 1.0f - mid)));
        }

        if (ratio <= low) return fill.lowColor;
        if (ratio <= mid) return fill.midColor;
        return fill.highColor;
    }

    EntityID FirstPlayerWithHealth(Registry& registry)
    {
        EntityID result = Entity::NULL_ID;
        Query<PlayerTagComponent, HealthComponent> query(registry);
        query.ForEachWithEntity([&](EntityID entity, PlayerTagComponent&, HealthComponent& health) {
            if (Entity::IsNull(result) && health.maxHealth > 0) {
                result = entity;
            }
        });
        return result;
    }

    EntityID FirstEnemyWithHealth(Registry& registry)
    {
        EntityID result = Entity::NULL_ID;
        Query<EnemyTagComponent, HealthComponent> query(registry);
        query.ForEachWithEntity([&](EntityID entity, EnemyTagComponent&, HealthComponent& health) {
            if (Entity::IsNull(result) && health.maxHealth > 0) {
                result = entity;
            }
        });
        return result;
    }

    EntityID FirstBossWithHealth(Registry& registry)
    {
        EntityID result = Entity::NULL_ID;
        Query<HUDLinkComponent, HealthComponent> bossQuery(registry);
        bossQuery.ForEachWithEntity([&](EntityID entity, HUDLinkComponent& hud, HealthComponent& health) {
            if (Entity::IsNull(result) && hud.asBossHUD && health.maxHealth > 0) {
                result = entity;
            }
        });
        return Entity::IsNull(result) ? FirstEnemyWithHealth(registry) : result;
    }

    EntityID ResolveTarget(Registry& registry, const HPGaugeBindingComponent& binding)
    {
        switch (binding.targetMode) {
        case HPGaugeTargetMode::Explicit:
            return (!Entity::IsNull(binding.explicitTarget) && registry.IsAlive(binding.explicitTarget))
                ? binding.explicitTarget
                : Entity::NULL_ID;
        case HPGaugeTargetMode::FirstEnemy:
            return FirstEnemyWithHealth(registry);
        case HPGaugeTargetMode::FirstBoss:
            return FirstBossWithHealth(registry);
        case HPGaugeTargetMode::FirstPlayer:
        default:
            return FirstPlayerWithHealth(registry);
        }
    }

    bool IsBindingActive(const HPGaugeBindingComponent& binding)
    {
        if (!binding.targetValid) {
            return binding.visibleWhenNoTarget;
        }
        if (binding.hideWhenDead && binding.currentHP <= 0) {
            return false;
        }
        if (binding.hideWhenFull && binding.maxHP > 0 && binding.currentHP >= binding.maxHP) {
            return false;
        }
        return true;
    }

    void SetCanvasVisible(Registry& registry, EntityID entity, bool visible)
    {
        if (auto* canvas = registry.GetComponent<CanvasItemComponent>(entity)) {
            canvas->visible = visible;
        }
    }

    EntityID FindBindingRoot(Registry& registry, EntityID entity, EntityID explicitRoot)
    {
        if (!Entity::IsNull(explicitRoot) && registry.IsAlive(explicitRoot) &&
            registry.GetComponent<HPGaugeBindingComponent>(explicitRoot)) {
            return explicitRoot;
        }

        EntityID current = entity;
        for (int guard = 0; guard < 64 && !Entity::IsNull(current) && registry.IsAlive(current); ++guard) {
            if (registry.GetComponent<HPGaugeBindingComponent>(current)) {
                return current;
            }
            auto* hierarchy = registry.GetComponent<HierarchyComponent>(current);
            if (!hierarchy) {
                break;
            }
            current = hierarchy->parent;
        }
        return Entity::NULL_ID;
    }

    bool ShouldShowFill(const HPGaugeBindingComponent& binding, const HPGaugeFillComponent& fill)
    {
        if (!binding.targetValid && fill.hideWhenNoTarget) {
            return false;
        }
        if (fill.hideWhenFull && binding.targetValid && binding.maxHP > 0 && binding.currentHP >= binding.maxHP) {
            return false;
        }
        return IsBindingActive(binding);
    }

    bool ShouldShowText(const HPGaugeBindingComponent& binding, const HPGaugeTextComponent& text)
    {
        if (!binding.targetValid && text.hideWhenNoTarget) {
            return false;
        }
        if (text.hideWhenDead && binding.currentHP <= 0) {
            return false;
        }
        if (text.hideWhenFull && binding.targetValid && binding.maxHP > 0 && binding.currentHP >= binding.maxHP) {
            return false;
        }
        return IsBindingActive(binding);
    }

    std::string FormatText(const HPGaugeBindingComponent& binding, const HPGaugeTextComponent& gaugeText)
    {
        char buffer[128] = {};
        const int hp = (std::max)(0, binding.currentHP);
        const int maxHP = (std::max)(0, binding.maxHP);
        switch (gaugeText.format) {
        case HPGaugeTextFormat::CurrentOnly:
            std::snprintf(buffer, sizeof(buffer), "%d", hp);
            break;
        case HPGaugeTextFormat::Percent:
            std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(std::round(binding.targetRatio * 100.0f)));
            break;
        case HPGaugeTextFormat::LabelCurrentMax:
            std::snprintf(buffer, sizeof(buffer), "%s %d / %d", gaugeText.label.c_str(), hp, maxHP);
            break;
        case HPGaugeTextFormat::CurrentMax:
        default:
            std::snprintf(buffer, sizeof(buffer), "%d / %d", hp, maxHP);
            break;
        }
        return buffer;
    }
}

void HPGaugeSystem::Update(Registry& registry, float deltaTime)
{
    Query<HPGaugeBindingComponent> bindingQuery(registry);
    bindingQuery.ForEachWithEntity([&](EntityID entity, HPGaugeBindingComponent& binding) {
        const EntityID target = ResolveTarget(registry, binding);
        binding.resolvedTarget = target;
        binding.targetValid = false;

        const HealthComponent* health = (!Entity::IsNull(target) && registry.IsAlive(target))
            ? registry.GetComponent<HealthComponent>(target)
            : nullptr;

        if (health && health->maxHealth > 0) {
            binding.targetValid = true;
            binding.currentHP = health->health;
            binding.maxHP = health->maxHealth;
            binding.targetRatio = Clamp01(static_cast<float>((std::max)(0, health->health)) /
                                          static_cast<float>((std::max)(1, health->maxHealth)));
        } else {
            binding.currentHP = 0;
            binding.maxHP = 0;
            binding.targetRatio = 0.0f;
        }

        const float previousDisplayed = binding.displayedRatio;
        binding.displayedRatio = Approach(binding.displayedRatio, binding.targetRatio, binding.smoothingSpeed, deltaTime);

        if (binding.targetRatio < binding.delayedRatio - 0.0001f) {
            if (binding.damagePreviewTimer <= 0.0f && binding.targetRatio < previousDisplayed - 0.0001f) {
                binding.damagePreviewTimer = binding.damagePreviewDelay;
            }
            if (binding.damagePreviewTimer > 0.0f) {
                binding.damagePreviewTimer = (std::max)(0.0f, binding.damagePreviewTimer - deltaTime);
            } else {
                binding.delayedRatio = Approach(binding.delayedRatio, binding.targetRatio, binding.damagePreviewSpeed, deltaTime);
            }
        } else {
            binding.delayedRatio = binding.targetRatio;
            binding.damagePreviewTimer = 0.0f;
        }

        binding.displayedRatio = Clamp01(binding.displayedRatio);
        binding.delayedRatio = Clamp01(binding.delayedRatio);

        SetCanvasVisible(registry, entity, IsBindingActive(binding));
    });

    Query<HPGaugeFillComponent> fillQuery(registry);
    fillQuery.ForEachWithEntity([&](EntityID entity, HPGaugeFillComponent& fill) {
        const EntityID root = FindBindingRoot(registry, entity, fill.bindingRoot);
        HPGaugeBindingComponent* binding = Entity::IsNull(root) ? nullptr : registry.GetComponent<HPGaugeBindingComponent>(root);
        if (!binding) {
            fill.runtimeRatio = 0.0f;
            SetCanvasVisible(registry, entity, false);
            return;
        }

        float ratio = fill.useDelayedRatio ? binding->delayedRatio : binding->targetRatio;
        if (fill.useDisplayedRatio && !fill.useDelayedRatio) {
            ratio = binding->displayedRatio;
        }
        fill.runtimeRatio = (ratio <= fill.minVisibleRatio) ? 0.0f : Clamp01(ratio);
        SetCanvasVisible(registry, entity, ShouldShowFill(*binding, fill));

        if (auto* sprite = registry.GetComponent<SpriteComponent>(entity)) {
            sprite->tint = ResolveColor(fill, fill.runtimeRatio);
        }
    });

    Query<HPGaugeTextComponent> textQuery(registry);
    textQuery.ForEachWithEntity([&](EntityID entity, HPGaugeTextComponent& gaugeText) {
        const EntityID root = FindBindingRoot(registry, entity, gaugeText.bindingRoot);
        HPGaugeBindingComponent* binding = Entity::IsNull(root) ? nullptr : registry.GetComponent<HPGaugeBindingComponent>(root);
        if (!binding) {
            SetCanvasVisible(registry, entity, false);
            return;
        }

        SetCanvasVisible(registry, entity, ShouldShowText(*binding, gaugeText));
        if (auto* text = registry.GetComponent<TextComponent>(entity)) {
            text->text = FormatText(*binding, gaugeText);
        }
    });
}
