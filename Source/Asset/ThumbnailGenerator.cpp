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

// サムネイル描画時のクリアカラー。
static constexpr float CLEAR_R = 0.2f;
static constexpr float CLEAR_G = 0.2f;
static constexpr float CLEAR_B = 0.22f;
static constexpr float CLEAR_A = 1.0f;

// singleton インスタンスを返す。
ThumbnailGenerator& ThumbnailGenerator::Instance() {
    static ThumbnailGenerator instance;
    return instance;
}

// 特別な破棄処理は不要なので default。
ThumbnailGenerator::~ThumbnailGenerator() = default;

// OffscreenRenderer が利用可能かどうかを返す。
bool ThumbnailGenerator::IsAvailable() const {
    return m_offscreen && m_offscreen->IsReady();
}

// 必要になった時だけ内部リソースを初期化する。
// テクスチャプールと共通球モデルを準備して、起動コストを後ろ倒しにする。
bool ThumbnailGenerator::LazyInitialize()
{
    // 初回リクエスト時にだけプールと共通球モデルを用意して、起動コストを抑える。
    m_initialized = false;
    m_texturePool = PreviewTexturePool{};
    m_sphereModel.reset();

    // OffscreenRenderer が無ければ使えない。
    if (!m_offscreen || !m_offscreen->IsReady()) {
        LOG_ERROR("[ThumbnailGenerator] OffscreenRenderer unavailable.");
        return false;
    }

    // リソース生成ファクトリを取得する。
    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        return false;
    }

    // 共通クリアカラーを指定して、サムネイル用テクスチャプールを初期化する。
    const float clearColor[4] = { CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A };
    m_texturePool.Initialize(factory, THUMB_SIZE, THUMB_SIZE,
        TextureFormat::RGBA8_UNORM, TextureFormat::D24_UNORM_S8_UINT,
        clearColor, static_cast<uint32_t>(MAX_CACHE));

    // マテリアルプレビュー用の球モデルを事前取得する。
    m_sphereModel = ResourceManager::Instance().GetModel("Data/Model/sphere/fbx_sphere_001.fbx", 1.0f, true);

    // 共有 depth が作れていれば初期化成功とみなす。
    m_initialized = (m_texturePool.GetSharedDepth() != nullptr);
    if (m_initialized) {
        LOG_INFO("[ThumbnailGenerator] Initialized.");
    }
    return m_initialized;
}

// ThumbnailGenerator に OffscreenRenderer を登録して初期化状態をリセットする。
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

    // 利用不可ならログを出す。
    if (!m_offscreen || !m_offscreen->IsReady()) {
        LOG_ERROR("[ThumbnailGenerator] OffscreenRenderer unavailable.");
    }
}

// モデルサムネイル生成を要求する。
// まだ未生成かつ未キューなら pending queue に積む。
void ThumbnailGenerator::Request(const std::string& modelPath)
{
    // 利用不可なら 1 回だけ警告を出して終了する。
    if (!IsAvailable()) {
        if (!m_loggedUnavailable) {
            LOG_WARN("[ThumbnailGenerator] Thumbnail generation unavailable.");
            m_loggedUnavailable = true;
        }
        return;
    }

    // 遅延初期化がまだなら行う。
    if (!m_initialized && !LazyInitialize()) return;

    // 空パス、キャッシュ済み、キュー済みなら何もしない。
    if (modelPath.empty() || m_cache.count(modelPath) || m_pendingSet.count(modelPath)) return;

    // モデルサムネイル生成要求をキューへ積む。
    m_pendingQueue.push_back({ modelPath, false });
    m_pendingSet.insert(modelPath);
}

// マテリアルサムネイル生成を要求する。
void ThumbnailGenerator::RequestMaterial(const std::string& matPath)
{
    // 利用不可なら何もしない。
    if (!IsAvailable()) return;

    // 遅延初期化がまだなら行う。
    if (!m_initialized && !LazyInitialize()) return;

    // 空パス、キャッシュ済み、キュー済みなら何もしない。
    if (matPath.empty() || m_cache.count(matPath) || m_pendingSet.count(matPath)) return;

    // マテリアルサムネイル生成要求をキューへ積む。
    m_pendingQueue.push_back({ matPath, true });
    m_pendingSet.insert(matPath);
}

