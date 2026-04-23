#include "EffectGraphSerializer.h"

#include <filesystem>
#include <fstream>
#include "JSONManager.h"

namespace
{
    // EffectGraphPin を json へ変換する。
    nlohmann::json ToJson(const EffectGraphPin& pin)
    {
        return {
            {"id", pin.id},
            {"nodeId", pin.nodeId},
            {"name", pin.name},
            {"kind", static_cast<int>(pin.kind)},
            {"valueType", static_cast<int>(pin.valueType)}
        };
    }

    // json から EffectGraphPin を復元する。
    EffectGraphPin PinFromJson(const nlohmann::json& json)
    {
        EffectGraphPin pin;
        pin.id = json.value("id", 0u);
        pin.nodeId = json.value("nodeId", 0u);
        pin.name = json.value("name", "");
        pin.kind = static_cast<EffectPinKind>(json.value("kind", 0));
        pin.valueType = static_cast<EffectValueType>(json.value("valueType", 0));
        return pin;
    }

    // EffectGraphNode を json へ変換する。
    nlohmann::json ToJson(const EffectGraphNode& node)
    {
        return {
            {"id", node.id},
            {"type", static_cast<int>(node.type)},
            {"title", node.title},
            {"position", node.position},
            {"scalar", node.scalar},
            {"scalar2", node.scalar2},
            {"vectorValue", node.vectorValue},
            {"vectorValue2", node.vectorValue2},
            {"vectorValue3", node.vectorValue3},
            {"vectorValue4", node.vectorValue4},
            {"vectorValue5", node.vectorValue5},
            {"vectorValue6", node.vectorValue6},
            {"vectorValue7", node.vectorValue7},
            {"vectorValue8", node.vectorValue8},
            {"vectorValue9", node.vectorValue9},
            {"stringValue", node.stringValue},
            {"stringValue2", node.stringValue2},
            {"stringValue3", node.stringValue3},
            {"stringValue4", node.stringValue4},
            {"stringValue5", node.stringValue5},
            {"stringValue6", node.stringValue6},
            {"intValue", node.intValue},
            {"intValue2", node.intValue2},
            {"boolValue", node.boolValue}
        };
    }

