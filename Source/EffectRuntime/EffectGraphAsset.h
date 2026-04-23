#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <DirectXMath.h>
#include "RenderContext/RenderState.h"
#include "EffectRuntime/EffectMeshVariant.h"

// ==========================================================
// Effect Graph のノード種別
// ==========================================================
enum class EffectGraphNodeType : uint8_t
{
    Output = 0,       // グラフ終端
    Spawn,            // 発生開始
    Lifetime,         // 寿命制御
    MeshSource,       // メッシュ入力
    MeshRenderer,     // メッシュ描画
    ParticleEmitter,  // パーティクル発生
    SpriteRenderer,   // スプライト/ビルボード描画
    Float,            // Float 定数
    Vec3,             // Vec3 定数
    Color             // Color 定数
};

// ピンの向き。
enum class EffectPinKind : uint8_t
{
    Input = 0,   // 入力ピン
    Output       // 出力ピン
};

// ピンが流す値の型。
enum class EffectValueType : uint8_t
{
    Flow = 0,    // 実行フロー
    Float,       // float
    Vec3,        // vec3
    Color,       // 色
    Mesh,        // メッシュ参照
    Particle     // パーティクルデータ
};

// ノードが属する実行ステージ。
enum class EffectNodeStage : uint8_t
{
    None = 0,
    Spawn,
    Update,
    Render
};

// パーティクル描画方式。
enum class EffectParticleDrawMode : uint8_t
{
    Billboard = 0, // カメラ正対
    Mesh,          // メッシュ描画
    Ribbon         // 軌跡リボン
};

// パーティクルソート方式。
enum class EffectParticleSortMode : uint8_t
{
    None = 0,
    BackToFront,
    FrontToBack
};

// パーティクルブレンド方式。
enum class EffectParticleBlendMode : uint8_t
{
    PremultipliedAlpha = 0,  // ONE / INV_SRC_ALPHA
    Additive,                // ONE / ONE
    AlphaBlend,              // SRC_ALPHA / INV_SRC_ALPHA
    Multiply,                // DEST_COLOR / ZERO
    SoftAdditive,            // ONE / INV_SRC_COLOR
    EnumCount
};

// パーティクル発生形状。
enum class EffectSpawnShapeType : uint8_t
{
    Point = 0,
    Sphere,
    Box,
    Cone,
    Circle,
    Line
};

// ==========================================================
// パーティクル最大数まわりの定数
// ==========================================================
inline constexpr uint32_t kEffectParticleDefaultMaxParticles = 5000000u;
inline constexpr uint32_t kEffectParticleMinSuggestedMaxParticles = 512u;
inline constexpr uint32_t kEffectParticleHardMaxParticles = 1u << 23;

// 要求粒子数を、最小値以上かつ 2 の累乗ベースで扱いやすい容量へ丸める。
inline uint32_t AlignEffectParticleCount(uint32_t requested, uint32_t minimum = kEffectParticleMinSuggestedMaxParticles)
{
    uint32_t capacity = (std::max)(minimum, 1u);
    while (capacity < requested && capacity < kEffectParticleHardMaxParticles) {
        capacity <<= 1u;
    }
    return capacity < requested ? requested : capacity;
}

// SpawnRate / Burst / Lifetime / Duration から推奨最大粒子数を見積もる。
inline uint32_t EstimateEffectParticleRecommendedMaxParticles(
    float spawnRate,
    uint32_t burstCount,
    float particleLifetime,
    float duration)
{
    const float clampedSpawnRate = (std::max)(spawnRate, 0.0f);
    const float clampedLifetime = (std::max)(particleLifetime, 0.0f);

    // Duration がある場合は、その範囲内で実際に同時生存しうる時間窓を使う。
    const float activeWindow = duration > 0.0f
        ? (std::min)(clampedLifetime, duration)
        : clampedLifetime;

    const float liveFromRate = clampedSpawnRate * activeWindow;
    const float requested = liveFromRate * 1.5f + static_cast<float>(burstCount) * 1.25f + 32.0f;

    return AlignEffectParticleCount(
        static_cast<uint32_t>(std::ceil((std::max)(requested, static_cast<float>(burstCount)))));
}

