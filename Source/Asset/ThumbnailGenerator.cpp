#include "ThumbnailGenerator.h"
#include "Render/OffscreenRenderer.h"
#include "Graphics.h"
#include "System/ResourceManager.h"
#include "Material/MaterialAsset.h"
#include "Model/Model.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"
#include "RHI/DX12/DX12Texture.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "Console/Logger.h"
#include "ImGuiRenderer.h"
#include <cmath>
#include <filesystem>

using namespace DirectX;

static constexpr float CLEAR_R = 0.2f;
static constexpr float CLEAR_G = 0.2f;
static constexpr float CLEAR_B = 0.22f;
static constexpr float CLEAR_A = 1.0f;

ThumbnailGenerator& ThumbnailGenerator::Instance() {
    static ThumbnailGenerator instance;
    return instance;
}

ThumbnailGenerator::~ThumbnailGenerator() = default;

bool ThumbnailGenerator::IsAvailable() const {
    return m_offscreen && m_offscreen->IsReady();
}

bool ThumbnailGenerator::LazyInitialize()
{
    m_initialized = false;
    m_texturePool = PreviewTexturePool{};
    m_sphereModel.reset();

    if (!m_offscreen || !m_offscreen->IsReady()) {
        LOG_ERROR("[ThumbnailGenerator] OffscreenRenderer unavailable.");
        return false;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        return false;
    }

    const float clearColor[4] = { CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A };
    m_texturePool.Initialize(factory, THUMB_SIZE, THUMB_SIZE,
        TextureFormat::RGBA8_UNORM, TextureFormat::D24_UNORM_S8_UINT,
        clearColor, static_cast<uint32_t>(MAX_CACHE));

    m_sphereModel = ResourceManager::Instance().GetModel("Data/Model/sphere/fbx_sphere_001.fbx");
    m_initialized = (m_texturePool.GetSharedDepth() != nullptr);
    if (m_initialized) {
        LOG_INFO("[ThumbnailGenerator] Initialized.");
    }
    return m_initialized;
}

void ThumbnailGenerator::Initialize(OffscreenRenderer* offscreen)
{
    m_offscreen = offscreen;
    m_loggedUnavailable = false;
    m_cache.clear();
    m_cacheOrder.clear();
    m_pendingQueue.clear();
    m_pendingSet.clear();
    m_visiblePaths.clear();
    m_initialized = false;
    m_texturePool = PreviewTexturePool{};

    if (!m_offscreen || !m_offscreen->IsReady()) {
        LOG_ERROR("[ThumbnailGenerator] OffscreenRenderer unavailable.");
    }
}

void ThumbnailGenerator::Request(const std::string& modelPath)
{
    if (!IsAvailable()) {
        if (!m_loggedUnavailable) {
            LOG_WARN("[ThumbnailGenerator] Thumbnail generation unavailable.");
            m_loggedUnavailable = true;
        }
        return;
    }
    if (!m_initialized && !LazyInitialize()) return;
    if (modelPath.empty() || m_cache.count(modelPath) || m_pendingSet.count(modelPath)) return;

    m_pendingQueue.push_back({ modelPath, false });
    m_pendingSet.insert(modelPath);
}

void ThumbnailGenerator::RequestMaterial(const std::string& matPath)
{
    if (!IsAvailable()) return;
    if (!m_initialized && !LazyInitialize()) return;
    if (matPath.empty() || m_cache.count(matPath) || m_pendingSet.count(matPath)) return;

    m_pendingQueue.push_back({ matPath, true });
    m_pendingSet.insert(matPath);
}

void ThumbnailGenerator::Invalidate(const std::string& path)
{
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        uint64_t fenceValue = m_offscreen ? m_offscreen->GetCurrentFenceValue() : 0;
        ImGuiRenderer::DeferUnregisterTexture(it->second.get(), fenceValue);
        if (auto* dx12Texture = dynamic_cast<DX12Texture*>(it->second.get())) {
            auto* device = Graphics::Instance().GetDX12Device();
            dx12Texture->SetRetireFence(
                device ? device->GetMainFence() : nullptr,
                device ? device->GetMainFenceCurrentValue() : fenceValue);
        }
        m_texturePool.DeferRelease(std::move(it->second), fenceValue);
        m_cache.erase(it);
    }
    for (auto it = m_cacheOrder.begin(); it != m_cacheOrder.end(); ++it) {
        if (*it == path) { m_cacheOrder.erase(it); break; }
    }
    if (!m_pendingSet.count(path)) {
        bool isMat = std::filesystem::path(path).extension().string() == ".mat";
        m_pendingQueue.push_back({ path, isMat });
        m_pendingSet.insert(path);
    }
}

