#include "TerrainBuildSystem.h"
#include "TerrainComponent.h"
#include "TerrainAsset.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"
#include "RHI/ITexture.h"
#include "GpuResourceUtils.h"
#include "Console/Logger.h"
#include "System/PathResolver.h"
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace DirectX;

namespace {

struct TerrainVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
};

struct WaterVertex {
    XMFLOAT3 position;
    XMFLOAT2 uv;
    XMFLOAT2 shoreData;
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

static void EnsureDefaultSplatData(TerrainAsset& asset)
{
    // 解像度変更や古いシーン読み込みでサイズが合わない場合は、草レイヤー全面で初期化する。
    const size_t pixelCount =
        static_cast<size_t>(asset.resolution) * static_cast<size_t>(asset.resolution);
    const size_t expectedSize = pixelCount * 4u;
    if (asset.splatData.size() == expectedSize) {
        return;
    }

    asset.splatData.assign(expectedSize, 0);
    for (size_t i = 0; i < pixelCount; ++i) {
        asset.splatData[i * 4u] = 255;
    }
}

static std::vector<uint32_t> BuildLodIndices(uint32_t vertsPerChunkX, uint32_t vertsPerChunkZ, uint32_t step)
{
    const uint32_t rows = vertsPerChunkZ - 1;
    const uint32_t cols = vertsPerChunkX - 1;
    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>((rows + step - 1) / step) * ((cols + step - 1) / step) * 6u);