// authoring 側で maxParticles が指定されていればそれを使い、
// 未指定なら推奨値から自動決定する。
inline uint32_t ResolveEffectParticleMaxParticles(
    int authoredMaxParticles,
    float spawnRate,
    uint32_t burstCount,
    float particleLifetime,
    float duration)
{
    if (authoredMaxParticles > 0) {
        return static_cast<uint32_t>(authoredMaxParticles);
    }

    return (std::min)(
        kEffectParticleDefaultMaxParticles,
        EstimateEffectParticleRecommendedMaxParticles(spawnRate, burstCount, particleLifetime, duration));
}

// authoring 側の maxParticles が推奨値を下回っているか判定する。
inline bool IsEffectParticleMaxParticlesTooLow(
    int authoredMaxParticles,
    float spawnRate,
    uint32_t burstCount,
    float particleLifetime,
    float duration)
{
    if (authoredMaxParticles <= 0) {
        return false;
    }

    return static_cast<uint32_t>(authoredMaxParticles) <
        EstimateEffectParticleRecommendedMaxParticles(spawnRate, burstCount, particleLifetime, duration);
}

// ==========================================================
// Graph 基本構造
// ==========================================================

// グラフ上の 1 ピン。
struct EffectGraphPin
{
    uint32_t id = 0;                           // ピン ID
    uint32_t nodeId = 0;                       // 所属ノード ID
    std::string name;                          // 表示名
    EffectPinKind kind = EffectPinKind::Input; // 入出力種別
    EffectValueType valueType = EffectValueType::Flow; // 値型
};

// グラフ上の 1 ノード。
// 汎用的に多数の value slot を持たせた authoring 用データ構造。
struct EffectGraphNode
{
    uint32_t id = 0;
    EffectGraphNodeType type = EffectGraphNodeType::Output;
    std::string title;
    DirectX::XMFLOAT2 position = { 0.0f, 0.0f };

    // 汎用スカラー値スロット。
    float scalar = 0.0f;
    float scalar2 = 0.0f;

    // 汎用ベクトル値スロット群。
    DirectX::XMFLOAT4 vectorValue = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue2 = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue3 = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue4 = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue5 = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue6 = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue7 = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue8 = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 vectorValue9 = { 0.0f, 0.0f, 0.0f, 0.0f };

    // 汎用文字列スロット群。
    std::string stringValue;
    std::string stringValue2;
    std::string stringValue3;
    std::string stringValue4;
    std::string stringValue5;
    std::string stringValue6;

    // 汎用整数スロット群。
    int intValue = 0;
    int intValue2 = 0;

    // 汎用 bool スロット。
    bool boolValue = false;
};

// ノード間リンク。
struct EffectGraphLink
{
    uint32_t id = 0;
    uint32_t startPinId = 0; // 出力側ピン
    uint32_t endPinId = 0;   // 入力側ピン
};

// 外部公開パラメータ。
struct EffectExposedParameter
{
    std::string name;
    EffectValueType valueType = EffectValueType::Float;
    DirectX::XMFLOAT4 defaultValue = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// プレビュー用既定値。
struct EffectGraphPreviewDefaults
{
    float duration = 2.0f;
    uint32_t seed = 1;
    std::string previewMeshPath;
    std::string previewMaterialPath;
};

// Effect Graph アセット本体。
struct EffectGraphAsset
{
    uint32_t schemaVersion = 1;
    std::string graphId = "effect_graph";
    std::string name = "Untitled Effect";

    std::vector<EffectGraphNode> nodes;
    std::vector<EffectGraphPin> pins;
    std::vector<EffectGraphLink> links;
    std::vector<EffectExposedParameter> exposedParameters;

    EffectGraphPreviewDefaults previewDefaults;
    std::vector<std::string> referencedAssets;

    // 新規 ID 発行用カウンタ。
    uint32_t nextNodeId = 1;
    uint32_t nextPinId = 1;
    uint32_t nextLinkId = 1;

    // nodeId からノードを検索する。
    EffectGraphNode* FindNode(uint32_t nodeId)
    {
        auto it = std::find_if(nodes.begin(), nodes.end(), [nodeId](const EffectGraphNode& node) { return node.id == nodeId; });
        return it != nodes.end() ? &(*it) : nullptr;
    }

