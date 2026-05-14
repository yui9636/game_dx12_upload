#pragma once

#include <vector>

#include <DirectXMath.h>

#include "Entity/Entity.h"
// CanvasItemComponent はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

struct CanvasItemComponent;
struct HierarchyComponent;
struct NameComponent;
struct RectTransformComponent;
class Registry;
struct SpriteComponent;
struct TextComponent;
struct TransformComponent;

struct UIEditorRect
{
    EntityID entity = Entity::NULL_ID;
    NameComponent* name = nullptr;
    HierarchyComponent* hierarchy = nullptr;
    TransformComponent* transform = nullptr;
    RectTransformComponent* rect = nullptr;
    CanvasItemComponent* canvas = nullptr;
    SpriteComponent* sprite = nullptr;
    TextComponent* text = nullptr;
    DirectX::XMFLOAT2 center = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 size = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    int sortingLayer = 0;
    int orderInLayer = 0;
    float z = 0.0f;
};

namespace UIEditor
{
    class UIRectEvaluator
    {
    public:
        static bool Evaluate(Registry& registry, EntityID entity, UIEditorRect& outRect);
        static std::vector<UIEditorRect> CollectCanvasWidgets(Registry& registry, EntityID canvas);
    };
}