void ThumbnailGenerator::SetVisiblePaths(const std::unordered_set<std::string>& paths)
{
    m_visiblePaths = paths;
}

std::shared_ptr<ITexture> ThumbnailGenerator::Get(const std::string& path) const
{
    auto it = m_cache.find(path);
    return (it != m_cache.end()) ? it->second : nullptr;
}

void ThumbnailGenerator::EvictOldest()
{
    while (m_cache.size() >= MAX_CACHE && !m_cacheOrder.empty()) {
        const std::string victim = m_cacheOrder.front();
        auto it = m_cache.find(victim);
        if (it != m_cache.end()) {
            uint64_t fenceValue = m_offscreen ? m_offscreen->GetCurrentFenceValue() : 0;
            ImGuiRenderer::DeferUnregisterTexture(it->second.get(), fenceValue);
            if (auto* dx12Texture = dynamic_cast<DX12Texture*>(it->second.get())) {
                auto* device = Graphics::Instance().GetDX12Device();
                dx12Texture->SetRetireFence(
                    device ? device->GetMainFence() : nullptr,
                    device ? device->GetMainFenceCurrentValue() : fenceValue);
            }
            m_texturePool.DeferRelease(std::move(it->second), fenceValue);
            m_cache.erase(it);
        }
        m_cacheOrder.pop_front();
    }
}

void ThumbnailGenerator::PumpOne()
{
    if (!IsAvailable() || m_pendingQueue.empty()) return;
    if (!m_initialized && !LazyInitialize()) return;
    if (!m_offscreen->IsGpuIdle()) return;

    const uint64_t completedFenceValue = m_offscreen->GetCompletedFenceValue();
    m_texturePool.ProcessDeferred(completedFenceValue);

    auto selected = m_pendingQueue.end();
    for (auto it = m_pendingQueue.begin(); it != m_pendingQueue.end(); ++it) {
        if (m_visiblePaths.count(it->path) > 0) {
            selected = it;
            break;
        }
    }
    if (selected == m_pendingQueue.end()) {
        selected = m_pendingQueue.begin();
    }

    auto cacheTexture = m_texturePool.Acquire();
    if (!cacheTexture) {
        return;
    }

    ThumbnailRequest req = *selected;
    m_pendingQueue.erase(selected);
    m_pendingSet.erase(req.path);

    auto texture = req.isMaterial
        ? GenerateMaterialTexture(req.path, cacheTexture)
        : GenerateTexture(req.path, cacheTexture);
    if (texture) {
        EvictOldest();
        m_cache[req.path] = texture;
        m_cacheOrder.push_back(req.path);
    } else {
        uint64_t fenceValue = m_offscreen->GetCurrentFenceValue();
        m_texturePool.DeferRelease(std::move(cacheTexture), fenceValue);
    }
}

void ThumbnailGenerator::SetupCamera(Model* model, XMFLOAT4X4& outViewProj, XMFLOAT3& outCamPos)
{
    BoundingBox aabb = model->GetWorldBounds();
    XMFLOAT3 center = aabb.Center;
    XMFLOAT3 ex = aabb.Extents;

    float maxTmp = (ex.x > ex.y) ? ex.x : ex.y;
    float maxDim = (maxTmp > ex.z) ? maxTmp : ex.z;
    float minTmp = (ex.x < ex.y) ? ex.x : ex.y;
    float minDim = (minTmp < ex.z) ? minTmp : ex.z;

    bool isEffect = (minDim < maxDim * 0.05f);
    float pitch = XMConvertToRadians(25.0f);
    float yaw = XMConvertToRadians(45.0f);
    if (isEffect) {
        if (ex.y == minDim) pitch = XMConvertToRadians(60.0f);
        else if (ex.z == minDim) yaw = XMConvertToRadians(10.0f);
        else if (ex.x == minDim) yaw = XMConvertToRadians(80.0f);
    }

    float radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&ex)));
    if (radius < 0.01f) radius = 1.0f;

    float fov = XMConvertToRadians(45.0f);
    float distance = (radius / sinf(fov * 0.5f)) * 1.3f;
    float nearZ = 0.01f;
    float farZ = distance * 10.0f;

    outCamPos = {
        center.x + distance * cosf(pitch) * sinf(yaw),
        center.y + distance * sinf(pitch),
        center.z - distance * cosf(pitch) * cosf(yaw)
    };

    XMVECTOR eye = XMLoadFloat3(&outCamPos);
    XMVECTOR at  = XMLoadFloat3(&center);
    XMVECTOR upV = XMVectorSet(0, 1, 0, 0);
    XMStoreFloat4x4(&outViewProj, XMMatrixLookAtLH(eye, at, upV) * XMMatrixPerspectiveFovLH(fov, 1.0f, nearZ, farZ));
}