// 指定パスのサムネイルキャッシュを無効化し、必要なら再生成要求を入れる。
void ThumbnailGenerator::Invalidate(const std::string& path)
{
    // 既存サムネを捨てるときは、ImGui スロットと texture を即時再利用せず deferred 返却する。
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        uint64_t fenceValue = m_offscreen ? m_offscreen->GetCurrentFenceValue() : 0;

        // ImGui 側の登録を安全に遅延解除する。
        ImGuiRenderer::DeferUnregisterTexture(it->second.get(), fenceValue);

        // DX12 テクスチャなら retire fence を設定して安全に解放する。
        if (auto* dx12Texture = dynamic_cast<DX12Texture*>(it->second.get())) {
            auto* device = Graphics::Instance().GetDX12Device();
            dx12Texture->SetRetireFence(
                device ? device->GetMainFence() : nullptr,
                device ? device->GetMainFenceCurrentValue() : fenceValue);
        }

        // テクスチャプールへ deferred 返却する。
        m_texturePool.DeferRelease(std::move(it->second), fenceValue);
        m_cache.erase(it);
    }

    // LRU 順管理からも除去する。
    for (auto it = m_cacheOrder.begin(); it != m_cacheOrder.end(); ++it) {
        if (*it == path) {
            m_cacheOrder.erase(it);
            break;
        }
    }

    // まだ pending でないなら再生成要求を積む。
    if (!m_pendingSet.count(path)) {
        bool isMat = std::filesystem::path(path).extension().string() == ".mat";
        m_pendingQueue.push_back({ path, isMat });
        m_pendingSet.insert(path);
    }
}

// 現在画面に見えているパス集合を保存する。
// PumpOne で可視アセットを優先処理するために使う。
void ThumbnailGenerator::SetVisiblePaths(const std::unordered_set<std::string>& paths)
{
    m_visiblePaths = paths;
}

// 指定パスのサムネイルテクスチャを取得する。
// キャッシュに無ければ nullptr を返す。
std::shared_ptr<ITexture> ThumbnailGenerator::Get(const std::string& path) const
{
    auto it = m_cache.find(path);
    return (it != m_cache.end()) ? it->second : nullptr;
}

// キャッシュ上限を超えた時、最も古いサムネイルを 1 つ以上追い出す。
void ThumbnailGenerator::EvictOldest()
{
    while (m_cache.size() >= MAX_CACHE && !m_cacheOrder.empty()) {
        const std::string victim = m_cacheOrder.front();
        auto it = m_cache.find(victim);

        if (it != m_cache.end()) {
            uint64_t fenceValue = m_offscreen ? m_offscreen->GetCurrentFenceValue() : 0;

            // ImGui 側の登録解除を遅延する。
            ImGuiRenderer::DeferUnregisterTexture(it->second.get(), fenceValue);

            // DX12 テクスチャなら安全な retire fence を設定する。
            if (auto* dx12Texture = dynamic_cast<DX12Texture*>(it->second.get())) {
                auto* device = Graphics::Instance().GetDX12Device();
                dx12Texture->SetRetireFence(
                    device ? device->GetMainFence() : nullptr,
                    device ? device->GetMainFenceCurrentValue() : fenceValue);
            }

            // テクスチャプールへ返却する。
            m_texturePool.DeferRelease(std::move(it->second), fenceValue);
            m_cache.erase(it);
        }

        m_cacheOrder.pop_front();
    }
}

// pending queue から 1 件だけサムネイル生成を進める。
// 可視アセットを優先し、GPU が空いている時だけ実行する。
void ThumbnailGenerator::PumpOne()
{
    // サムネ生成は 1 フレームに 1 件だけ進め、可視項目を優先する。
    if (!IsAvailable() || m_pendingQueue.empty()) return;
    if (!m_initialized && !LazyInitialize()) return;
    if (!m_offscreen->IsGpuIdle()) return;

    // 完了済み fence までの deferred release を処理する。
    const uint64_t completedFenceValue = m_offscreen->GetCompletedFenceValue();
    m_texturePool.ProcessDeferred(completedFenceValue);

    // まず可視アセットの中から 1 件選ぶ。
    auto selected = m_pendingQueue.end();
    for (auto it = m_pendingQueue.begin(); it != m_pendingQueue.end(); ++it) {
        if (m_visiblePaths.count(it->path) > 0) {
            selected = it;
            break;
        }
    }

    // 可視項目が無ければ先頭を選ぶ。
    if (selected == m_pendingQueue.end()) {
        selected = m_pendingQueue.begin();
    }

    // 再利用可能なキャッシュテクスチャを取得する。
    auto cacheTexture = m_texturePool.Acquire();
    if (!cacheTexture) {
        return;
    }

    // 選ばれた要求を取り出す。
    ThumbnailRequest req = *selected;
    m_pendingQueue.erase(selected);
    m_pendingSet.erase(req.path);

    // モデルかマテリアルかで生成ルートを分ける。
    auto texture = req.isMaterial
        ? GenerateMaterialTexture(req.path, cacheTexture)
        : GenerateTexture(req.path, cacheTexture);

    if (texture) {
        // 成功したら古いキャッシュを追い出してから保存する。
        EvictOldest();
        m_cache[req.path] = texture;
        m_cacheOrder.push_back(req.path);
    }
    else {
        // 失敗したら借りたテクスチャを deferred 返却する。
        uint64_t fenceValue = m_offscreen->GetCurrentFenceValue();
        m_texturePool.DeferRelease(std::move(cacheTexture), fenceValue);
    }
}

