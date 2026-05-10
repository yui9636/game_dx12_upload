#include "UIEditor/HPGaugeTemplateFactory.h"

#include <algorithm>
#include <optional>
#include <string>

#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/HPGaugeComponent.h"

namespace
{
    struct TemplatePreset
    {
        const char* name;
        const char* placementHint;
        HPGaugeTargetMode targetMode;
        DirectX::XMFLOAT2 size;
        HPGaugeTextFormat textFormat;
        const char* label;
        float fontSize;
    };

    TemplatePreset GetPreset(UIEditorTemplateKind kind)
    {
        switch (kind) {
        case UIEditorTemplateKind::BossHP:
            return { "BossHP_Widget", "Recommended Level placement: top center", HPGaugeTargetMode::FirstBoss, { 760.0f, 46.0f }, HPGaugeTextFormat::LabelCurrentMax, "BOSS", 22.0f };
        case UIEditorTemplateKind::PlayerHP:
        default:
            return { "PlayerHP_Widget", "Recommended Level placement: top left", HPGaugeTargetMode::FirstPlayer, { 440.0f, 56.0f }, HPGaugeTextFormat::CurrentMax, "HP", 24.0f };
        }
    }

    TransformComponent MakeTransform(const DirectX::XMFLOAT2& position)
    {
        TransformComponent transform{};
        transform.localPosition = { position.x, position.y, 0.0f };
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;
        return transform;
    }

    RectTransformComponent MakeRect(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& size)
    {
        RectTransformComponent rect{};
        rect.anchoredPosition = position;
        rect.sizeDelta = size;
        rect.pivot = { 0.5f, 0.5f };
        rect.anchorMin = { 0.5f, 0.5f };
        rect.anchorMax = { 0.5f, 0.5f };
        return rect;
    }

    CanvasItemComponent MakeCanvasItem(int order)
    {
        CanvasItemComponent item{};
        item.orderInLayer = order;
        item.pixelSnap = true;
        return item;
    }

    EntitySnapshot::Node MakeNode(uint32_t localId,
                                  uint32_t parentId,
                                  const std::string& name,
                                  const DirectX::XMFLOAT2& position,
                                  const DirectX::XMFLOAT2& size,
                                  int order)
    {
        EntitySnapshot::Node node;
        node.localID = localId;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = parentId;
        node.externalParent = Entity::NULL_ID;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = MakeTransform(position);
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = MakeRect(position, size);
        std::get<std::optional<CanvasItemComponent>>(node.components) = MakeCanvasItem(order);
        return node;
    }

    SpriteComponent MakeSprite(const DirectX::XMFLOAT4& tint)
    {
        SpriteComponent sprite{};
        sprite.textureAssetPath = UIEditorTemplates::kWhiteTexturePath;
        sprite.tint = tint;
        return sprite;
    }

    HPGaugeBindingComponent MakeBinding(HPGaugeTargetMode targetMode)
    {
        HPGaugeBindingComponent binding{};
        binding.targetMode = targetMode;
        binding.visibleWhenNoTarget = true;
        binding.hideWhenDead = true;
        binding.hideWhenFull = false;
        return binding;
    }

    HPGaugeFillComponent MakeFill(bool delayed)
    {
        HPGaugeFillComponent fill{};
        fill.useDisplayedRatio = !delayed;
        fill.useDelayedRatio = delayed;
        fill.hideWhenNoTarget = false;
        fill.runtimeRatio = 1.0f;
        if (delayed) {
            fill.colorMode = HPGaugeColorMode::Fixed;
            fill.fixedColor = { 0.85f, 0.16f, 0.12f, 0.42f };
        }
        return fill;
    }

    TextComponent MakeText(float fontSize)
    {
        TextComponent text{};
        text.text = "100 / 100";
        text.fontAssetPath = UIEditorTemplates::kDefaultFontPath;
        text.fontSize = fontSize;
        text.color = { 1.0f, 1.0f, 1.0f, 0.96f };
        text.alignment = TextAlignment::Center;
        return text;
    }

    HPGaugeTextComponent MakeGaugeText(HPGaugeTextFormat format, const char* label)
    {
        HPGaugeTextComponent text{};
        text.format = format;
        text.label = label ? label : "HP";
        text.hideWhenNoTarget = false;
        return text;
    }
}

namespace UIEditorTemplates
{
    const char* GetTemplateName(UIEditorTemplateKind kind)
    {
        return GetPreset(kind).name;
    }

    const char* GetTemplatePlacementHint(UIEditorTemplateKind kind)
    {
        return GetPreset(kind).placementHint;
    }

    const char* GetPartName(UIEditorPartKind kind)
    {
        switch (kind) {
        case UIEditorPartKind::Canvas: return "Canvas";
        case UIEditorPartKind::GaugeRoot: return "Gauge Root";
        case UIEditorPartKind::FillImage: return "Fill Image";
        case UIEditorPartKind::DamagePreview: return "Damage Preview";
        case UIEditorPartKind::HPText: return "HP Text";
        case UIEditorPartKind::Image:
        default: return "Image";
        }
    }

    EntitySnapshot::Snapshot BuildCanvasSnapshot(const DirectX::XMFLOAT2& referenceResolution)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node canvas = MakeNode(
            0,
            EntitySnapshot::kInvalidLocalID,
            kAuthoringCanvasName,
            { 0.0f, 0.0f },
            referenceResolution,
            -100);
        if (auto& item = std::get<std::optional<CanvasItemComponent>>(canvas.components); item.has_value()) {
            item->interactable = false;
        }

