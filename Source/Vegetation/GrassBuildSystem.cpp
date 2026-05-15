#include "GrassBuildSystem.h"
#include "GrassComponent.h"
#include "Terrain/TerrainComponent.h"
#include "Terrain/TerrainAsset.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"
#include "Console/Logger.h"
#include <random>
#include <cmath>
#include <algorithm>

namespace {

struct GrassMeshVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 uv;
};

// 3 枚の quad を 60deg ずつ回した cross-mesh を生成する。
// 各 quad は (-w/2, 0, 0) - (w/2, 1, 0) の単位形状で VS でスケール/回転される。
static void BuildCrossQuadMesh(std::vector<GrassMeshVertex>& verts, std::vector<uint16_t>& indices)
{
    const int numQuads = 3;
    verts.clear();
    indices.clear();
    verts.reserve(numQuads * 4);
    indices.reserve(numQuads * 6);
    for (int q = 0; q < numQuads; ++q) {
        const float angleDeg = 60.0f * static_cast<float>(q);
        const float angle = angleDeg * 3.14159265f / 180.0f;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float halfW = 0.5f;
        // 四角形を XZ 面 (Y は高さ) で構築。回転は Y 軸まわり。
        DirectX::XMFLOAT3 p0 = { -halfW * c, 0.0f, -halfW * s };  // 左下
        DirectX::XMFLOAT3 p1 = {  halfW * c, 0.0f,  halfW * s };  // 右下
        DirectX::XMFLOAT3 p2 = { -halfW * c, 1.0f, -halfW * s };  // 左上
        DirectX::XMFLOAT3 p3 = {  halfW * c, 1.0f,  halfW * s };  // 右上
        uint16_t base = static_cast<uint16_t>(verts.size());
        verts.push_back({ p0, { 0.0f, 1.0f } });
        verts.push_back({ p1, { 1.0f, 1.0f } });
        verts.push_back({ p2, { 0.0f, 0.0f } });
        verts.push_back({ p3, { 1.0f, 0.0f } });
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
        indices.push_back(base + 1); indices.push_back(base + 2); indices.push_back(base + 3);
    }
}

} // anonymous namespace

void GrassBuildSystem::Update(Registry& registry)
{
    Query<GrassComponent> q(registry);
    q.ForEachWithEntity([&](EntityID entity, GrassComponent& gc) {
        if (!gc.enabled) return;
        if (!gc.needsRebuild) return;
        if (RebuildEntity(entity, registry)) {
            gc.needsRebuild = false;
        }
    });
}