// モデル全体が画面に収まるようにカメラ位置と ViewProjection を計算する。
// 細長いエフェクト形状は見やすさ優先で角度補正を行う。
void ThumbnailGenerator::SetupCamera(Model* model, XMFLOAT4X4& outViewProj, XMFLOAT3& outCamPos)
{
    // モデル全体が収まる距離を計算し、細長いエフェクト形状は見やすい角度へ補正する。
    BoundingBox aabb = model->GetWorldBounds();
    XMFLOAT3 center = aabb.Center;
    XMFLOAT3 ex = aabb.Extents;

    // 最大寸法と最小寸法を求める。
    float maxTmp = (ex.x > ex.y) ? ex.x : ex.y;
    float maxDim = (maxTmp > ex.z) ? maxTmp : ex.z;
    float minTmp = (ex.x < ex.y) ? ex.x : ex.y;
    float minDim = (minTmp < ex.z) ? minTmp : ex.z;

    // 非常に薄い形状ならエフェクトっぽいとみなす。
    bool isEffect = (minDim < maxDim * 0.05f);

    // 基本の俯角・方位角。
    float pitch = XMConvertToRadians(25.0f);
    float yaw = XMConvertToRadians(45.0f);

    // 細長い方向に応じて見やすい向きへ補正する。
    if (isEffect) {
        if (ex.y == minDim) pitch = XMConvertToRadians(60.0f);
        else if (ex.z == minDim) yaw = XMConvertToRadians(10.0f);
        else if (ex.x == minDim) yaw = XMConvertToRadians(80.0f);
    }

    // バウンディングボックス extents の長さから半径相当を作る。
    float radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&ex)));
    if (radius < 0.01f) radius = 1.0f;

    // FOV と半径から、モデルが収まるカメラ距離を計算する。
    float fov = XMConvertToRadians(45.0f);
    float distance = (radius / sinf(fov * 0.5f)) * 1.3f;
    float nearZ = 0.01f;
    float farZ = distance * 10.0f;

    // 球面座標的にカメラ位置を決める。
    outCamPos = {
        center.x + distance * cosf(pitch) * sinf(yaw),
        center.y + distance * sinf(pitch),
        center.z - distance * cosf(pitch) * cosf(yaw)
    };

    // LookAt と Perspective を掛けて ViewProjection を作る。
    XMVECTOR eye = XMLoadFloat3(&outCamPos);
    XMVECTOR at = XMLoadFloat3(&center);
    XMVECTOR upV = XMVectorSet(0, 1, 0, 0);
    XMStoreFloat4x4(&outViewProj, XMMatrixLookAtLH(eye, at, upV) * XMMatrixPerspectiveFovLH(fov, 1.0f, nearZ, farZ));
}

// 1 枚のサムネイルを offscreen へ描画する共通処理。
// RT 設定、クリア、viewport 設定、submit までを担当する。
void ThumbnailGenerator::RenderThumbnail(ITexture* target, std::function<void()> setupAndDraw)
{
    // OffscreenRenderer は毎ジョブ BeginJob から始め、前回状態を持ち越さない。
    m_offscreen->BeginJob();
    m_offscreen->ClearExternalRT(target, m_texturePool.GetSharedDepth(), CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A);
    m_offscreen->SetExternalRenderTarget(target, m_texturePool.GetSharedDepth());
    m_offscreen->SetViewport((float)THUMB_SIZE, (float)THUMB_SIZE);

    // 呼び出し側が scene upload や draw を行う。
    setupAndDraw();

    // 最後に GPU へ submit する。
    m_offscreen->SubmitDirect(target);
}

