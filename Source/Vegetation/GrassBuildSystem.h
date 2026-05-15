#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"

class Registry;
class IBuffer;

// 草インスタンス 1 本分のデータ (HLSL の Instance Input と一致させる)。
struct GrassInstanceData {
    DirectX::XMFLOAT3 worldPos;     // 12
    float             scale;        //  4
    DirectX::XMFLOAT3 colorTint;    // 12
    float             rotationY;    //  4
};
static_assert(sizeof(GrassInstanceData) == 32, "GrassInstanceData must be 32 bytes");

struct GrassRuntimeData {
    std::unique_ptr<IBuffer> meshVertexBuffer;  // クロスクワッド (位置 + UV)
    std::unique_ptr<IBuffer> meshIndexBuffer;
    uint32_t meshIndexCount = 0;

    std::unique_ptr<IBuffer> instanceBuffer;
    uint32_t instanceCount = 0;

    DirectX::XMFLOAT3 boundsCenter  = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsExtents = { 0.0f, 0.0f, 0.0f };

    // VS/PS に渡すパラメータ (DrawCall でコピー)
    DirectX::XMFLOAT3 windDirection = { 1.0f, 0.0f, 0.0f };
    float windStrength = 0.35f;
    float windSpeed    = 1.4f;
    DirectX::XMFLOAT3 colorBottom = { 0.16f, 0.28f, 0.10f };
    DirectX::XMFLOAT3 colorTop    = { 0.55f, 0.78f, 0.30f };
    float drawDistance = 80.0f;
};

class GrassBuildSystem {
public:
    void Update(Registry& registry);

    const GrassRuntimeData* GetRuntimeData(EntityID entity) const;

    void Clear();

private:
    bool RebuildEntity(EntityID entity, Registry& registry);

    std::unordered_map<EntityID, GrassRuntimeData> m_runtimeDataMap;
};
