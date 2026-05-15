#include "TerrainBuildSystem.h"
#include "TerrainComponent.h"
#include "TerrainAsset.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"
#include <DirectXMath.h>
#include <algorithm>

using namespace DirectX;

namespace {

struct TerrainVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
};

static float SampleH(const std::vector<float>& data, uint32_t res, int x, int z, float hs)
{
    x = std::clamp(x, 0, static_cast<int>(res) - 1);
    z = std::clamp(z, 0, static_cast<int>(res) - 1);
    return data[static_cast<size_t>(z) * res + x] * hs;
}

static XMFLOAT3 CalcNormal(float hL, float hR, float hD, float hU, float csx, float csz)
{
    XMVECTOR n = XMVector3Normalize(XMVectorSet(
        -(hR - hL) / (2.0f * csx),
        1.0f,
        -(hU - hD) / (2.0f * csz),
        0.0f));
    XMFLOAT3 out;
    XMStoreFloat3(&out, n);
    return out;
}

} // namespace

void TerrainBuildSystem::Update(Registry& registry)
{
    Query<TerrainComponent> q(registry);
    q.ForEachWithEntity([&](EntityID entity, TerrainComponent& tc) {
        if (!tc.needsRebuild) return;
        if (!tc.asset) return;
        RebuildEntity(entity, *tc.asset);
        tc.needsRebuild = false;
    });
}

void TerrainBuildSystem::RebuildEntity(EntityID entity, TerrainAsset& asset)
{
    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return;
    if (asset.heightData.empty()) {
        asset.GenerateFromNoise();
    }

    TerrainRuntimeData& rd = m_runtimeDataMap[entity];
    rd.worldSizeX   = asset.worldSizeX;
    rd.worldSizeZ   = asset.worldSizeZ;
    rd.heightScale  = asset.heightScale;
    rd.chunkCountX  = asset.chunkCountX;
    rd.chunkCountZ  = asset.chunkCountZ;
    rd.resolution   = asset.resolution;
    rd.chunks.clear();

    const uint32_t vertsPerChunkX = (asset.resolution / asset.chunkCountX) + 1;
    const uint32_t vertsPerChunkZ = (asset.resolution / asset.chunkCountZ) + 1;
    const float cellSizeX = asset.worldSizeX / static_cast<float>(asset.resolution - 1);
    const float cellSizeZ = asset.worldSizeZ / static_cast<float>(asset.resolution - 1);
    const float chunkW    = asset.worldSizeX / static_cast<float>(asset.chunkCountX);
    const float chunkD    = asset.worldSizeZ / static_cast<float>(asset.chunkCountZ);

    for (uint32_t cz = 0; cz < asset.chunkCountZ; ++cz) {
        for (uint32_t cx = 0; cx < asset.chunkCountX; ++cx) {
            const int baseX = static_cast<int>(cx * (asset.resolution / asset.chunkCountX));
            const int baseZ = static_cast<int>(cz * (asset.resolution / asset.chunkCountZ));

            std::vector<TerrainVertex> verts;
            verts.reserve(vertsPerChunkX * vertsPerChunkZ);
            for (uint32_t lz = 0; lz < vertsPerChunkZ; ++lz) {
                for (uint32_t lx = 0; lx < vertsPerChunkX; ++lx) {
                    int gx = baseX + static_cast<int>(lx);
                    int gz = baseZ + static_cast<int>(lz);
                    float y  = SampleH(asset.heightData, asset.resolution, gx, gz, asset.heightScale);
                    float wx = static_cast<float>(gx) * cellSizeX - asset.worldSizeX * 0.5f;
                    float wz = static_cast<float>(gz) * cellSizeZ - asset.worldSizeZ * 0.5f;
                    float hL = SampleH(asset.heightData, asset.resolution, gx-1, gz,   asset.heightScale);
                    float hR = SampleH(asset.heightData, asset.resolution, gx+1, gz,   asset.heightScale);
                    float hD = SampleH(asset.heightData, asset.resolution, gx,   gz-1, asset.heightScale);
                    float hU = SampleH(asset.heightData, asset.resolution, gx,   gz+1, asset.heightScale);
                    TerrainVertex v;
                    v.position = { wx, y, wz };
                    v.normal   = CalcNormal(hL, hR, hD, hU, cellSizeX, cellSizeZ);
                    v.uv       = {
                        static_cast<float>(gx) / static_cast<float>(asset.resolution - 1),
                        static_cast<float>(gz) / static_cast<float>(asset.resolution - 1)
                    };
                    verts.push_back(v);
                }
            }

            std::vector<uint32_t> indices;
            const uint32_t rows = vertsPerChunkZ - 1;
            const uint32_t cols = vertsPerChunkX - 1;
            indices.reserve(rows * cols * 6);
            for (uint32_t lz = 0; lz < rows; ++lz) {
                for (uint32_t lx = 0; lx < cols; ++lx) {
                    uint32_t i0 = lz * vertsPerChunkX + lx;
                    uint32_t i1 = i0 + 1;
                    uint32_t i2 = i0 + vertsPerChunkX;
                    uint32_t i3 = i2 + 1;
                    indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
                    indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
                }
            }

            TerrainChunkBuffer chunk;
            chunk.chunkWorldOffset = {
                static_cast<float>(cx) * chunkW - asset.worldSizeX * 0.5f,
                0.0f,
                static_cast<float>(cz) * chunkD - asset.worldSizeZ * 0.5f
            };
            chunk.indexCount = static_cast<uint32_t>(indices.size());
            const uint32_t vbSize = static_cast<uint32_t>(verts.size()   * sizeof(TerrainVertex));
            const uint32_t ibSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
            chunk.vertexBuffer = factory->CreateBuffer(vbSize, BufferType::Vertex, verts.data());
            chunk.indexBuffer  = factory->CreateBuffer(ibSize, BufferType::Index,  indices.data());
            rd.chunks.push_back(std::move(chunk));
        }
    }
}

const TerrainRuntimeData* TerrainBuildSystem::GetRuntimeData(EntityID entity) const
{
    auto it = m_runtimeDataMap.find(entity);
    return it != m_runtimeDataMap.end() ? &it->second : nullptr;
}

void TerrainBuildSystem::Clear()
{
    m_runtimeDataMap.clear();
}