void ThumbnailGenerator::RenderThumbnail(ITexture* target, std::function<void()> setupAndDraw)
{
    m_offscreen->BeginJob();
    m_offscreen->ClearExternalRT(target, m_texturePool.GetSharedDepth(), CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A);
    m_offscreen->SetExternalRenderTarget(target, m_texturePool.GetSharedDepth());
    m_offscreen->SetViewport((float)THUMB_SIZE, (float)THUMB_SIZE);

    setupAndDraw();

    m_offscreen->SubmitDirect(target);
}

std::shared_ptr<ITexture> ThumbnailGenerator::GenerateTexture(const std::string& modelPath, std::shared_ptr<ITexture> cacheTex)
{
    auto model = ResourceManager::Instance().GetModel(modelPath);
    if (!model) return nullptr;

    if (!cacheTex) return nullptr;

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    model->UpdateTransform(identity);

    XMFLOAT4X4 viewProj;
    XMFLOAT3 camPos;
    SetupCamera(model.get(), viewProj, camPos);

    XMFLOAT3 lightDir   = { -0.5f, -0.7f, 0.5f };
    XMFLOAT3 lightColor = { 1.0f, 1.0f, 1.0f };

    auto modelRes = model->GetModelResource();

    std::vector<XMFLOAT4> savedColors;
    for (int i = 0; i < modelRes->GetMeshCount(); ++i) {
        auto* mesh = modelRes->GetMeshResource(i);
        savedColors.push_back(mesh->material.color);
        if (!mesh->material.diffuseMap) {
            mesh->material.color = { 1.0f, 0.0f, 1.0f, 1.0f };
        }
    }

    RenderThumbnail(cacheTex.get(), [&]() {
        m_offscreen->UploadScene(viewProj, camPos, lightDir, lightColor,
            (float)THUMB_SIZE, (float)THUMB_SIZE);
        m_offscreen->BindScene();
        m_offscreen->BindSampler();

        XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_offscreen->GetModelRenderer().Draw(
            ShaderId::Phong, modelRes,
            identity, identity, white, 0.0f, 1.0f, 0.0f,
            nullptr, BlendState::Opaque, DepthState::TestAndWrite, RasterizerState::SolidCullNone);
    });

    for (int i = 0; i < modelRes->GetMeshCount(); ++i) {
        modelRes->GetMeshResource(i)->material.color = savedColors[i];
    }

    return cacheTex;
}

std::shared_ptr<ITexture> ThumbnailGenerator::GenerateMaterialTexture(const std::string& matPath, std::shared_ptr<ITexture> cacheTex)
{
    if (!m_sphereModel) return nullptr;

    auto material = ResourceManager::Instance().GetMaterial(matPath);
    if (!material) return nullptr;

    if (!cacheTex) return nullptr;

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    m_sphereModel->UpdateTransform(identity);

    auto& meshMaterials = m_sphereModel->GetMaterialss();
    for (auto& mat : meshMaterials) {
        mat.color = material->baseColor;
        mat.metallicFactor = material->metallic;
        mat.roughnessFactor = material->roughness;
        mat.diffuseMap = ResourceManager::Instance().GetTexture(material->diffuseTexturePath);
        mat.normalMap = ResourceManager::Instance().GetTexture(material->normalTexturePath);
        if (!material->metallicRoughnessTexturePath.empty()) {
            mat.metallicMap = ResourceManager::Instance().GetTexture(material->metallicRoughnessTexturePath);
            mat.roughnessMap = mat.metallicMap;
        } else {
            mat.metallicMap = nullptr;
            mat.roughnessMap = nullptr;
        }
        mat.emissiveMap = ResourceManager::Instance().GetTexture(material->emissiveTexturePath);
    }
    m_sphereModel->UpdateTransform(identity);

    XMFLOAT4X4 viewProj;
    XMFLOAT3 camPos;
    SetupCamera(m_sphereModel.get(), viewProj, camPos);

    XMFLOAT3 lightDir   = { -0.5f, -0.7f, 0.5f };
    XMFLOAT3 lightColor = { 3.0f, 3.0f, 3.0f };

    RenderThumbnail(cacheTex.get(), [&]() {
        m_offscreen->UploadScene(viewProj, camPos, lightDir, lightColor,
            (float)THUMB_SIZE, (float)THUMB_SIZE);
        m_offscreen->BindScene();
        m_offscreen->BindSampler();

        auto modelRes = m_sphereModel->GetModelResource();
        m_offscreen->GetModelRenderer().Draw(
            ShaderId::Phong, modelRes,
            identity, identity,
            material->baseColor, material->metallic, material->roughness, material->emissive,
            material.get(), BlendState::Opaque, DepthState::TestAndWrite, RasterizerState::SolidCullNone);
    });

    return cacheTex;
}
