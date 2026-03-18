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
// ResourceNode: グラフ内の1リソースを表す
// ============================================================
struct ResourceNode {
    std::string name;
    TextureDesc desc;
    uint16_t version = 0;

    // 寿命追跡 (Compile で計算)
    uint16_t firstPassIndex = UINT16_MAX;
    uint16_t lastPassIndex  = 0;

    // このバージョンを生成したパス
    uint16_t producerPassIndex = UINT16_MAX;

    // 物理リソース
    std::unique_ptr<ITexture> ownedTexture;   // トランジェント用
    ITexture* externalTexture = nullptr;       // imported 用
    bool isImported = false;

    // カリング用参照カウント
    uint32_t refCount = 0;

    ITexture* GetPhysical() const {
        return isImported ? externalTexture : ownedTexture.get();
    }
};

// ============================================================
// PassNode: グラフ内の1パスを表す
// ============================================================
struct PassNode {
    std::string name;
    uint16_t passIndex = 0;
    IRenderPass* renderPass = nullptr;

    // DAG エッジ
    std::vector<ResourceHandle> reads;
    std::vector<ResourceHandle> writes;

    // カリング
    uint32_t refCount = 0;
    bool culled = false;
    bool hasSideEffects = false;
};

// ============================================================
// FrameGraph: Frostbite-style FrameGraph
// Setup → Compile → Execute の3フェーズ
// ============================================================
class FrameGraph {
public:
    FrameGraph() = default;
    ~FrameGraph() = default;

    // パス登録
    void AddPass(IRenderPass* pass);

    // 外部リソースのインポート (backbuffer, shadow map 等)
    ResourceHandle ImportTexture(const std::string& name, ITexture* texture);

    // 3フェーズ実行
    void Execute(const RenderQueue& queue, RenderContext& rc);

    // リソースハンドルから物理テクスチャを取得
    ITexture* GetPhysicalTexture(ResourceHandle handle) const;

    // デバッグ: Graphviz DOT形式でグラフを出力
    std::string DumpGraphviz() const;

private:
    void Setup();
    void Compile();
    void ExecutePasses(const RenderQueue& queue, RenderContext& rc);

    // Compile サブステップ
    void BuildDAG();
    void CullPasses();
    void CalculateLifetimes();
    void TopologicalSort();

private:
    std::vector<PassNode>     m_passNodes;
    std::vector<ResourceNode> m_resourceNodes;

    // Blackboard: パス間でリソースハンドルを共有
    std::unordered_map<std::string, ResourceHandle> m_blackboard;

    // Compile 結果
    std::vector<uint16_t>                m_executionOrder;
    std::vector<std::vector<uint16_t>>   m_adjacency;   // passIndex → 後続パスリスト
    std::vector<uint16_t>                m_inDegree;

    // リソースプール (フレーム間で永続)
    RenderGraphResourcePool m_resourcePool;
    uint64_t m_frameCount = 0;

    // BuilderImpl はフレンド
    class BuilderImpl;
    friend class BuilderImpl;
    friend class FrameGraphResources;
};