    // const 版 node 検索。
    const EffectGraphNode* FindNode(uint32_t nodeId) const
    {
        auto it = std::find_if(nodes.begin(), nodes.end(), [nodeId](const EffectGraphNode& node) { return node.id == nodeId; });
        return it != nodes.end() ? &(*it) : nullptr;
    }

    // pinId からピンを検索する。
    EffectGraphPin* FindPin(uint32_t pinId)
    {
        auto it = std::find_if(pins.begin(), pins.end(), [pinId](const EffectGraphPin& pin) { return pin.id == pinId; });
        return it != pins.end() ? &(*it) : nullptr;
    }

    // const 版 pin 検索。
    const EffectGraphPin* FindPin(uint32_t pinId) const
    {
        auto it = std::find_if(pins.begin(), pins.end(), [pinId](const EffectGraphPin& pin) { return pin.id == pinId; });
        return it != pins.end() ? &(*it) : nullptr;
    }
};

// ==========================================================
// ノード種別ユーティリティ
// ==========================================================

// ノード種別を表示文字列へ変換する。
inline const char* EffectGraphNodeTypeToString(EffectGraphNodeType type)
{
    switch (type) {
    case EffectGraphNodeType::Output:          return "Effect Output";
    case EffectGraphNodeType::Spawn:           return "Spawn";
    case EffectGraphNodeType::Lifetime:        return "Lifetime";
    case EffectGraphNodeType::MeshSource:      return "Mesh Source";
    case EffectGraphNodeType::MeshRenderer:    return "Mesh Renderer";
    case EffectGraphNodeType::ParticleEmitter: return "Particle Emitter";
    case EffectGraphNodeType::SpriteRenderer:  return "Sprite Renderer";
    case EffectGraphNodeType::Float:           return "Float";
    case EffectGraphNodeType::Vec3:            return "Vec3";
    case EffectGraphNodeType::Color:           return "Color";
    default:                                   return "Unknown";
    }
}

// ノード種別から実行ステージを返す。
inline EffectNodeStage GetEffectNodeStage(EffectGraphNodeType type)
{
    switch (type) {
    case EffectGraphNodeType::Spawn:
        return EffectNodeStage::Spawn;

    case EffectGraphNodeType::Lifetime:
    case EffectGraphNodeType::Float:
    case EffectGraphNodeType::Vec3:
    case EffectGraphNodeType::Color:
        return EffectNodeStage::Update;

    case EffectGraphNodeType::MeshSource:
    case EffectGraphNodeType::MeshRenderer:
    case EffectGraphNodeType::ParticleEmitter:
    case EffectGraphNodeType::SpriteRenderer:
    case EffectGraphNodeType::Output:
        return EffectNodeStage::Render;

    default:
        return EffectNodeStage::None;
    }
}

// 副作用を持つノードかどうか判定する。
// Flow fan-out 制限などに使う。
inline bool IsEffectSideEffectNode(EffectGraphNodeType type)
{
    switch (type) {
    case EffectGraphNodeType::Spawn:
    case EffectGraphNodeType::Lifetime:
    case EffectGraphNodeType::MeshRenderer:
    case EffectGraphNodeType::ParticleEmitter:
    case EffectGraphNodeType::SpriteRenderer:
        return true;

    default:
        return false;
    }
}

// ==========================================================
// Graph 作成補助
// ==========================================================

// ノード種別に応じたテンプレートピンを追加する。
inline void AppendEffectTemplatePins(EffectGraphAsset& asset, const EffectGraphNode& node)
{
    auto addPin = [&](std::string_view name, EffectPinKind kind, EffectValueType valueType) {
        EffectGraphPin pin;
        pin.id = asset.nextPinId++;
        pin.nodeId = node.id;
        pin.name = std::string(name);
        pin.kind = kind;
        pin.valueType = valueType;
        asset.pins.push_back(std::move(pin));
        };

    switch (node.type) {
    case EffectGraphNodeType::Output:
        addPin("In", EffectPinKind::Input, EffectValueType::Flow);
        break;

    case EffectGraphNodeType::Spawn:
        addPin("Out", EffectPinKind::Output, EffectValueType::Flow);
        break;

    case EffectGraphNodeType::Lifetime:
        addPin("In", EffectPinKind::Input, EffectValueType::Flow);
        addPin("Out", EffectPinKind::Output, EffectValueType::Flow);
        break;

    case EffectGraphNodeType::MeshSource:
        addPin("Mesh", EffectPinKind::Output, EffectValueType::Mesh);
        break;

    case EffectGraphNodeType::MeshRenderer:
        addPin("In", EffectPinKind::Input, EffectValueType::Flow);
        addPin("Mesh", EffectPinKind::Input, EffectValueType::Mesh);
        addPin("Color", EffectPinKind::Input, EffectValueType::Color);
        addPin("Out", EffectPinKind::Output, EffectValueType::Flow);
        break;

    case EffectGraphNodeType::ParticleEmitter:
        addPin("In", EffectPinKind::Input, EffectValueType::Flow);
        addPin("Particle", EffectPinKind::Output, EffectValueType::Particle);
        break;

    case EffectGraphNodeType::SpriteRenderer:
        addPin("Particle", EffectPinKind::Input, EffectValueType::Particle);
        addPin("Color", EffectPinKind::Input, EffectValueType::Color);
        addPin("Out", EffectPinKind::Output, EffectValueType::Flow);
        break;

    case EffectGraphNodeType::Float:
        addPin("Value", EffectPinKind::Output, EffectValueType::Float);
        break;

    case EffectGraphNodeType::Vec3:
        addPin("Value", EffectPinKind::Output, EffectValueType::Vec3);
        break;

    case EffectGraphNodeType::Color:
        addPin("Value", EffectPinKind::Output, EffectValueType::Color);
        break;
    }
}

// 指定型ノードを新規作成し、初期値とテンプレートピンを付与する。
inline EffectGraphNode& AddEffectGraphNode(EffectGraphAsset& asset, EffectGraphNodeType type, const DirectX::XMFLOAT2& pos)
{
    EffectGraphNode node;
    node.id = asset.nextNodeId++;
    node.type = type;
    node.title = EffectGraphNodeTypeToString(type);
    node.position = pos;

    // ノード種別ごとの初期値。
    switch (type) {
    case EffectGraphNodeType::Lifetime:
        node.scalar = asset.previewDefaults.duration;
        break;

    case EffectGraphNodeType::MeshSource:
        node.stringValue = asset.previewDefaults.previewMeshPath;
        break;

    case EffectGraphNodeType::MeshRenderer:
        node.stringValue = asset.previewDefaults.previewMaterialPath;
        node.vectorValue = { 1.0f, 1.0f, 1.0f, 1.0f };
        node.intValue = static_cast<int>(BlendState::Additive);
        break;

    case EffectGraphNodeType::ParticleEmitter:
        node.scalar = 32.0f;
        node.scalar2 = 0.0f;
        node.intValue = static_cast<int>(kEffectParticleDefaultMaxParticles);
        node.vectorValue = { 1.0f, 0.18f, 0.04f, 1.0f };
        node.vectorValue2 = { 0.0f, -0.55f, 0.0f, 0.0f };
        node.vectorValue3 = { 0.35f, 0.35f, 0.35f, 6.0f };
        node.vectorValue4 = { 0.0f, 0.18f, 0.20f, 0.0f };
        node.intValue2 = static_cast<int>(EffectSpawnShapeType::Sphere);
        break;

    case EffectGraphNodeType::SpriteRenderer:
        node.scalar = 96.0f;
        node.vectorValue = { 1.0f, 1.0f, 1.0f, 1.0f };
        node.intValue = static_cast<int>(EffectParticleDrawMode::Billboard);
        node.intValue2 = static_cast<int>(EffectParticleSortMode::BackToFront);
        node.vectorValue2 = { 0.08f, 0.30f, 1.0f, 0.0f };
        node.vectorValue3 = { 1.0f, 1.0f, 1.0f, 1.0f };
        node.boolValue = true;
        break;

    case EffectGraphNodeType::Float:
        node.scalar = 1.0f;
        break;

    case EffectGraphNodeType::Color:
        node.vectorValue = { 1.0f, 1.0f, 1.0f, 1.0f };
        node.vectorValue2 = { 1.0f, 1.0f, 1.0f, 0.0f };
        break;

    default:
        break;
    }

    asset.nodes.push_back(node);
    AppendEffectTemplatePins(asset, asset.nodes.back());
    return asset.nodes.back();
}

// 最低限の接続が済んだデフォルトグラフを作る。
inline EffectGraphAsset CreateDefaultEffectGraphAsset()
{
    EffectGraphAsset asset;
    asset.graphId = "effect_graph_default";
    asset.name = "New Effect";
    asset.previewDefaults.duration = 2.0f;
    asset.previewDefaults.seed = 1;

    auto& spawn = AddEffectGraphNode(asset, EffectGraphNodeType::Spawn, DirectX::XMFLOAT2{ 40.0f, 80.0f });
    auto& lifetime = AddEffectGraphNode(asset, EffectGraphNodeType::Lifetime, DirectX::XMFLOAT2{ 280.0f, 80.0f });
    auto& meshSource = AddEffectGraphNode(asset, EffectGraphNodeType::MeshSource, DirectX::XMFLOAT2{ 280.0f, 230.0f });
    auto& meshRenderer = AddEffectGraphNode(asset, EffectGraphNodeType::MeshRenderer, DirectX::XMFLOAT2{ 560.0f, 150.0f });
    auto& output = AddEffectGraphNode(asset, EffectGraphNodeType::Output, DirectX::XMFLOAT2{ 860.0f, 150.0f });

    // 条件に一致するピンを探す。
    auto findPin = [&](uint32_t nodeId, EffectPinKind kind, EffectValueType type) -> uint32_t {
        for (const auto& pin : asset.pins) {
            if (pin.nodeId == nodeId && pin.kind == kind && pin.valueType == type) {
                return pin.id;
            }
        }
        return 0;
        };

    // リンクを追加する。
    auto addLink = [&](uint32_t startPinId, uint32_t endPinId) {
        EffectGraphLink link;
        link.id = asset.nextLinkId++;
        link.startPinId = startPinId;
        link.endPinId = endPinId;
        asset.links.push_back(std::move(link));
        };

    addLink(findPin(spawn.id, EffectPinKind::Output, EffectValueType::Flow), findPin(lifetime.id, EffectPinKind::Input, EffectValueType::Flow));
    addLink(findPin(lifetime.id, EffectPinKind::Output, EffectValueType::Flow), findPin(meshRenderer.id, EffectPinKind::Input, EffectValueType::Flow));
    addLink(findPin(meshSource.id, EffectPinKind::Output, EffectValueType::Mesh), findPin(meshRenderer.id, EffectPinKind::Input, EffectValueType::Mesh));
    addLink(findPin(meshRenderer.id, EffectPinKind::Output, EffectValueType::Flow), findPin(output.id, EffectPinKind::Input, EffectValueType::Flow));

    return asset;
}

// ==========================================================
// コンパイル後データ
// ==========================================================

// コンパイル後に残る公開パラメータ情報。
struct EffectCompiledParameter
{
    std::string name;
    EffectValueType valueType = EffectValueType::Float;
    DirectX::XMFLOAT4 defaultValue = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// コンパイル済みノード情報。
struct EffectCompiledNode
{
    uint32_t sourceNodeId = 0;                      // 元ノード ID
    EffectGraphNodeType type = EffectGraphNodeType::Output;
    EffectNodeStage stage = EffectNodeStage::None; // 実行ステージ
    std::string debugLabel;                        // デバッグ表示用名
};

// ステージごとの実行順 ID リスト。
struct EffectExecutionPlan
{
    std::vector<uint32_t> spawnNodeIds;
    std::vector<uint32_t> updateNodeIds;
    std::vector<uint32_t> renderNodeIds;
};

// MeshRenderer 用コンパイル済み descriptor。
struct EffectMeshRendererDescriptor
{
    bool enabled = false;
    std::string meshAssetPath;
    std::string materialAssetPath;
    int shaderId = 1;
    uint32_t shaderVariantKey = 0;
    BlendState blendState = BlendState::Additive;
    DepthState depthState = DepthState::TestOnly;
    RasterizerState rasterizerState = RasterizerState::SolidCullBack;
    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Phase A: Mesh Variant System 用設定。
    EffectMeshVariantParams variantParams;
};

// Particle シミュレーション/描画用コンパイル済み descriptor。
struct EffectParticleSimulationLayout
{
    bool enabled = false;
    uint32_t maxParticles = 0;
    float spawnRate = 32.0f;
    uint32_t burstCount = 0;
    float particleLifetime = 1.0f;
    float startSize = 0.18f;
    float endSize = 0.04f;
    float speed = 1.0f;

