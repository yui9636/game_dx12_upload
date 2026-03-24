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


class Actor;

enum class BTStatus {
    Success,
    Failure,
    Running
};

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

    unsigned int nodeId = 0;
    std::string nodeName;

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
// ----------------------------------------------------------------------------
class BTAction : public BTNode {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        return BTStatus::Success;
    }
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTBrain {
public:
    void AddPhase(const std::string& phaseName, std::shared_ptr<BTNode> root) {
        roots[phaseName] = root;
        if (activeRoot == nullptr) activeRoot = root;
    }

    void SetActivePhase(const std::string& phaseName) {
        if (roots.count(phaseName)) {
            activeRoot = roots[phaseName];
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
        outMap[node->nodeId] = { node->GetStatus(), 0.0f /* éûçèÇÕñ¢égóp */ };

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

        float totalWeight = std::accumulate(weights.begin(), weights.end(), 0.0f);
        if (totalWeight <= 0.0f) {
            selectedIndex = 0;
            return;
        }

        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dis(0.0f, totalWeight);
        float randomValue = dis(gen);

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

        BTStatus result = children[selectedIndex]->Tick(ctx);

        if (result == BTStatus::Running) return BTStatus::Running;

        return result;
    }
};



