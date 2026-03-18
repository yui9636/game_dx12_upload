#include "FrameGraph.h"
#include "FrameGraphResources.h"
#include "RenderPass/IRenderPass.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "Console/Profiler.h"
#include "Console/Logger.h"
#include <queue>
#include <algorithm>
#include <sstream>
#include <cassert>

// ============================================================
// FrameGraphResources 実装
// ============================================================
ITexture* FrameGraphResources::GetTexture(ResourceHandle handle) {
    return m_graph.GetPhysicalTexture(handle);
}

// ============================================================
// BuilderImpl: 各パスの Setup() 中に使用されるビルダー
// ============================================================
class FrameGraph::BuilderImpl : public FrameGraphBuilder {
public:
    BuilderImpl(FrameGraph& graph, PassNode& passNode)
        : m_graph(graph), m_passNode(passNode) {
    }

    ResourceHandle CreateTexture(const std::string& name, const TextureDesc& desc) override {
        ResourceHandle handle;
        handle.index = static_cast<uint16_t>(m_graph.m_resourceNodes.size());
        handle.version = 0;

        ResourceNode node;
        node.name = name;
        node.desc = desc;
        node.version = 0;
        node.isImported = false;
        m_graph.m_resourceNodes.push_back(std::move(node));

        return handle;
    }

    ResourceHandle Read(ResourceHandle input) override {
        if (!input.IsValid()) return input;
        if (input.index >= m_graph.m_resourceNodes.size()) return input;

        m_passNode.reads.push_back(input);
        return input;
    }

    ResourceHandle Write(ResourceHandle input) override {
        if (!input.IsValid()) return input;
        if (input.index >= m_graph.m_resourceNodes.size()) return input;

        // バージョンをインクリメント
        ResourceNode& node = m_graph.m_resourceNodes[input.index];
        node.version++;
        node.producerPassIndex = m_passNode.passIndex;

        ResourceHandle newHandle;
        newHandle.index = input.index;
        newHandle.version = node.version;

        m_passNode.writes.push_back(newHandle);

        return newHandle;
    }

    void RegisterHandle(const std::string& name, ResourceHandle handle) override {
        m_graph.m_blackboard[name] = handle;
    }

    ResourceHandle GetHandle(const std::string& name) const override {
        auto it = m_graph.m_blackboard.find(name);
        return (it != m_graph.m_blackboard.end()) ? it->second : ResourceHandle{};
    }

private:
    FrameGraph& m_graph;
    PassNode& m_passNode;
};

// ============================================================
// FrameGraph 実装
// ============================================================

void FrameGraph::AddPass(IRenderPass* pass) {
    if (!pass) return;

    PassNode node;
    node.name = pass->GetName();
    node.passIndex = static_cast<uint16_t>(m_passNodes.size());
    node.renderPass = pass;
    node.hasSideEffects = pass->HasSideEffects();
    m_passNodes.push_back(std::move(node));
}

ResourceHandle FrameGraph::ImportTexture(const std::string& name, ITexture* texture) {
    ResourceHandle handle;
    handle.index = static_cast<uint16_t>(m_resourceNodes.size());
    handle.version = 0;

    ResourceNode node;
    node.name = name;
    node.isImported = true;
    node.externalTexture = texture;
    node.version = 0;
    // imported リソースは初めから producer がいる扱い
    node.producerPassIndex = UINT16_MAX;
    m_resourceNodes.push_back(std::move(node));

    m_blackboard[name] = handle;
    return handle;
}

void FrameGraph::Execute(const RenderQueue& queue, RenderContext& rc) {
    Setup();
    Compile();
    ExecutePasses(queue, rc);

    // トランジェントリソースをプールに返却
    for (auto& node : m_resourceNodes) {
        if (!node.isImported && node.ownedTexture) {
            m_resourcePool.ReleaseTexture(node.desc, std::move(node.ownedTexture), m_frameCount);
        }
    }
    m_resourcePool.Tick(m_frameCount);
    m_frameCount++;

    // フレームごとにクリア
    m_passNodes.clear();
    m_resourceNodes.clear();
    m_blackboard.clear();
    m_executionOrder.clear();
    m_adjacency.clear();
    m_inDegree.clear();
}