    EffectParticleDrawMode drawMode = EffectParticleDrawMode::Billboard;
    EffectParticleSortMode sortMode = EffectParticleSortMode::BackToFront;
    EffectSpawnShapeType shapeType = EffectSpawnShapeType::Sphere;

    std::string texturePath;
    std::string meshAssetPath;

    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 tintEnd = { 1.0f, 1.0f, 1.0f, 0.0f };

    DirectX::XMFLOAT3 acceleration = { 0.0f, -0.55f, 0.0f };
    float drag = 0.0f;
    DirectX::XMFLOAT3 shapeParameters = { 0.35f, 0.35f, 0.35f };
    float spinRate = 6.0f;

    float ribbonWidth = 0.08f;
    float ribbonVelocityStretch = 0.30f;
    float sizeCurveBias = 1.0f;
    float alphaCurveBias = 1.0f;

    uint32_t subUvColumns = 1;
    uint32_t subUvRows = 1;
    float subUvFrameRate = 0.0f;

    float curlNoiseStrength = 0.0f;
    float curlNoiseScale = 0.18f;
    float curlNoiseScrollSpeed = 0.20f;
    float vortexStrength = 0.0f;

    bool softParticleEnabled = false;
    float softParticleScale = 96.0f;
    EffectParticleBlendMode blendMode = EffectParticleBlendMode::PremultipliedAlpha;

