#pragma once
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "RenderContext/RenderQueue.h"
#include "Component/MaterialComponent.h"

// ECSからメッシュ情報を抽出し、描画伝票(RenderQueue)を作成する専用システム
class MeshExtractSystem {
public:
    void Extract(Registry& registry, RenderQueue& queue);
};