// ============================================================
// Phase 1: Setup
// ============================================================
void FrameGraph::Setup() {
    m_resourceNodes.reserve(32);

    for (size_t i = 0; i < m_passNodes.size(); ++i) {
        BuilderImpl builder(*this, m_passNodes[i]);
        m_passNodes[i].renderPass->Setup(builder);
    }
}

// ============================================================
// Phase 2: Compile
// ============================================================
void FrameGraph::Compile() {
    BuildDAG();
    CullPasses();
    TopologicalSort();
    CalculateLifetimes();
}

// Step 2a: DAG 構築
void FrameGraph::BuildDAG() {
    const size_t passCount = m_passNodes.size();
    m_adjacency.resize(passCount);
    m_inDegree.resize(passCount, 0);

    // 各リソースの各バージョンについて、producer パスを見つけるマップ
    // key: (resourceIndex, version) → producerPassIndex
    std::unordered_map<uint32_t, uint16_t> producerMap;

    for (auto& pass : m_passNodes) {
        for (const auto& w : pass.writes) {
            uint32_t key = (static_cast<uint32_t>(w.index) << 16) | w.version;
            producerMap[key] = pass.passIndex;
        }
    }

    // imported リソース (version=0) は外部 producer 扱い
    for (size_t i = 0; i < m_resourceNodes.size(); ++i) {
        if (m_resourceNodes[i].isImported) {
            uint32_t key = (static_cast<uint32_t>(i) << 16) | 0;
            // 外部なので producerMap には登録しない (エッジ不要)
        }
    }

    // Read エッジ: reads のハンドルを見て、そのバージョンの producer → この pass
    for (auto& pass : m_passNodes) {
        for (const auto& r : pass.reads) {
            uint32_t key = (static_cast<uint32_t>(r.index) << 16) | r.version;
            auto it = producerMap.find(key);
            if (it != producerMap.end()) {
                uint16_t producerIdx = it->second;
                if (producerIdx != pass.passIndex) {
                    m_adjacency[producerIdx].push_back(pass.passIndex);
                    m_inDegree[pass.passIndex]++;
                }
            }
        }
    }

    // Write-on-existing (同一リソースの前バージョンの producer → この pass)
    for (auto& pass : m_passNodes) {
        for (const auto& w : pass.writes) {
            if (w.version > 1) {
                // 前バージョンの producer への暗黙の依存
                uint32_t prevKey = (static_cast<uint32_t>(w.index) << 16) | (w.version - 1);
                auto it = producerMap.find(prevKey);
                if (it != producerMap.end()) {
                    uint16_t prevProducer = it->second;
                    if (prevProducer != pass.passIndex) {
                        m_adjacency[prevProducer].push_back(pass.passIndex);
                        m_inDegree[pass.passIndex]++;
                    }
                }
            }
        }
    }
}

// Step 2b: パスカリング (逆方向 refCount 伝播)
void FrameGraph::CullPasses() {
    // シード: hasSideEffects を持つパスを参照済みにする
    std::queue<uint16_t> workQueue;

    for (auto& pass : m_passNodes) {
        if (pass.hasSideEffects) {
            pass.refCount = 1;
            workQueue.push(pass.passIndex);
        }
    }

    // 逆方向に伝播: 参照されたパスの reads の producer も参照する
    // producer マップを再構築
    std::unordered_map<uint32_t, uint16_t> producerMap;
    for (auto& pass : m_passNodes) {
        for (const auto& w : pass.writes) {
            uint32_t key = (static_cast<uint32_t>(w.index) << 16) | w.version;
            producerMap[key] = pass.passIndex;
        }
    }

    while (!workQueue.empty()) {
        uint16_t idx = workQueue.front();
        workQueue.pop();

        PassNode& pass = m_passNodes[idx];
        for (const auto& r : pass.reads) {
            uint32_t key = (static_cast<uint32_t>(r.index) << 16) | r.version;
            auto it = producerMap.find(key);
            if (it != producerMap.end()) {
                PassNode& producer = m_passNodes[it->second];
                if (producer.refCount == 0) {
                    workQueue.push(producer.passIndex);
                }
                producer.refCount++;
            }
        }

        // Write-on-existing の暗黙依存も伝播
        for (const auto& w : pass.writes) {
            if (w.version > 1) {
                uint32_t prevKey = (static_cast<uint32_t>(w.index) << 16) | (w.version - 1);
                auto it = producerMap.find(prevKey);
                if (it != producerMap.end()) {
                    PassNode& prevProducer = m_passNodes[it->second];
                    if (prevProducer.refCount == 0) {
                        workQueue.push(prevProducer.passIndex);
                    }
                    prevProducer.refCount++;
                }
            }
        }
    }

    // refCount == 0 のパスをカリング
    for (auto& pass : m_passNodes) {
        pass.culled = (pass.refCount == 0);
        if (pass.culled) {
            LOG_INFO("[FrameGraph] Culled pass: %s", pass.name.c_str());
        }
    }
}