    float randomSpeedRange = 0.0f;
    float randomSizeRange = 0.0f;
    float randomLifeRange = 0.0f;

    float windStrength = 0.0f;
    DirectX::XMFLOAT3 windDirection = { 1.0f, 0.0f, 0.0f };
    float windTurbulence = 0.0f;

    // Phase 1C: サイズカーブ 4キー。
    DirectX::XMFLOAT4 sizeCurveValues = { 0.18f, 0.18f, 0.04f, 0.04f };
    DirectX::XMFLOAT4 sizeCurveTimes = { 0.0f,  0.33f, 0.66f, 1.0f };
    uint32_t sizeCurveKeyCount = 2;

    // Phase 1C: 色グラデーション 4キー。
    DirectX::XMFLOAT4 gradientColor0 = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 gradientColor1 = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 gradientColor2 = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT4 gradientColor3 = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT2 gradientMidTimes = { 0.33f, 0.66f };
    uint32_t gradientKeyCount = 2;

    // Phase 2: Attractor/Repeller。
    DirectX::XMFLOAT4 attractors[4] = {};
    DirectX::XMFLOAT4 attractorRadii = { 5.0f, 5.0f, 5.0f, 5.0f };
    DirectX::XMFLOAT4 attractorFalloff = { 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t attractorCount = 0;

    // Phase 2: GPU Collision。
    bool collisionEnabled = false;
    DirectX::XMFLOAT4 collisionPlane = { 0.0f, 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 collisionSpheres[4] = {};
    uint32_t collisionSphereCount = 0;
    float collisionRestitution = 0.5f;
    float collisionFriction = 0.3f;
};

// エフェクトグラフをコンパイルした最終成果物。
struct CompiledEffectAsset
{
    bool valid = false;                         // エラー無しなら true
    std::string sourceAssetPath;               // 元アセットパス
    std::string graphId;
    std::string name;

    std::vector<std::string> errors;           // コンパイルエラー
    std::vector<std::string> warnings;         // コンパイル警告

    EffectExecutionPlan executionPlan;
    std::vector<EffectCompiledNode> spawnNodeList;
    std::vector<EffectCompiledNode> updateNodeList;
    std::vector<EffectCompiledNode> renderNodeList;

    std::vector<EffectCompiledParameter> constantParameters;

    EffectMeshRendererDescriptor meshRenderer;
    EffectParticleSimulationLayout particleRenderer;

    std::vector<uint32_t> shaderVariantKeys;   // 必要な shader variant
    std::vector<std::string> requiredAssetReferences; // 依存アセット一覧

    float duration = 2.0f;
};