bool GrassBuildSystem::RebuildEntity(EntityID entity, Registry& registry)
{
    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return false;

    GrassComponent* gc = registry.GetComponent<GrassComponent>(entity);
    if (!gc) return false;
    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(entity);
    if (!tc || !tc->asset) {
        // Terrain がないと配置できない (現状は terrain entity に紐づく実装)
        return false;
    }
    TerrainAsset& asset = *tc->asset;
    if (asset.heightData.empty() || asset.splatData.empty()) return false;

    GrassRuntimeData& rd = m_runtimeDataMap[entity];

    // メッシュは一度だけ作る (毎リビルドで再生成しない)
    if (!rd.meshVertexBuffer || !rd.meshIndexBuffer) {
        std::vector<GrassMeshVertex> verts;
        std::vector<uint16_t> indices;
        BuildCrossQuadMesh(verts, indices);
        rd.meshVertexBuffer = factory->CreateBuffer(
            static_cast<uint32_t>(verts.size() * sizeof(GrassMeshVertex)),
            BufferType::Vertex, verts.data());
        rd.meshIndexBuffer = factory->CreateBuffer(
            static_cast<uint32_t>(indices.size() * sizeof(uint16_t)),
            BufferType::Index, indices.data());
        rd.meshIndexCount = static_cast<uint32_t>(indices.size());
    }

    // パラメータをランタイムへコピー
    rd.windDirection = gc->windDirection;
    rd.windStrength  = gc->windStrength;
    rd.windSpeed     = gc->windSpeed;
    rd.colorBottom   = gc->colorBottom;
    rd.colorTop      = gc->colorTop;
    rd.drawDistance  = gc->drawDistance;

    // splatData から草インスタンスを生成。
    const uint32_t res = asset.resolution;
    if (res < 2) return false;
    const float cellSizeX = asset.worldSizeX / static_cast<float>(res - 1);
    const float cellSizeZ = asset.worldSizeZ / static_cast<float>(res - 1);
    const float halfW = asset.worldSizeX * 0.5f;
    const float halfD = asset.worldSizeZ * 0.5f;

    std::mt19937 rng(static_cast<uint32_t>(gc->seed));
    std::uniform_real_distribution<float> uni01(0.0f, 1.0f);

    std::vector<GrassInstanceData> instances;
    // 上限: 1 セル N 本 × 解像度二乗。安全弁として 200k 本でクランプ。
    const uint32_t maxBlades = (std::min<uint32_t>)(static_cast<uint32_t>(res) * static_cast<uint32_t>(res) * gc->maxBladesPerCell, 200000u);
    instances.reserve(maxBlades / 4);

    float minY =  1e9f, maxY = -1e9f;
    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            const size_t cellIdx = static_cast<size_t>(z) * res + x;
            const float grassWeight = asset.splatData[cellIdx * 4u + 0] / 255.0f;
            if (grassWeight < gc->densityThreshold) continue;

            // この cell に生やす本数
            const float scaledDensity = grassWeight * gc->densityMultiplier;
            const float bladesF = scaledDensity * static_cast<float>(gc->maxBladesPerCell);
            uint32_t blades = static_cast<uint32_t>(bladesF);
            // 端数分を確率的に追加
            if (uni01(rng) < (bladesF - static_cast<float>(blades))) blades++;
            if (blades == 0) continue;

            const float cellWorldX = static_cast<float>(x) * cellSizeX - halfW;
            const float cellWorldZ = static_cast<float>(z) * cellSizeZ - halfD;
            const float heightNorm = asset.heightData[cellIdx];
            const float baseY = heightNorm * asset.heightScale - asset.heightScale * 0.5f;

            for (uint32_t b = 0; b < blades; ++b) {
                if (instances.size() >= maxBlades) break;
                const float jitterX = (uni01(rng) - 0.5f) * cellSizeX;
                const float jitterZ = (uni01(rng) - 0.5f) * cellSizeZ;
                const float hVar = 1.0f + (uni01(rng) * 2.0f - 1.0f) * gc->bladeHeightVariance;
                const float scale = gc->bladeHeight * (std::max)(0.05f, hVar);
                const float rotY = uni01(rng) * 6.28318f;
                // 色は colorBottom..colorTop ではなく、シェーダ側で頂点 Y で混ぜる。
                // ここでは tint の variance (個体差) だけ持たせる。
                DirectX::XMFLOAT3 tint = {
                    1.0f + (uni01(rng) * 2.0f - 1.0f) * gc->colorTintVariance.x,
                    1.0f + (uni01(rng) * 2.0f - 1.0f) * gc->colorTintVariance.y,
                    1.0f + (uni01(rng) * 2.0f - 1.0f) * gc->colorTintVariance.z,
                };
                GrassInstanceData inst;
                inst.worldPos  = { cellWorldX + jitterX, baseY, cellWorldZ + jitterZ };
                inst.scale     = scale;
                inst.colorTint = tint;
                inst.rotationY = rotY;
                instances.push_back(inst);

                minY = (std::min)(minY, baseY);
                maxY = (std::max)(maxY, baseY + scale);
            }
        }
    }

    if (instances.empty()) {
        rd.instanceBuffer.reset();
        rd.instanceCount = 0;
        return true;
    }

    rd.instanceBuffer = factory->CreateBuffer(
        static_cast<uint32_t>(instances.size() * sizeof(GrassInstanceData)),
        BufferType::Vertex, instances.data());
    rd.instanceCount = static_cast<uint32_t>(instances.size());

    rd.boundsCenter = {
        0.0f,
        (minY + maxY) * 0.5f,
        0.0f
    };
    rd.boundsExtents = {
        asset.worldSizeX * 0.5f,
        (std::max)((maxY - minY) * 0.5f, 0.5f),
        asset.worldSizeZ * 0.5f
    };

    LOG_INFO("[Grass] Rebuilt entity %u: instances=%u", static_cast<uint32_t>(entity), rd.instanceCount);
    return true;
}

const GrassRuntimeData* GrassBuildSystem::GetRuntimeData(EntityID entity) const
{
    auto it = m_runtimeDataMap.find(entity);
    return it != m_runtimeDataMap.end() ? &it->second : nullptr;
}

void GrassBuildSystem::Clear()
{
    m_runtimeDataMap.clear();
}
