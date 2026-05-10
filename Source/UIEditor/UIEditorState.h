#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "Component/RectTransformComponent.h"
#include "Entity/Entity.h"

enum class UIEditorTemplateKind
{
    PlayerHP,
    BossHP
};

enum class UIEditorPartKind
{
    Canvas,
    GaugeRoot,
    Image,
    FillImage,
    DamagePreview,
    HPText
};

enum class UIEditorTool
{
    Select,
    Move,
    Resize,
    Pan
};

struct UIEditorViewState
{
    DirectX::XMFLOAT2 referenceResolution = { 1920.0f, 1080.0f };
    DirectX::XMFLOAT2 pan = { 0.0f, 0.0f };
    float zoom = 1.0f;
    float gridSize = 120.0f;
    bool showSafeArea = true;
    bool showGrid = true;
    bool snapToGrid = true;
    bool pixelSnap = true;
};

struct UIEditorHPPreviewState
{
    bool enabled = true;
    float ratio = 1.0f;
    float delayedRatio = 1.0f;
    int currentHP = 100;
    int maxHP = 100;
};

struct UIEditorInteractionState
{
    UIEditorTool tool = UIEditorTool::Select;
    bool draggingRect = false;
    bool resizingRect = false;
    bool panningView = false;
    EntityID editEntity = Entity::NULL_ID;
    EntityID hoveredEntity = Entity::NULL_ID;
    std::vector<EntityID> pickCandidates;
    DirectX::XMFLOAT2 pickPopupScreenPos = { 0.0f, 0.0f };
    RectTransformComponent beforeRect{};
    std::string statusMessage;
    float statusTimer = 0.0f;
    bool statusSuccess = true;
};

struct UIEditorPrefabState
{
    std::filesystem::path lastPrefabPath;
};