        snapshot.nodes.push_back(std::move(canvas));
        return snapshot;
    }

    EntitySnapshot::Snapshot BuildTemplateSnapshot(UIEditorTemplateKind kind)
    {
        const TemplatePreset preset = GetPreset(kind);
        const float barHeight = (std::max)(18.0f, preset.size.y - 16.0f);
        const DirectX::XMFLOAT2 barSize = { preset.size.x, barHeight };

        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node root = MakeNode(0, EntitySnapshot::kInvalidLocalID, preset.name, { 0.0f, 0.0f }, preset.size, 100);
        std::get<std::optional<HPGaugeBindingComponent>>(root.components) = MakeBinding(preset.targetMode);

        EntitySnapshot::Node background = MakeNode(1, 0, "Background", { 0.0f, 0.0f }, barSize, 0);
        std::get<std::optional<SpriteComponent>>(background.components) = MakeSprite({ 0.02f, 0.02f, 0.025f, 0.72f });

        EntitySnapshot::Node damage = MakeNode(2, 0, "DamagePreview", { 0.0f, 0.0f }, barSize, 1);
        std::get<std::optional<SpriteComponent>>(damage.components) = MakeSprite({ 0.85f, 0.16f, 0.12f, 0.42f });
        std::get<std::optional<HPGaugeFillComponent>>(damage.components) = MakeFill(true);

        EntitySnapshot::Node fill = MakeNode(3, 0, "Fill", { 0.0f, 0.0f }, barSize, 2);
        std::get<std::optional<SpriteComponent>>(fill.components) = MakeSprite({ 0.18f, 0.86f, 0.36f, 0.94f });
        std::get<std::optional<HPGaugeFillComponent>>(fill.components) = MakeFill(false);

        EntitySnapshot::Node text = MakeNode(4, 0, "HP_Text", { 0.0f, -1.0f }, preset.size, 3);
        std::get<std::optional<TextComponent>>(text.components) = MakeText(preset.fontSize);
        std::get<std::optional<HPGaugeTextComponent>>(text.components) = MakeGaugeText(preset.textFormat, preset.label);

        snapshot.nodes.push_back(std::move(root));
        snapshot.nodes.push_back(std::move(background));
        snapshot.nodes.push_back(std::move(damage));
        snapshot.nodes.push_back(std::move(fill));
        snapshot.nodes.push_back(std::move(text));
        return snapshot;
    }

    EntitySnapshot::Snapshot BuildPartSnapshot(UIEditorPartKind kind)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        switch (kind) {
        case UIEditorPartKind::GaugeRoot: {
            EntitySnapshot::Node root = MakeNode(0, EntitySnapshot::kInvalidLocalID, "HPGauge_Widget", { 0.0f, 0.0f }, { 420.0f, 56.0f }, 100);
            std::get<std::optional<HPGaugeBindingComponent>>(root.components) = MakeBinding(HPGaugeTargetMode::FirstPlayer);
            snapshot.nodes.push_back(std::move(root));
            break;
        }
        case UIEditorPartKind::FillImage: {
            EntitySnapshot::Node fill = MakeNode(0, EntitySnapshot::kInvalidLocalID, "Fill", { 0.0f, 0.0f }, { 360.0f, 28.0f }, 2);
            std::get<std::optional<SpriteComponent>>(fill.components) = MakeSprite({ 0.18f, 0.86f, 0.36f, 0.94f });
            std::get<std::optional<HPGaugeFillComponent>>(fill.components) = MakeFill(false);
            snapshot.nodes.push_back(std::move(fill));
            break;
        }
        case UIEditorPartKind::DamagePreview: {
            EntitySnapshot::Node damage = MakeNode(0, EntitySnapshot::kInvalidLocalID, "DamagePreview", { 0.0f, 0.0f }, { 360.0f, 28.0f }, 1);
            std::get<std::optional<SpriteComponent>>(damage.components) = MakeSprite({ 0.85f, 0.16f, 0.12f, 0.42f });
            std::get<std::optional<HPGaugeFillComponent>>(damage.components) = MakeFill(true);
            snapshot.nodes.push_back(std::move(damage));
            break;
        }
        case UIEditorPartKind::HPText: {
            EntitySnapshot::Node text = MakeNode(0, EntitySnapshot::kInvalidLocalID, "HP_Text", { 0.0f, 0.0f }, { 360.0f, 40.0f }, 3);
            std::get<std::optional<TextComponent>>(text.components) = MakeText(24.0f);
            std::get<std::optional<HPGaugeTextComponent>>(text.components) = MakeGaugeText(HPGaugeTextFormat::CurrentMax, "HP");
            snapshot.nodes.push_back(std::move(text));
            break;
        }
        case UIEditorPartKind::Image:
        default: {
            EntitySnapshot::Node image = MakeNode(0, EntitySnapshot::kInvalidLocalID, "Image", { 0.0f, 0.0f }, { 360.0f, 28.0f }, 0);
            std::get<std::optional<SpriteComponent>>(image.components) = MakeSprite({ 0.08f, 0.09f, 0.11f, 0.80f });
            snapshot.nodes.push_back(std::move(image));
            break;
        }
        }

        return snapshot;
    }
}
