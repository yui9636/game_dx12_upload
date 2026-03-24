#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include "FrameGraphTypes.h"
#include "FrameGraphBuilder.h"
#include "RenderGraphResourcePool.h"

class IRenderPass;
class ITexture;
class ICommandList;
class RenderQueue;
struct RenderContext;
class FrameGraphResources;

// ============================================================
// ============================================================
struct ResourceNode {
    std::string name;
    TextureDesc desc;
    uint16_t version = 0;

    uint16_t firstPassIndex = UINT16_MAX;
    uint16_t lastPassIndex  = 0;

    uint16_t producerPassIndex = UINT16_MAX;

    std::unique_ptr<ITexture> ownedTexture;
    ITexture* externalTexture = nullptr;       // imported 用
    bool isImported = false;

    uint32_t refCount = 0;

    ITexture* GetPhysical() const {
        return isImported ? externalTexture : ownedTexture.get();
    }
};

// ============================================================
// ============================================================
struct PassNode {
    std::string name;
    uint16_t passIndex = 0;
    IRenderPass* renderPass = nullptr;

    std::vector<ResourceHandle> reads;
    std::vector<ResourceHandle> writes;

    uint32_t refCount = 0;
    bool culled = false;
    bool hasSideEffects = false;
};

// ============================================================
// FrameGraph: Frostbite-style FrameGraph
// ============================================================
class FrameGraph {
public:
    FrameGraph() = default;
    ~FrameGraph() = default;

    void AddPass(IRenderPass* pass);

    ResourceHandle ImportTexture(const std::string& name, ITexture* texture);

    void Execute(const RenderQueue& queue, RenderContext& rc);

    ITexture* GetPhysicalTexture(ResourceHandle handle) const;

    std::string DumpGraphviz() const;

private:
    void Setup();
    void Compile();
    void ExecutePasses(const RenderQueue& queue, RenderContext& rc);

    void BuildDAG();
    void CullPasses();
    void CalculateLifetimes();
    void TopologicalSort();

private:
    std::vector<PassNode>     m_passNodes;
    std::vector<ResourceNode> m_resourceNodes;

    std::unordered_map<std::string, ResourceHandle> m_blackboard;

    // Compile 結果
    std::vector<uint16_t>                m_executionOrder;
    std::vector<std::vector<uint16_t>>   m_adjacency;
    std::vector<uint16_t>                m_inDegree;

    RenderGraphResourcePool m_resourcePool;
    uint64_t m_frameCount = 0;

    class BuilderImpl;
    friend class BuilderImpl;
    friend class FrameGraphResources;
};