// Step 2c: リソース寿命計算
// Step 2c: リソース寿命計算
void FrameGraph::CalculateLifetimes() {
    // 全てのリソースの寿命を初期化（first は最大値、last は最小値へ）
    for (auto& res : m_resourceNodes) {
        res.firstPassIndex = UINT16_MAX;
        res.lastPassIndex = 0;
    }

    // executionOrder（ソート済み）に基づいてリソースの first/last を計算
    for (uint16_t execIdx : m_executionOrder) {
        PassNode& pass = m_passNodes[execIdx];
        if (pass.culled) continue;

        auto updateLifetime = [&](uint16_t resourceIndex) {
            if (resourceIndex >= m_resourceNodes.size()) return;
            ResourceNode& res = m_resourceNodes[resourceIndex];

            // std::min(a, b) の代わり
            res.firstPassIndex = (pass.passIndex < res.firstPassIndex) ? pass.passIndex : res.firstPassIndex;
            // std::max(a, b) の代わり
            res.lastPassIndex = (pass.passIndex > res.lastPassIndex) ? pass.passIndex : res.lastPassIndex;
            };

        for (const auto& r : pass.reads)  updateLifetime(r.index);
        for (const auto& w : pass.writes) updateLifetime(w.index);
    }
}

// Step 2d: トポロジカルソート (Kahn's algorithm)
void FrameGraph::TopologicalSort() {
    const size_t passCount = m_passNodes.size();

    // カリングされたパスの inDegree を計算し直す (カリング済みパスを除外)
    std::vector<uint16_t> sortInDegree(passCount, 0);
    std::vector<std::vector<uint16_t>> sortAdjacency(passCount);

    for (size_t i = 0; i < passCount; ++i) {
        if (m_passNodes[i].culled) continue;
        for (uint16_t succ : m_adjacency[i]) {
            if (m_passNodes[succ].culled) continue;
            sortAdjacency[i].push_back(succ);
            sortInDegree[succ]++;
        }
    }

    std::queue<uint16_t> q;
    for (size_t i = 0; i < passCount; ++i) {
        if (!m_passNodes[i].culled && sortInDegree[i] == 0) {
            q.push(static_cast<uint16_t>(i));
        }
    }

    m_executionOrder.clear();
    m_executionOrder.reserve(passCount);

    while (!q.empty()) {
        uint16_t idx = q.front();
        q.pop();
        m_executionOrder.push_back(idx);

        for (uint16_t succ : sortAdjacency[idx]) {
            sortInDegree[succ]--;
            if (sortInDegree[succ] == 0) {
                q.push(succ);
            }
        }
    }

    // サイクル検出
    size_t nonCulledCount = 0;
    for (auto& p : m_passNodes) {
        if (!p.culled) nonCulledCount++;
    }
    if (m_executionOrder.size() != nonCulledCount) {
        LOG_ERROR("[FrameGraph] Cycle detected! Sorted %zu of %zu passes.",
            m_executionOrder.size(), nonCulledCount);
    }
}

