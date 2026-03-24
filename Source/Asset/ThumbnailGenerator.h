#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <functional>
#include <DirectXMath.h>
#include "Entity/Entity.h"
#include "Render/PreviewTexturePool.h"

class Registry;
class ITexture;
class Model;
class OffscreenRenderer;

struct ThumbnailRequest {
    std::string path;
    bool isMaterial = false;
};

class ThumbnailGenerator {
public:
    static ThumbnailGenerator& Instance();
    void Initialize(OffscreenRenderer* offscreen);
    bool IsAvailable() const;

    void Request(const std::string& modelPath);
    void RequestMaterial(const std::string& matPath);
    void Invalidate(const std::string& path);
    void PumpOne();
    bool HasPending() const { return !m_pendingQueue.empty(); }
    std::shared_ptr<ITexture> Get(const std::string& path) const;
    void SetVisiblePaths(const std::unordered_set<std::string>& paths);

private:
    ThumbnailGenerator() = default;
    ~ThumbnailGenerator();

    bool LazyInitialize();
    void SetupCamera(Model* model, DirectX::XMFLOAT4X4& outViewProj, DirectX::XMFLOAT3& outCamPos);
    std::shared_ptr<ITexture> GenerateTexture(const std::string& modelPath, std::shared_ptr<ITexture> target);
    std::string ResolvePreviewModelPath(const std::string& modelPath) const;
    std::shared_ptr<ITexture> GenerateMaterialTexture(const std::string& matPath, std::shared_ptr<ITexture> target);
    void RenderThumbnail(ITexture* target, std::function<void()> setupAndDraw);
    void EvictOldest();

    static constexpr size_t MAX_CACHE = 128;
    static constexpr int THUMB_SIZE = 128;

    OffscreenRenderer* m_offscreen = nullptr;
    std::shared_ptr<Model> m_sphereModel;
    PreviewTexturePool m_texturePool;

    std::unordered_map<std::string, std::shared_ptr<ITexture>> m_cache;
    std::deque<std::string> m_cacheOrder;  // LRU eviction
    std::deque<ThumbnailRequest> m_pendingQueue;
    std::unordered_set<std::string> m_pendingSet;
    std::unordered_set<std::string> m_visiblePaths;
    bool m_initialized = false;
    bool m_loggedUnavailable = false;
};

