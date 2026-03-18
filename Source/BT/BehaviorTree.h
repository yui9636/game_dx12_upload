#pragma once
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <iostream>
#include <variant>
#include <random>
#include <numeric> 
#include <algorithm> 


// 前方宣言
class Actor;

// 実行結果ステータス
enum class BTStatus {
    Success,
    Failure,
    Running
};

// 共有メモリ（ブラックボード）
struct BTContext {
    std::shared_ptr<Actor> owner;
    std::weak_ptr<Actor> target;
    float deltaTime = 0.0f;

    float stageRadius = 1000.0f;
};

struct BTNodeReport {
    BTStatus status;
    float lastTickTime;
};

// ----------------------------------------------------------------------------
// ノード基底クラス
// ----------------------------------------------------------------------------
class BTNode {
public:
    virtual ~BTNode() = default;

    BTStatus Tick(BTContext& ctx) {
        if (status != BTStatus::Running) OnEnter(ctx);
        status = OnUpdate(ctx);
        if (status != BTStatus::Running) OnExit(ctx, status);
        return status;
    }

    void Abort(BTContext& ctx) {
        if (status == BTStatus::Running) {
            OnExit(ctx, BTStatus::Failure);
            status = BTStatus::Failure;
        }
    }

    void Reset() { status = BTStatus::Failure; }
    BTStatus GetStatus() const { return status; }

    // デバッグ・識別用
    unsigned int nodeId = 0;
    std::string nodeName;

    // 動的パラメータ（エディタのプロパティを受け取る）
    std::map<std::string, float> paramsFloat;
    std::map<std::string, int> paramsInt;
    std::map<std::string, std::string> paramsString;

protected:
    virtual void OnEnter(BTContext& ctx) {}
    virtual BTStatus OnUpdate(BTContext& ctx) = 0;
    virtual void OnExit(BTContext& ctx, BTStatus result) {}

    BTStatus status = BTStatus::Failure;
};

// ----------------------------------------------------------------------------
// コンポジット (Composite): 子を持つノード
// ----------------------------------------------------------------------------
class BTComposite : public BTNode {
public:
    void AddChild(std::shared_ptr<BTNode> child) { children.push_back(child); }
    const std::vector<std::shared_ptr<BTNode>>& GetChildren() const { return children; }

    std::vector<float> weights;
protected:
    std::vector<std::shared_ptr<BTNode>> children;
    int currentChildIndex = 0;
};

class BTDecorator : public BTNode {
public:
    void SetChild(std::shared_ptr<BTNode> node) { child = node; }
    std::shared_ptr<BTNode> GetChild() const { return child; }

protected:
    std::shared_ptr<BTNode> child = nullptr;
};


// Selector: どれか成功すればOK (OR)
class BTSelector : public BTComposite {
protected:
    void OnEnter(BTContext& ctx) override { currentChildIndex = 0; }
    BTStatus OnUpdate(BTContext& ctx) override {
        while (currentChildIndex < children.size()) {
            BTStatus result = children[currentChildIndex]->Tick(ctx);
            if (result == BTStatus::Running) return BTStatus::Running;
            if (result == BTStatus::Success) return BTStatus::Success;
            currentChildIndex++;
        }
        return BTStatus::Failure;
    }
};

// Sequence: 全員成功でOK (AND)
class BTSequence : public BTComposite {
protected:
    void OnEnter(BTContext& ctx) override { currentChildIndex = 0; }
    BTStatus OnUpdate(BTContext& ctx) override {
        while (currentChildIndex < children.size()) {
            BTStatus result = children[currentChildIndex]->Tick(ctx);
            if (result == BTStatus::Running) return BTStatus::Running;
            if (result == BTStatus::Failure) return BTStatus::Failure;
            currentChildIndex++;
        }
        return BTStatus::Success;
    }
};

// ----------------------------------------------------------------------------
// アクション (Action): 実際の行動
// ----------------------------------------------------------------------------
// 汎用アクションノード：具体的な処理は派生させるか、パラメータで分岐する
class BTAction : public BTNode {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        // デフォルトでは即成功 (継承して使う前提)
        return BTStatus::Success;
    }
};

// ----------------------------------------------------------------------------
// ブレイン (Brain): AIの頭脳全体を管理するクラス
// ----------------------------------------------------------------------------
class BTBrain {
public:
    // フェーズごとのルートノードを登録
    void AddPhase(const std::string& phaseName, std::shared_ptr<BTNode> root) {
        roots[phaseName] = root;
        if (activeRoot == nullptr) activeRoot = root; // 最初に追加されたものをデフォルトに
    }

    void SetActivePhase(const std::string& phaseName) {
        if (roots.count(phaseName)) {
            activeRoot = roots[phaseName];
            // フェーズ切り替え時にリセット等の処理が必要ならここ
        }
    }

    void Tick(BTContext& ctx) {
        if (activeRoot) activeRoot->Tick(ctx);
    }

    std::map<unsigned int, BTNodeReport> GetLiveStatusMap() const {
        std::map<unsigned int, BTNodeReport> reports;
        if (activeRoot) CollectStatusRecursive(activeRoot, reports);
        return reports;
    }


private:
    void CollectStatusRecursive(std::shared_ptr<BTNode> node, std::map<unsigned int, BTNodeReport>& outMap) const {
        if (!node) return;
        outMap[node->nodeId] = { node->GetStatus(), 0.0f /* 時間は任意 */ };

        if (auto comp = std::dynamic_pointer_cast<BTComposite>(node)) {
            for (auto& child : comp->GetChildren()) {
                CollectStatusRecursive(child, outMap);
            }
        }

        else if (auto dec = std::dynamic_pointer_cast<BTDecorator>(node)) {
            CollectStatusRecursive(dec->GetChild(), outMap);
        }
    }

    std::map<std::string, std::shared_ptr<BTNode>> roots;
    std::shared_ptr<BTNode> activeRoot = nullptr;
};


class BTWeightedSelector : public BTComposite {
    int selectedIndex = -1;

protected:
    void OnEnter(BTContext& ctx) override {
        selectedIndex = -1;
        if (children.empty()) return;

        // 1. 合計値を計算
        float totalWeight = std::accumulate(weights.begin(), weights.end(), 0.0f);
        if (totalWeight <= 0.0f) {
            selectedIndex = 0; // 重みが設定されていなければ一番最初
            return;
        }

        // 2. 乱数生成
        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dis(0.0f, totalWeight);
        float randomValue = dis(gen);

        // 3. ルーレット選択
        float currentSum = 0.0f;
        for (size_t i = 0; i < weights.size(); ++i) {
            currentSum += weights[i];
            if (randomValue <= currentSum) {
                selectedIndex = (int)i;
                break;
            }
        }
    }

    BTStatus OnUpdate(BTContext& ctx) override {
        if (selectedIndex < 0 || selectedIndex >= (int)children.size()) {
            return BTStatus::Failure;
        }

        // 選ばれた行動を Tick する
        BTStatus result = children[selectedIndex]->Tick(ctx);

        // 実行中ならそのまま返す
        if (result == BTStatus::Running) return BTStatus::Running;

        // 成功または失敗したら完了
        return result;
    }
};