    for (uint32_t z = 0; z < rows; z += step) {
        const uint32_t z1 = (std::min)(z + step, rows);
        for (uint32_t x = 0; x < cols; x += step) {
            const uint32_t x1 = (std::min)(x + step, cols);
            const uint32_t i0 = z  * vertsPerChunkX + x;
            const uint32_t i1 = z  * vertsPerChunkX + x1;
            const uint32_t i2 = z1 * vertsPerChunkX + x;
            const uint32_t i3 = z1 * vertsPerChunkX + x1;
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    return indices;
}

static void BuildWaterMesh(IResourceFactory& factory, TerrainRuntimeData& rd, const TerrainAsset& asset)
{
    rd.waterVertexBuffer.reset();
    rd.waterIndexBuffer.reset();
    rd.waterIndexCount = 0;

    if (!asset.water.enabled) {
        return;
    }

    const uint32_t kGrid = (std::min<uint32_t>)((std::max)(asset.resolution - 1u, 2u), 128u);
    const float halfW = asset.worldSizeX * 0.5f;
    const float halfZ = asset.worldSizeZ * 0.5f;
    const float seaLevel = asset.water.seaLevel;
    const float waterY = seaLevel + (std::max)(0.05f, asset.heightScale * 0.003f);
    const float shoreDepth = (std::max)(0.75f, asset.heightScale * 0.06f);
    const uint32_t vertsPerSide = kGrid + 1u;

    std::vector<WaterVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(static_cast<size_t>(vertsPerSide) * vertsPerSide);
    indices.reserve(static_cast<size_t>(kGrid) * kGrid * 6u);

    bool hasVisibleWater = false;
    for (uint32_t z = 0; z < vertsPerSide; ++z) {
        for (uint32_t x = 0; x < vertsPerSide; ++x) {
            const float u0 = static_cast<float>(x) / static_cast<float>(kGrid);
            const float v0 = static_cast<float>(z) / static_cast<float>(kGrid);
            const float terrainY = asset.SampleHeight(u0, v0);
            const float waterDepth = seaLevel - terrainY;
            const float shore = std::clamp(waterDepth / shoreDepth, 0.0f, 1.0f);
            hasVisibleWater = hasVisibleWater || waterDepth > 0.02f;

            vertices.push_back({
                { u0 * asset.worldSizeX - halfW, waterY, v0 * asset.worldSizeZ - halfZ },
                { u0, v0 },
                { shore, 0.0f }
            });
        }
    }

    if (!hasVisibleWater) {
        return;
    }

    for (uint32_t z = 0; z < kGrid; ++z) {
        for (uint32_t x = 0; x < kGrid; ++x) {
            const uint32_t i0 = z * vertsPerSide + x;
            const uint32_t i1 = i0 + 1u;
            const uint32_t i2 = (z + 1u) * vertsPerSide + x;
            const uint32_t i3 = i2 + 1u;
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    rd.waterVertexBuffer = factory.CreateBuffer(
        static_cast<uint32_t>(vertices.size() * sizeof(WaterVertex)),
        BufferType::Vertex,
        vertices.data());
    rd.waterIndexBuffer = factory.CreateBuffer(
        static_cast<uint32_t>(indices.size() * sizeof(uint32_t)),
        BufferType::Index,
        indices.data());
    rd.waterIndexCount = static_cast<uint32_t>(indices.size());
}

} // 無名名前空間

void TerrainBuildSystem::Update(Registry& registry)
{
    Query<TerrainComponent> q(registry);
    q.ForEachWithEntity([&](EntityID entity, TerrainComponent& tc) {
        if (!tc.asset) return;
        if (tc.needsRebuild) {
            RebuildEntity(entity, *tc.asset);
            tc.needsRebuild = false;
            tc.needsSplatUpload = false;   // 全部入りで更新済み
        } else if (tc.needsSplatUpload) {
            // ペイント等のスプラットのみ変更時は重いメッシュ再構築を回避する。
            UploadSplatOnly(entity, *tc.asset);
            tc.needsSplatUpload = false;
        }
    });
}

void TerrainBuildSystem::RebuildEntity(EntityID entity, TerrainAsset& asset)
{
    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return;

    asset.resolution = std::max<uint32_t>(2, asset.resolution);
    asset.UpgradeLegacyDefaultWorldSize();
    asset.chunkCountX = std::clamp<uint32_t>(asset.chunkCountX, 1, asset.resolution - 1);
    asset.chunkCountZ = std::clamp<uint32_t>(asset.chunkCountZ, 1, asset.resolution - 1);

    const size_t expectedHeightCount =
        static_cast<size_t>(asset.resolution) * static_cast<size_t>(asset.resolution);
    if (asset.heightData.size() != expectedHeightCount) {
        asset.GenerateFromNoise();
    }
    EnsureDefaultSplatData(asset);
    asset.EnsureDefaultLayers();

    TerrainRuntimeData& rd = m_runtimeDataMap[entity];
    rd.worldSizeX   = asset.worldSizeX;
    rd.worldSizeZ   = asset.worldSizeZ;
    rd.heightScale  = asset.heightScale;
    rd.chunkCountX  = asset.chunkCountX;
    rd.chunkCountZ  = asset.chunkCountZ;
    rd.resolution   = asset.resolution;
    rd.chunks.clear();
    rd.gpuSplatMap.reset();
    rd.waterEnabled = asset.water.enabled;
    rd.waterSeaLevel = asset.water.seaLevel;
    rd.waterShallowColor = asset.water.shallowColor;
    rd.waterDeepColor = asset.water.deepColor;
    rd.waterDepthFade = asset.water.depthFade;
    rd.waterWaveSpeed = asset.water.waveSpeed;
    rd.waterWaveScale = asset.water.waveScale;
    // アルベドは "パス変化したものだけ" 再ロードする。tileScale はリセット不要。
    // (リセットすると毎フレーム再ロード扱いになるためここでは reset しない)

    const uint32_t cellsX = asset.resolution - 1;
    const uint32_t cellsZ = asset.resolution - 1;
    const float cellSizeX = asset.worldSizeX / static_cast<float>(asset.resolution - 1);
    const float cellSizeZ = asset.worldSizeZ / static_cast<float>(asset.resolution - 1);

    for (uint32_t cz = 0; cz < asset.chunkCountZ; ++cz) {
        for (uint32_t cx = 0; cx < asset.chunkCountX; ++cx) {
            const uint32_t startX = (cx * cellsX) / asset.chunkCountX;
            const uint32_t endX   = ((cx + 1) * cellsX) / asset.chunkCountX;
            const uint32_t startZ = (cz * cellsZ) / asset.chunkCountZ;
            const uint32_t endZ   = ((cz + 1) * cellsZ) / asset.chunkCountZ;

            const uint32_t vertsPerChunkX = endX - startX + 1;
            const uint32_t vertsPerChunkZ = endZ - startZ + 1;
            const float chunkOriginX = static_cast<float>(startX) * cellSizeX - asset.worldSizeX * 0.5f;
            const float chunkOriginZ = static_cast<float>(startZ) * cellSizeZ - asset.worldSizeZ * 0.5f;

            std::vector<TerrainVertex> verts;
            verts.reserve(static_cast<size_t>(vertsPerChunkX) * vertsPerChunkZ);
            float minY = asset.heightScale;
            float maxY = -asset.heightScale;
            for (uint32_t lz = 0; lz < vertsPerChunkZ; ++lz) {
                for (uint32_t lx = 0; lx < vertsPerChunkX; ++lx) {
                    int gx = static_cast<int>(startX + lx);
                    int gz = static_cast<int>(startZ + lz);
                    float y  = SampleH(asset.heightData, asset.resolution, gx, gz, asset.heightScale) - asset.heightScale * 0.5f;
                    float wx = static_cast<float>(lx) * cellSizeX;
                    float wz = static_cast<float>(lz) * cellSizeZ;
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
                    minY = (std::min)(minY, y);
                    maxY = (std::max)(maxY, y);
                    verts.push_back(v);
                }
            }

            std::vector<uint32_t> lodIndices[5];
            for (uint32_t lod = 0; lod < 5; ++lod) {
                lodIndices[lod] = BuildLodIndices(vertsPerChunkX, vertsPerChunkZ, 1u << lod);
            }

            TerrainChunkBuffer chunk;
            chunk.chunkWorldOffset = { chunkOriginX, 0.0f, chunkOriginZ };
            const float chunkWidth = static_cast<float>(vertsPerChunkX - 1) * cellSizeX;
            const float chunkDepth = static_cast<float>(vertsPerChunkZ - 1) * cellSizeZ;
            chunk.boundsCenter = {
                chunkOriginX + chunkWidth * 0.5f,
                (minY + maxY) * 0.5f,
                chunkOriginZ + chunkDepth * 0.5f
            };
            chunk.boundsExtents = {
                chunkWidth * 0.5f,
                (std::max)((maxY - minY) * 0.5f, 0.1f),
                chunkDepth * 0.5f
            };
            chunk.indexCount = static_cast<uint32_t>(lodIndices[0].size());
            const uint32_t vbSize = static_cast<uint32_t>(verts.size()   * sizeof(TerrainVertex));
            chunk.vertexBuffer = factory->CreateBuffer(vbSize, BufferType::Vertex, verts.data());
            for (int lod = 0; lod < 5; ++lod) {
                chunk.lodIndexCounts[lod] = static_cast<uint32_t>(lodIndices[lod].size());
                const uint32_t ibSize = static_cast<uint32_t>(lodIndices[lod].size() * sizeof(uint32_t));
                chunk.lodIndexBuffers[lod] = factory->CreateBuffer(ibSize, BufferType::Index, lodIndices[lod].data());
            }
            chunk.indexBuffer = chunk.lodIndexBuffers[0].get()
                ? factory->CreateBuffer(
                    static_cast<uint32_t>(lodIndices[0].size() * sizeof(uint32_t)),
                    BufferType::Index,
                    lodIndices[0].data())
                : nullptr;
            rd.chunks.push_back(std::move(chunk));
        }
    }

    BuildWaterMesh(*factory, rd, asset);

    {
        const size_t expectedSplat =
            static_cast<size_t>(asset.resolution) * asset.resolution * 4u;
        if (asset.splatData.size() == expectedSplat) {
            DirectX::ScratchImage img;
            if (SUCCEEDED(img.Initialize2D(
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                    asset.resolution, asset.resolution, 1, 1)))
            {
                const DirectX::Image* im = img.GetImage(0, 0, 0);
                if (im && im->pixels) {
                    const size_t rowBytes = static_cast<size_t>(asset.resolution) * 4u;
                    for (uint32_t row = 0; row < asset.resolution; ++row) {
                        memcpy(im->pixels + im->rowPitch * row,
                               asset.splatData.data() + rowBytes * row,
                               rowBytes);
                    }
                    rd.gpuSplatMap = factory->CreateTextureFromMemory(img, img.GetMetadata());
                }
            }
        }
    }

    const int layerCount = static_cast<int>(
        (std::min)(asset.layers.size(), static_cast<size_t>(3)));

    // 1 枚分の (path, cachedPath, gpuTex) を一般化ロード処理にまとめる。
    auto loadIfChanged = [&](const std::string& path, std::string& cached, std::unique_ptr<ITexture>& gpu, const char* label, int layerIdx) {
        if (path.empty()) {
            gpu.reset();
            cached.clear();
            return;
        }
        if (gpu && cached == path) {
            return; // unchanged, skip
        }
        const std::string resolvedPath = PathResolver::Resolve(path);
        DirectX::ScratchImage img;
        DirectX::TexMetadata  meta;
        if (SUCCEEDED(GpuResourceUtils::LoadImageFromFile(resolvedPath.c_str(), img, meta))) {
            gpu = factory->CreateTextureFromMemory(img, meta);
            if (!gpu) {
                cached.clear();
                LOG_WARN("[Terrain] Layer %d %s upload failed: %s", layerIdx, label, resolvedPath.c_str());
            } else {
                cached = path;
            }
        } else {
            gpu.reset();
            cached.clear();
            LOG_WARN("[Terrain] Layer %d %s load failed: %s", layerIdx, label, resolvedPath.c_str());
        }
    };

    for (int i = 0; i < layerCount; ++i) {
        rd.layerTileScales[i] = asset.layers[i].tileScale;
        loadIfChanged(asset.layers[i].albedoPath,    rd.loadedAlbedoPaths[i], rd.gpuAlbedos[i], "albedo",     i);
        loadIfChanged(asset.layers[i].normalPath,    rd.loadedNormalPaths[i], rd.gpuNormals[i], "normal",     i);
        loadIfChanged(asset.layers[i].roughnessPath, rd.loadedMRAPaths[i],    rd.gpuMRAs[i],    "metallic-roughness-AO", i);
    }
}

bool TerrainBuildSystem::UploadSplatOnly(EntityID entity, TerrainAsset& asset)
{
    auto it = m_runtimeDataMap.find(entity);
    if (it == m_runtimeDataMap.end()) {
        // ランタイムデータが無ければフルリビルドにフォールバック。
        RebuildEntity(entity, asset);
        return true;
    }
    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return false;

    TerrainRuntimeData& rd = it->second;
    const size_t expectedSplat =
        static_cast<size_t>(asset.resolution) * asset.resolution * 4u;
    if (asset.splatData.size() != expectedSplat) {
        return false;
    }

    DirectX::ScratchImage img;
    if (FAILED(img.Initialize2D(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            asset.resolution, asset.resolution, 1, 1))) {
        return false;
    }
    const DirectX::Image* im = img.GetImage(0, 0, 0);
    if (!im || !im->pixels) return false;

    const size_t rowBytes = static_cast<size_t>(asset.resolution) * 4u;
    for (uint32_t row = 0; row < asset.resolution; ++row) {
        memcpy(im->pixels + im->rowPitch * row,
               asset.splatData.data() + rowBytes * row,
               rowBytes);
    }
    rd.gpuSplatMap = factory->CreateTextureFromMemory(img, img.GetMetadata());
    return rd.gpuSplatMap != nullptr;
}

const TerrainRuntimeData* TerrainBuildSystem::GetRuntimeData(EntityID entity) const
{
    auto it = m_runtimeDataMap.find(entity);
    return it != m_runtimeDataMap.end() ? &it->second : nullptr;
}

void TerrainBuildSystem::UpdateLod(Registry& registry,
                                   const DirectX::XMFLOAT3& cameraPosition,
                                   const float lodDistances[5])
{
    for (auto& [entity, rd] : m_runtimeDataMap) {
        if (!registry.IsAlive(entity)) {
            continue;
        }

        DirectX::XMFLOAT3 entityOffset = { 0.0f, 0.0f, 0.0f };
        if (const TransformComponent* transform = registry.GetComponent<TransformComponent>(entity)) {
            entityOffset = transform->worldPosition;
        }

        for (TerrainChunkBuffer& chunk : rd.chunks) {
            const float cx = chunk.boundsCenter.x + entityOffset.x;
            const float cy = chunk.boundsCenter.y + entityOffset.y;
            const float cz = chunk.boundsCenter.z + entityOffset.z;
            const float dx = cx - cameraPosition.x;
            const float dy = cy - cameraPosition.y;
            const float dz = cz - cameraPosition.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            int lod = 0;
            while (lod < 4 && distance > lodDistances[lod]) {
                ++lod;
            }
            chunk.currentLod = std::clamp(lod, 0, 4);
        }
    }
}

void TerrainBuildSystem::Clear()
{
    m_runtimeDataMap.clear();
}
