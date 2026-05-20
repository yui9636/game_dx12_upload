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

// FrameGraph に登録された texture の仮想ノード。
// imported はバックバッファなど外部所有、ownedTexture は pool から取得した一時 texture。
struct ResourceNode {
    std::string name;
    TextureDesc desc;
    uint16_t version = 0;

    uint16_t firstPassIndex = UINT16_MAX;
    uint16_t lastPassIndex  = 0;

    uint16_t producerPassIndex = UINT16_MAX;

    std::unique_ptr<ITexture> ownedTexture;
    ITexture* externalTexture = nullptr;       // imported 用。所有権は FrameGraph へ移らない。
    bool isImported = false;

    // CullPasses で生存判定に使う参照数。
    uint32_t refCount = 0;

    ITexture* GetPhysical() const {
        return isImported ? externalTexture : ownedTexture.get();
    }
};

// 1 つの IRenderPass と、その pass が宣言した read/write resource の集合。
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
// FrameGraph は pass 依存関係、resource lifetime、一時 texture pool をまとめて扱う。
class FrameGraph {
public:
    FrameGraph() = default;
    ~FrameGraph() = default;

    void SetFrameIndex(uint64_t frameIndex) { m_frameCount = frameIndex; }

    // Execute 前に実行対象 pass を追加する。所有権は呼び出し側が保持する。
    void AddPass(IRenderPass* pass);

    // 既存 texture を FrameGraph の Blackboard に登録する。バックバッファなどで使う。
    ResourceHandle ImportTexture(const std::string& name, ITexture* texture);

    // Setup -> Compile -> ExecutePasses を実行し、使い終わった一時 texture を pool に戻す。
    void Execute(const RenderQueue& queue, RenderContext& rc);

    ITexture* GetPhysicalTexture(ResourceHandle handle) const;

    // デバッグ用に pass/resource の関係を Graphviz 形式で出力する。
    std::string DumpGraphviz() const;

private:
    // 各 pass の Setup を呼び、read/write 宣言を集める。
    void Setup(const RenderContext& rc);
    // DAG 構築、未使用 pass culling、実行順、resource lifetime を確定する。
    void Compile();
    // 確定した順序で resource を確保し、barrier を張って pass を実行する。
    void ExecutePasses(const RenderQueue& queue, RenderContext& rc);

    void BuildDAG();
    void CullPasses();
    void CalculateLifetimes();
    void TopologicalSort();

private:
    std::vector<PassNode>     m_passNodes;
    std::vector<ResourceNode> m_resourceNodes;

    std::unordered_map<std::string, ResourceHandle> m_blackboard;

    // Compile 結果。m_executionOrder は culled されなかった pass の実行順。
    std::vector<uint16_t>                m_executionOrder;
    std::vector<std::vector<uint16_t>>   m_adjacency;
    std::vector<uint16_t>                m_inDegree;

    // transient texture の再利用 pool。frameCount を使って古い texture を破棄する。
    RenderGraphResourcePool m_resourcePool;
    uint64_t m_frameCount = 0;

    class BuilderImpl;
    friend class BuilderImpl;
    friend class FrameGraphResources;
};
