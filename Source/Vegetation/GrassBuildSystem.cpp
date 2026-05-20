#include "GrassBuildSystem.h"
#include "GrassComponent.h"
#include "Terrain/TerrainComponent.h"
#include "Terrain/TerrainAsset.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "System/ResourceManager.h"
#include "Render/Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"
#include "RHI/ITexture.h"
#include "Model/Model.h"
#include "Model/ModelResource.h"
#include "Console/Logger.h"
#include <random>
#include <cmath>
#include <algorithm>

namespace {

// Load (or cache) the model + albedo for one runtime layer. Returns true if
// the runtime layer now has valid mesh buffers.
bool LoadLayerMesh(FoliageLayerRuntime& rd, const FoliageLayer& layer)
{
    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return false;

    if (rd.model && rd.loadedMeshPath == layer.meshPath && rd.meshVertexBuffer) {
        return true;  // already loaded
    }

    // sourceOnly=true bypasses the (sometimes empty) cereal cache and parses
    // the source .fbx / .gltf directly.
    rd.model = ResourceManager::Instance().GetModel(layer.meshPath, 1.0f, true);
    rd.loadedMeshPath = layer.meshPath;
    rd.meshVertexBuffer = nullptr;
    rd.meshIndexBuffer  = nullptr;
    rd.meshIndexCount   = 0;
    rd.meshVertexStride = 0;
    rd.albedoTexture    = nullptr;
    if (!rd.model) {
        LOG_WARN("[Foliage] Failed to load model: %s", layer.meshPath.c_str());
        return false;
    }
    auto modelRes = rd.model->GetModelResource();
    if (!modelRes || modelRes->GetMeshCount() == 0) {
        LOG_WARN("[Foliage] Model has no meshes: %s", layer.meshPath.c_str());
        return false;
    }
    const ModelResource::MeshResource* mesh = modelRes->GetMeshResource(0);
    if (!mesh || !mesh->vertexBuffer || !mesh->indexBuffer) {
        LOG_WARN("[Foliage] Mesh has no GPU buffers: %s", layer.meshPath.c_str());
        return false;
    }
    rd.meshVertexBuffer = mesh->vertexBuffer.get();
    rd.meshIndexBuffer  = mesh->indexBuffer.get();
    rd.meshIndexCount   = mesh->indexCount;
    rd.meshVertexStride = mesh->vertexStride;

    rd.albedoTexture = mesh->material.albedoMap ? mesh->material.albedoMap.get() :
                      (mesh->material.diffuseMap ? mesh->material.diffuseMap.get() : nullptr);
    if (!rd.albedoTexture) {
        const std::string& path =
            !mesh->material.albedoTextureFileName.empty() ? mesh->material.albedoTextureFileName :
            !mesh->material.diffuseTextureFileName.empty() ? mesh->material.diffuseTextureFileName : std::string();
        if (!path.empty()) {
            auto tex = ResourceManager::Instance().GetTexture(path);
            if (tex) rd.albedoTexture = tex.get();
        }
    }
    if (!rd.albedoTexture) {
        // Fall back to a sibling .png in the model folder.
        auto slash = layer.meshPath.find_last_of("/\\");
        std::string folder = (slash != std::string::npos) ? layer.meshPath.substr(0, slash + 1) : "";
        // Try to find a png matching the model basename, then well-known siblings.
        std::string base;
        auto dot = layer.meshPath.find_last_of('.');
        if (dot != std::string::npos) {
            base = layer.meshPath.substr((slash != std::string::npos) ? slash + 1 : 0,
                                         dot - ((slash != std::string::npos) ? slash + 1 : 0));
        }
        const char* candidates[] = { "zassou1.png", "zassou2.png", "zassou3.png" };
        std::string tryPath = folder + base + ".png";
        auto tex = ResourceManager::Instance().GetTexture(tryPath);
        if (tex) {
            rd.albedoTexture = tex.get();
        } else {
            for (const char* c : candidates) {
                tex = ResourceManager::Instance().GetTexture(folder + c);
                if (tex) { rd.albedoTexture = tex.get(); break; }
            }
        }
    }

    rd.alphaCutoff = (mesh->material.alphaCutoff > 0.001f) ? mesh->material.alphaCutoff : 0.5f;
    const DirectX::BoundingBox& lb = mesh->localBounds;
    rd.meshLocalMin = {
        lb.Center.x - lb.Extents.x,
        lb.Center.y - lb.Extents.y,
        lb.Center.z - lb.Extents.z
    };
    rd.meshLocalMax = {
        lb.Center.x + lb.Extents.x,
        lb.Center.y + lb.Extents.y,
        lb.Center.z + lb.Extents.z
    };
    LOG_INFO("[Foliage] Loaded '%s' verts-stride=%u indices=%u albedo=%p",
             layer.meshPath.c_str(), rd.meshVertexStride, rd.meshIndexCount, rd.albedoTexture);
    return true;
}

// Sample terrain slope (returned in degrees) at the given cell.
float SampleSlopeDegrees(const TerrainAsset& asset, int x, int z, float cellSizeX, float cellSizeZ)
{
    const int res = static_cast<int>(asset.resolution);
    auto sampleH = [&](int sx, int sz) -> float {
        sx = std::clamp(sx, 0, res - 1);
        sz = std::clamp(sz, 0, res - 1);
        return asset.heightData[static_cast<size_t>(sz) * res + sx] * asset.heightScale;
    };
    const float hL = sampleH(x - 1, z);
    const float hR = sampleH(x + 1, z);
    const float hD = sampleH(x, z - 1);
    const float hU = sampleH(x, z + 1);
    const float dx = (hR - hL) / (2.0f * cellSizeX);
    const float dz = (hU - hD) / (2.0f * cellSizeZ);
    const float slope = std::sqrt(dx * dx + dz * dz);   // |dH/d(world)|
    return std::atan(slope) * (180.0f / 3.14159265f);
}

void GenerateLayerInstances(FoliageLayerRuntime& rd, const FoliageLayer& layer,
                            const TerrainAsset& asset, float seaLevelY,
                            std::vector<GrassInstanceData>& instances,
                            float& minY, float& maxY)
{
    instances.clear();
    const uint32_t res = asset.resolution;
    if (res < 2) return;
    const float cellSizeX = asset.worldSizeX / static_cast<float>(res - 1);
    const float cellSizeZ = asset.worldSizeZ / static_cast<float>(res - 1);
    const float halfW = asset.worldSizeX * 0.5f;
    const float halfD = asset.worldSizeZ * 0.5f;

    std::mt19937 rng(static_cast<uint32_t>(layer.seed));
    std::uniform_real_distribution<float> uni01(0.0f, 1.0f);

    const int splatChannel = std::clamp(layer.splatChannel, -1, 3);
    // Hard cap to keep memory bounded (200k * 32B = 6.4MB).
    const uint32_t maxInstances = (std::min<uint32_t>)(
        static_cast<uint32_t>(res) * static_cast<uint32_t>(res) * layer.maxPerCell, 200000u);
    instances.reserve(maxInstances / 4);

    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            const size_t cellIdx = static_cast<size_t>(z) * res + x;

            // Splat weight (one channel, or max of all).
            float weight = 0.0f;
            if (splatChannel >= 0 && splatChannel < 4) {
                weight = asset.splatData[cellIdx * 4u + splatChannel] / 255.0f;
            } else {
                weight = (std::max)({
                    asset.splatData[cellIdx * 4u + 0] / 255.0f,
                    asset.splatData[cellIdx * 4u + 1] / 255.0f,
                    asset.splatData[cellIdx * 4u + 2] / 255.0f,
                });
            }
            if (weight < layer.densityThreshold) continue;

            const float heightNorm = asset.heightData[cellIdx];
            if (heightNorm < layer.minAltitudeNorm || heightNorm > layer.maxAltitudeNorm) continue;
            const float baseY = heightNorm * asset.heightScale - asset.heightScale * 0.5f;
            if (baseY < seaLevelY) continue;  // submerged

            // Slope mask
            if (layer.maxSlopeDegrees < 89.5f) {
                const float slopeDeg = SampleSlopeDegrees(asset, static_cast<int>(x), static_cast<int>(z), cellSizeX, cellSizeZ);
                if (slopeDeg > layer.maxSlopeDegrees) continue;
            }

            // Blades in this cell
            const float scaledDensity = weight * layer.densityMultiplier;
            const float bladesF = scaledDensity * static_cast<float>(layer.maxPerCell);
            uint32_t blades = static_cast<uint32_t>(bladesF);
            if (uni01(rng) < (bladesF - static_cast<float>(blades))) blades++;
            if (blades == 0) continue;

            const float cellWorldX = static_cast<float>(x) * cellSizeX - halfW;
            const float cellWorldZ = static_cast<float>(z) * cellSizeZ - halfD;

            for (uint32_t b = 0; b < blades; ++b) {
                if (instances.size() >= maxInstances) break;
                const float jitterX = (uni01(rng) - 0.5f) * cellSizeX;
                const float jitterZ = (uni01(rng) - 0.5f) * cellSizeZ;
                const float sVar = 1.0f + (uni01(rng) * 2.0f - 1.0f) * layer.sizeVariance;
                const float scale = layer.sizeScale * (std::max)(0.05f, sVar);
                const float rotY = uni01(rng) * 6.28318f;
                DirectX::XMFLOAT3 tint = {
                    1.0f + (uni01(rng) * 2.0f - 1.0f) * layer.tintVariance.x,
                    1.0f + (uni01(rng) * 2.0f - 1.0f) * layer.tintVariance.y,
                    1.0f + (uni01(rng) * 2.0f - 1.0f) * layer.tintVariance.z,
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
    if (gc->layers.empty()) gc->EnsureDefaultLayers();

    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(entity);
    if (!tc || !tc->asset) return false;
    TerrainAsset& asset = *tc->asset;
    if (asset.heightData.empty() || asset.splatData.empty()) return false;

    GrassRuntimeData& rd = m_runtimeDataMap[entity];
    rd.windDirection = gc->windDirection;
    rd.drawDistance  = gc->drawDistance;
    rd.layers.resize(gc->layers.size());

    const float seaLevelY = asset.water.enabled ? asset.water.seaLevel : -1e9f;

    float entityMinY =  1e9f;
    float entityMaxY = -1e9f;

    for (size_t li = 0; li < gc->layers.size(); ++li) {
        FoliageLayer& layer = gc->layers[li];
        FoliageLayerRuntime& lr = rd.layers[li];

        if (!layer.enabled) {
            lr.instanceBuffer.reset();
            lr.instanceCount = 0;
            layer.lastInstanceCount = 0;
            continue;
        }

        if (!LoadLayerMesh(lr, layer)) {
            lr.instanceBuffer.reset();
            lr.instanceCount = 0;
            layer.lastInstanceCount = 0;
            continue;
        }

        // Per-layer shader params.
        lr.colorBottom  = layer.colorBottom;
        lr.colorTop     = layer.colorTop;
        lr.useWind      = layer.useWind;
        lr.windStrength = layer.windStrength;
        lr.windSpeed    = layer.windSpeed;

        std::vector<GrassInstanceData> instances;
        float layerMinY =  1e9f, layerMaxY = -1e9f;
        GenerateLayerInstances(lr, layer, asset, seaLevelY, instances, layerMinY, layerMaxY);

        if (instances.empty()) {
            lr.instanceBuffer.reset();
            lr.instanceCount = 0;
            layer.lastInstanceCount = 0;
            LOG_INFO("[Foliage] Layer '%s' produced 0 instances (threshold=%.2f density=%.2f maxPerCell=%u)",
                     layer.name.c_str(), layer.densityThreshold, layer.densityMultiplier, layer.maxPerCell);
            continue;
        }

        lr.instanceBuffer = factory->CreateBuffer(
            static_cast<uint32_t>(instances.size() * sizeof(GrassInstanceData)),
            BufferType::Vertex, instances.data());
        lr.instanceCount = static_cast<uint32_t>(instances.size());
        layer.lastInstanceCount = lr.instanceCount;

        entityMinY = (std::min)(entityMinY, layerMinY);
        entityMaxY = (std::max)(entityMaxY, layerMaxY);

        LOG_INFO("[Foliage] Layer '%s' instances=%u", layer.name.c_str(), lr.instanceCount);
    }

    if (entityMinY > entityMaxY) {
        // No instances anywhere; use degenerate bounds.
        rd.boundsCenter  = { 0.0f, 0.0f, 0.0f };
        rd.boundsExtents = { 0.0f, 0.0f, 0.0f };
    } else {
        rd.boundsCenter  = { 0.0f, (entityMinY + entityMaxY) * 0.5f, 0.0f };
        rd.boundsExtents = {
            asset.worldSizeX * 0.5f,
            (std::max)((entityMaxY - entityMinY) * 0.5f, 0.5f),
            asset.worldSizeZ * 0.5f
        };
    }
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
