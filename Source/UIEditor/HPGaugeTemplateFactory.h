#pragma once

#include <DirectXMath.h>

#include "UIEditor/UIEditorState.h"
#include "Undo/EntitySnapshot.h"

namespace UIEditorTemplates
{
    constexpr const char* kAuthoringCanvasName = "UIAuthoring_Canvas";
    constexpr const char* kWhiteTexturePath = "Data/Texture/UI/White.png";
    constexpr const char* kDefaultFontPath = "Data/Font/ArialUni.ttf";

    const char* GetTemplateName(UIEditorTemplateKind kind);
    const char* GetTemplatePlacementHint(UIEditorTemplateKind kind);
    const char* GetPartName(UIEditorPartKind kind);

    EntitySnapshot::Snapshot BuildCanvasSnapshot(const DirectX::XMFLOAT2& referenceResolution);
    EntitySnapshot::Snapshot BuildTemplateSnapshot(UIEditorTemplateKind kind);
    EntitySnapshot::Snapshot BuildPartSnapshot(UIEditorPartKind kind);
}
