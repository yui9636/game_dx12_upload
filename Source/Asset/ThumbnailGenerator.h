#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <DirectXMath.h>
#include "Entity/Entity.h"
#include "Model/ModelRenderer.h"

class FrameBuffer;
class Registry;
class ICommandList;
class DX12RootSignature;
struct RenderContext;
struct RenderQueue;
class ITexture;

class ThumbnailGenerator {
public:
    static ThumbnailGenerator& Instance();
    void Initialize();
    bool IsAvailable() const { return m_available; }

    void Request(const std::string& modelPath);
    void PumpOne();
    std::shared_ptr<ITexture> Get(const std::string& modelPath) const;

private:
    ThumbnailGenerator() = default;
    ~ThumbnailGenerator();

    RenderContext BuildThumbnailRenderContext(FrameBuffer* targetBuffer);
    std::shared_ptr<ITexture> GenerateTexture(const std::string& modelPath);

    std::unique_ptr<ICommandList> m_commandList;
    std::unique_ptr<DX12RootSignature> m_dx12RootSignature;

    std::unique_ptr<ModelRenderer> m_renderer;

    std::unique_ptr<Registry> m_thumbRegistry;
    EntityID m_thumbCamera = Entity::NULL_ID;

    std::unordered_map<std::string, std::shared_ptr<ITexture>> m_cache;
    std::deque<std::string> m_pendingQueue;
    std::unordered_set<std::string> m_pendingSet;
    bool m_available = false;
    bool m_loggedUnavailable = false;
};