    // json から EffectGraphNode を復元する。
    EffectGraphNode NodeFromJson(const nlohmann::json& json)
    {
        EffectGraphNode node;
        node.id = json.value("id", 0u);
        node.type = static_cast<EffectGraphNodeType>(json.value("type", 0));
        node.title = json.value("title", EffectGraphNodeTypeToString(node.type));
        node.position = json.value("position", DirectX::XMFLOAT2{ 0.0f, 0.0f });
        node.scalar = json.value("scalar", 0.0f);
        node.scalar2 = json.value("scalar2", 0.0f);
        node.vectorValue = json.value("vectorValue", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue2 = json.value("vectorValue2", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue3 = json.value("vectorValue3", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue4 = json.value("vectorValue4", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue5 = json.value("vectorValue5", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue6 = json.value("vectorValue6", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue7 = json.value("vectorValue7", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue8 = json.value("vectorValue8", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.vectorValue9 = json.value("vectorValue9", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        node.stringValue = json.value("stringValue", "");
        node.stringValue2 = json.value("stringValue2", "");
        node.stringValue3 = json.value("stringValue3", "");
        node.stringValue4 = json.value("stringValue4", "");
        node.stringValue5 = json.value("stringValue5", "");
        node.stringValue6 = json.value("stringValue6", "");
        node.intValue = json.value("intValue", 0);
        node.intValue2 = json.value("intValue2", 0);
        node.boolValue = json.value("boolValue", false);
        return node;
    }

    // EffectGraphLink を json へ変換する。
    nlohmann::json ToJson(const EffectGraphLink& link)
    {
        return {
            {"id", link.id},
            {"startPinId", link.startPinId},
            {"endPinId", link.endPinId}
        };
    }

    // json から EffectGraphLink を復元する。
    EffectGraphLink LinkFromJson(const nlohmann::json& json)
    {
        EffectGraphLink link;
        link.id = json.value("id", 0u);
        link.startPinId = json.value("startPinId", 0u);
        link.endPinId = json.value("endPinId", 0u);
        return link;
    }

    // EffectExposedParameter を json へ変換する。
    nlohmann::json ToJson(const EffectExposedParameter& parameter)
    {
        return {
            {"name", parameter.name},
            {"valueType", static_cast<int>(parameter.valueType)},
            {"defaultValue", parameter.defaultValue}
        };
    }

    // json から EffectExposedParameter を復元する。
    EffectExposedParameter ParameterFromJson(const nlohmann::json& json)
    {
        EffectExposedParameter parameter;
        parameter.name = json.value("name", "");
        parameter.valueType = static_cast<EffectValueType>(json.value("valueType", 0));
        parameter.defaultValue = json.value("defaultValue", DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
        return parameter;
    }
}

// EffectGraphAsset を JSON ファイルとして保存する。
bool EffectGraphSerializer::Save(const std::string& path, const EffectGraphAsset& asset)
{
    std::filesystem::path filePath(path);

    // 親ディレクトリがあるなら事前に作成する。
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    // ルート JSON を組み立てる。
    nlohmann::json root;
    root["schemaVersion"] = asset.schemaVersion;
    root["graphId"] = asset.graphId;
    root["name"] = asset.name;
    root["nextNodeId"] = asset.nextNodeId;
    root["nextPinId"] = asset.nextPinId;
    root["nextLinkId"] = asset.nextLinkId;

    // プレビュー既定値を保存する。
    root["previewDefaults"] = {
        {"duration", asset.previewDefaults.duration},
        {"seed", asset.previewDefaults.seed},
        {"previewMeshPath", asset.previewDefaults.previewMeshPath},
        {"previewMaterialPath", asset.previewDefaults.previewMaterialPath}
    };

    // 参照アセット一覧を保存する。
    root["referencedAssets"] = asset.referencedAssets;

    // ノード一覧を保存する。
    root["nodes"] = nlohmann::json::array();
    for (const auto& node : asset.nodes) {
        root["nodes"].push_back(ToJson(node));
    }

    // ピン一覧を保存する。
    root["pins"] = nlohmann::json::array();
    for (const auto& pin : asset.pins) {
        root["pins"].push_back(ToJson(pin));
    }

    // リンク一覧を保存する。
    root["links"] = nlohmann::json::array();
    for (const auto& link : asset.links) {
        root["links"].push_back(ToJson(link));
    }

    // 公開パラメータ一覧を保存する。
    root["exposedParameters"] = nlohmann::json::array();
    for (const auto& parameter : asset.exposedParameters) {
        root["exposedParameters"].push_back(ToJson(parameter));
    }

    // ファイルを書き込みモードで開く。
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        return false;
    }

    // 整形付き JSON として保存する。
    output << root.dump(2);
    return true;
}

// JSON ファイルから EffectGraphAsset を読み込む。
bool EffectGraphSerializer::Load(const std::string& path, EffectGraphAsset& outAsset)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        input >> root;
    }
    catch (...) {
        return false;
    }

    // 読み込み前に初期化する。
    outAsset = {};
    outAsset.schemaVersion = root.value("schemaVersion", 1u);
    outAsset.graphId = root.value("graphId", "effect_graph");
    outAsset.name = root.value("name", "Untitled Effect");
    outAsset.nextNodeId = root.value("nextNodeId", 1u);
    outAsset.nextPinId = root.value("nextPinId", 1u);
    outAsset.nextLinkId = root.value("nextLinkId", 1u);

    // プレビュー既定値を読み込む。
    if (root.contains("previewDefaults")) {
        const auto& preview = root["previewDefaults"];
        outAsset.previewDefaults.duration = preview.value("duration", 2.0f);
        outAsset.previewDefaults.seed = preview.value("seed", 1u);
        outAsset.previewDefaults.previewMeshPath = preview.value("previewMeshPath", "");
        outAsset.previewDefaults.previewMaterialPath = preview.value("previewMaterialPath", "");
    }

    // 参照アセット一覧を読み込む。
    if (root.contains("referencedAssets") && root["referencedAssets"].is_array()) {
        outAsset.referencedAssets = root["referencedAssets"].get<std::vector<std::string>>();
    }

    // ノード一覧を読み込む。
    if (root.contains("nodes") && root["nodes"].is_array()) {
        for (const auto& nodeJson : root["nodes"]) {
            outAsset.nodes.push_back(NodeFromJson(nodeJson));
        }
    }

    // ピン一覧を読み込む。
    if (root.contains("pins") && root["pins"].is_array()) {
        for (const auto& pinJson : root["pins"]) {
            outAsset.pins.push_back(PinFromJson(pinJson));
        }
    }

    // リンク一覧を読み込む。
    if (root.contains("links") && root["links"].is_array()) {
        for (const auto& linkJson : root["links"]) {
            outAsset.links.push_back(LinkFromJson(linkJson));
        }
    }

    // 公開パラメータ一覧を読み込む。
    if (root.contains("exposedParameters") && root["exposedParameters"].is_array()) {
        for (const auto& parameterJson : root["exposedParameters"]) {
            outAsset.exposedParameters.push_back(ParameterFromJson(parameterJson));
        }
    }

    return true;
}