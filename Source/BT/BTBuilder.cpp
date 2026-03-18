#include "BTBuilder.h"
#include <fstream>
#include <iostream>
#include "JSONManager.h"
#include "System/Dialog.h" // パス解決用などで使用している場合
#include "BTNodes.h"

static std::shared_ptr<BTNode> CreateNodeInstance(int type, const std::string& name) {
    switch (type) {
    case 0: // Root
        return std::make_shared<BTSelector>();

    case 1: // Composite
        if (name.find("Weighted") != std::string::npos) return std::make_shared<BTWeightedSelector>();
        if (name.find("Selector") != std::string::npos) return std::make_shared<BTSelector>();
        if (name.find("Sequence") != std::string::npos) return std::make_shared<BTSequence>();
        return std::make_shared<BTSequence>();

    case 2: // Decorator (★ここを拡張)
        if (name.find("CheckHP") != std::string::npos)   return std::make_shared<BTDecorator_CheckHP>();
        if (name.find("Cooldown") != std::string::npos)  return std::make_shared<BTDecorator_Cooldown>();
        if (name.find("Loop") != std::string::npos)      return std::make_shared<BTDecorator_Loop>();
        if (name.find("CheckDist") != std::string::npos) return std::make_shared<BTDecorator_CheckDist>();
        if (name.find("CheckWall") != std::string::npos) return std::make_shared<BTDecorator_CheckWall>();
        if (name.find("Probability") != std::string::npos) return std::make_shared<BTDecorator_Probability>();
     return std::make_shared<BTDecorator_CheckDist>();


    case 3: // Action
        if (name.find("Log") != std::string::npos)        return std::make_shared<BTAction_Log>();
        if (name.find("Wait") != std::string::npos)       return std::make_shared<BTAction_Wait>();
        if (name.find("MoveTo") != std::string::npos)     return std::make_shared<BTAction_MoveTo>();
        if (name.find("Chase") != std::string::npos)      return std::make_shared<BTAction_MoveTo>();
        if (name.find("PlayAction") != std::string::npos) return std::make_shared<BTAction_PlayAnim>();
        if (name.find("Rotate") != std::string::npos)      return std::make_shared<BTAction_RotateToTarget>();

        return std::make_shared<BTAction>();

    default:
        return std::make_shared<BTAction>();
    }
}




std::shared_ptr<BTBrain> BTBuilder::BuildFromFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "[BTBuilder] Failed to open file: " << path << std::endl;
        return nullptr;
    }

    json rootJson;
    try {
        ifs >> rootJson;
    }
    catch (...) {
        return nullptr;
    }

    auto brain = std::make_shared<BTBrain>();

    // 1. 全ノードの生成とIDマッピング
    // Map: NodeID -> BTNode Instance
    std::map<unsigned int, std::shared_ptr<BTNode>> nodeMap;
    // Map: PinID -> NodeID (リンク解決用)
    std::map<unsigned int, unsigned int> pinToNodeMap;

    for (const auto& nJ : rootJson["nodes"]) {
        unsigned int nid = nJ["id"];
        int type = nJ["type"];
        std::string name = nJ["name"];

        auto node = CreateNodeInstance(type, name);
        node->nodeId = nid;
        node->nodeName = name;

        // プロパティの読み込み
        if (nJ.contains("props")) {
            for (auto& [key, val] : nJ["props"].items()) {
                if (val.is_number_float()) node->paramsFloat[key] = val.get<float>();
                else if (val.is_number_integer()) node->paramsInt[key] = val.get<int>();
                else if (val.is_string()) node->paramsString[key] = val.get<std::string>();
            }
        }

        nodeMap[nid] = node;

        // ピン情報のマッピング
        for (const auto& p : nJ["inputs"]) {
            pinToNodeMap[p["id"]] = nid;
        }
        for (const auto& p : nJ["outputs"]) {
            pinToNodeMap[p["id"]] = nid;
        }
    }

    // 2. リンクの接続
    // JSON: links [ { "s": startPin, "e": endPin }, ... ]
    for (const auto& lJ : rootJson["links"]) {
        unsigned int startPin = lJ["s"];
        unsigned int endPin = lJ["e"];
        float weight = lJ.value("w", 1.0f);

        if (pinToNodeMap.count(startPin) && pinToNodeMap.count(endPin)) {
            unsigned int parentId = pinToNodeMap[startPin];
            unsigned int childId = pinToNodeMap[endPin];

            auto parent = nodeMap[parentId];
            auto child = nodeMap[childId];

            if (auto comp = std::dynamic_pointer_cast<BTComposite>(parent)) {
                comp->AddChild(child);
                comp->weights.push_back(weight);
            }
            // ★Decorator対応: 子をセットする
            else if (auto dec = std::dynamic_pointer_cast<BTDecorator>(parent)) {
                dec->SetChild(child);
            }
        }
    }


    // 3. フェーズ情報の構築 (Brainへの登録)
    // JSON: phases { "Default": 10, "Angry": 25 }
    if (rootJson.contains("phases")) {
        for (auto& [phaseName, rootIdVal] : rootJson["phases"].items()) {
            unsigned int rootId = rootIdVal;
            if (nodeMap.count(rootId)) {
                brain->AddPhase(phaseName, nodeMap[rootId]);
            }
        }
    }
    else {
        // フェーズ情報がない古い形式の場合、ID最小のRootなどを探す等のフォールバック
    }

    return brain;
}