// モデルファイルのサムネイルを生成する。
std::shared_ptr<ITexture> ThumbnailGenerator::GenerateTexture(const std::string& modelPath, std::shared_ptr<ITexture> cacheTex)
{
    // モデルを取得する。
    auto model = ResourceManager::Instance().GetModel(modelPath, 1.0f, true);
    if (!model) return nullptr;

    // 出力先テクスチャが無ければ失敗。
    if (!cacheTex) return nullptr;

    // モデル transform を単位行列へリセットする。
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    model->UpdateTransform(identity);

    // モデル全体が収まるカメラを作る。
    XMFLOAT4X4 viewProj;
    XMFLOAT3 camPos;
    SetupCamera(model.get(), viewProj, camPos);

    // シンプルなライトを置く。
    XMFLOAT3 lightDir = { -0.5f, -0.7f, 0.5f };
    XMFLOAT3 lightColor = { 1.0f, 1.0f, 1.0f };

    auto modelRes = model->GetModelResource();

    // 元のマテリアルカラーを保存する。
    std::vector<XMFLOAT4> savedColors;
    for (int i = 0; i < modelRes->GetMeshCount(); ++i) {
        auto* mesh = modelRes->GetMeshResource(i);
        savedColors.push_back(mesh->material.color);

        // テクスチャが無いメッシュは見分けやすいマゼンタ色へ一時的に置き換える。
        if (!mesh->material.diffuseMap) {
            mesh->material.color = { 1.0f, 0.0f, 1.0f, 1.0f };
        }
    }

    // 実際に offscreen へ描画する。
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

    // 一時変更したマテリアルカラーを元へ戻す。
    for (int i = 0; i < modelRes->GetMeshCount(); ++i) {
        modelRes->GetMeshResource(i)->material.color = savedColors[i];
    }

    return cacheTex;
}

// マテリアルアセットのサムネイルを生成する。
// 共通球モデルへマテリアルを当てて描画する。
std::shared_ptr<ITexture> ThumbnailGenerator::GenerateMaterialTexture(const std::string& matPath, std::shared_ptr<ITexture> cacheTex)
{
    // 共通球モデルが無ければ失敗。
    if (!m_sphereModel) return nullptr;

    // マテリアルアセットを取得する。
    auto material = ResourceManager::Instance().GetMaterial(matPath);
    if (!material) return nullptr;

    // 出力先テクスチャが無ければ失敗。
    if (!cacheTex) return nullptr;

    // 球モデル transform を単位行列へリセットする。
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    m_sphereModel->UpdateTransform(identity);

    // 球モデルの全マテリアルへ対象マテリアルを反映する。
    auto& meshMaterials = m_sphereModel->GetMaterialss();
    for (auto& mat : meshMaterials) {
        mat.color = material->baseColor;
        mat.metallicFactor = material->metallic;
        mat.roughnessFactor = material->roughness;
        mat.diffuseMap = ResourceManager::Instance().GetTexture(material->diffuseTexturePath);
        mat.normalMap = ResourceManager::Instance().GetTexture(material->normalTexturePath);

        // metallic/roughness 共通テクスチャを設定する。
        if (!material->metallicRoughnessTexturePath.empty()) {
            mat.metallicMap = ResourceManager::Instance().GetTexture(material->metallicRoughnessTexturePath);
            mat.roughnessMap = mat.metallicMap;
        }
        else {
            mat.metallicMap = nullptr;
            mat.roughnessMap = nullptr;
        }

        mat.emissiveMap = ResourceManager::Instance().GetTexture(material->emissiveTexturePath);
    }

    // 材質反映後の transform を再更新する。
    m_sphereModel->UpdateTransform(identity);

    // 球全体が収まるカメラを作る。
    XMFLOAT4X4 viewProj;
    XMFLOAT3 camPos;
    SetupCamera(m_sphereModel.get(), viewProj, camPos);

    // 少し強めのライトを置く。
    XMFLOAT3 lightDir = { -0.5f, -0.7f, 0.5f };
    XMFLOAT3 lightColor = { 3.0f, 3.0f, 3.0f };

    // 実際に offscreen へ描画する。
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