// ============================================================
// Phase 3: Execute
// ============================================================
void FrameGraph::ExecutePasses(const RenderQueue& queue, RenderContext& rc) {
    IResourceFactory* factory = Graphics::Instance().GetResourceFactory();
    FrameGraphResources resources(*this);

    for (uint16_t execIdx : m_executionOrder) {
        PassNode& pass = m_passNodes[execIdx];
        PROFILE_SCOPE(pass.name.c_str());

        // リソース取得: このパスで初めて使うリソースを Acquire
        auto acquireForPass = [&](uint16_t resourceIndex) {
            if (resourceIndex >= m_resourceNodes.size()) return;
            ResourceNode& res = m_resourceNodes[resourceIndex];
            if (res.isImported) return;
            if (res.ownedTexture) return; // 既に取得済み
            if (res.firstPassIndex == pass.passIndex) {
                res.ownedTexture = m_resourcePool.AcquireTexture(
                    res.name, res.desc, factory, m_frameCount);
            }
            };

        for (const auto& r : pass.reads)  acquireForPass(r.index);
        for (const auto& w : pass.writes) acquireForPass(w.index);

        // ステート遷移
        for (const auto& r : pass.reads) {
            if (r.index >= m_resourceNodes.size()) continue;
            ITexture* tex = m_resourceNodes[r.index].GetPhysical();
            if (tex && tex->GetCurrentState() != ResourceState::ShaderResource) {
                rc.commandList->TransitionBarrier(tex, ResourceState::ShaderResource);
            }
        }
        for (const auto& w : pass.writes) {
            if (w.index >= m_resourceNodes.size()) continue;
            ResourceNode& res = m_resourceNodes[w.index];
            ITexture* tex = res.GetPhysical();
            if (!tex) continue;

            // Depth フォーマットは DepthWrite、それ以外は RenderTarget
            ResourceState targetState = ResourceState::RenderTarget;
            if (res.desc.format == TextureFormat::D32_FLOAT ||
                res.desc.format == TextureFormat::D24_UNORM_S8_UINT) {
                targetState = ResourceState::DepthWrite;
            }
            if (tex->GetCurrentState() != targetState) {
                rc.commandList->TransitionBarrier(tex, targetState);
            }
        }

        // パス実行
        pass.renderPass->Execute(resources, queue, rc);
    }
}

ITexture* FrameGraph::GetPhysicalTexture(ResourceHandle handle) const {
    if (!handle.IsValid()) return nullptr;
    if (handle.index >= m_resourceNodes.size()) return nullptr;
    return m_resourceNodes[handle.index].GetPhysical();
}

// ============================================================
// デバッグ: Graphviz DOT 出力
// ============================================================
std::string FrameGraph::DumpGraphviz() const {
    std::ostringstream ss;
    ss << "digraph FrameGraph {\n";
    ss << "  rankdir=LR;\n";
    ss << "  node [shape=box, style=filled];\n\n";

    // パスノード
    for (const auto& pass : m_passNodes) {
        const char* color = pass.culled ? "gray90" : (pass.hasSideEffects ? "lightcoral" : "lightblue");
        ss << "  pass_" << pass.passIndex << " [label=\"" << pass.name << "\"";
        ss << ", fillcolor=" << color << "];\n";
    }

    // リソースノード
    ss << "\n  node [shape=ellipse, style=filled, fillcolor=lightyellow];\n";
    for (size_t i = 0; i < m_resourceNodes.size(); ++i) {
        const auto& res = m_resourceNodes[i];
        ss << "  res_" << i << " [label=\"" << res.name;
        if (res.isImported) ss << "\\n(imported)";
        ss << "\\nv" << res.version << "\"];\n";
    }

    // エッジ
    ss << "\n";
    for (const auto& pass : m_passNodes) {
        for (const auto& w : pass.writes) {
            ss << "  pass_" << pass.passIndex << " -> res_" << w.index;
            ss << " [label=\"w:v" << w.version << "\"];\n";
        }
        for (const auto& r : pass.reads) {
            ss << "  res_" << r.index << " -> pass_" << pass.passIndex;
            ss << " [label=\"r:v" << r.version << "\"];\n";
        }
    }

    ss << "}\n";
    return ss.str();
}
