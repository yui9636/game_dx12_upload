#include "EffectCompiler.h"
#include "Console/Logger.h"
#include "EffectParameterBindings.h"

#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace
{
    // ピン同士の値型が接続可能かを判定する。
    // 現状は完全一致のみ許可している。
    bool IsCompatibleValueType(EffectValueType outputType, EffectValueType inputType)
    {
        return outputType == inputType;
    }

    // MeshRenderer 用のシェーダバリアントキーを組み立てる。
    // variant system 側で shaderFlags が明示されている場合はそれを優先する。
    uint32_t BuildShaderVariantKey(const EffectMeshRendererDescriptor& descriptor, bool hasColorInput)
    {
        // variant system によって shaderFlags が設定済みなら、それ自体をキーとして使う。
        if (descriptor.variantParams.shaderFlags != 0) {
            return descriptor.variantParams.shaderFlags;
        }

        uint32_t key = 0;

        // Additive ブレンドなら bit0 を立てる。
        if (descriptor.blendState == BlendState::Additive) {
            key |= 1u << 0;
        }

        // materialAssetPath があれば bit1 を立てる。
        if (!descriptor.materialAssetPath.empty()) {
            key |= 1u << 1;
        }

        // Color 入力があれば bit2 を立てる。
        if (hasColorInput) {
            key |= 1u << 2;
        }

        return key;
    }
}

// EffectGraphAsset を実行可能な CompiledEffectAsset へ変換する。
std::shared_ptr<CompiledEffectAsset> EffectCompiler::Compile(const EffectGraphAsset& asset, const std::string& sourceAssetPath)
{
    auto compiled = std::make_shared<CompiledEffectAsset>();

    // 元アセット情報を転写する。
    compiled->sourceAssetPath = sourceAssetPath;
    compiled->graphId = asset.graphId;
    compiled->name = asset.name;
    compiled->duration = asset.previewDefaults.duration;

    // DAG 検証とトポロジカルソート用の補助マップ。
    std::unordered_map<uint32_t, uint32_t> incomingEdgeCount;
    std::unordered_map<uint32_t, std::vector<uint32_t>> adjacency;
    std::unordered_map<uint32_t, uint32_t> pinToNode;

    // pin id -> node id の逆引き表を作る。
    for (const auto& pin : asset.pins) {
        pinToNode[pin.id] = pin.nodeId;
    }

    // 各ノードの入次数を初期化し、Output ノード数も数える。
    uint32_t outputNodeCount = 0;
    for (const auto& node : asset.nodes) {
        incomingEdgeCount.emplace(node.id, 0);
        if (node.type == EffectGraphNodeType::Output) {
            ++outputNodeCount;
        }
    }

    // Output ノードはちょうど 1 個でなければならない。
    if (outputNodeCount != 1) {
        compiled->errors.push_back("Exactly one Effect Output node is required.");
    }

    // 副作用ノードの Flow 出力 fan-out 制限チェック用。
    std::unordered_map<uint32_t, uint32_t> sideEffectFlowFanOut;

    // Link を走査して接続妥当性チェックと隣接情報を作る。
    for (const auto& link : asset.links) {
        const EffectGraphPin* startPin = asset.FindPin(link.startPinId);
        const EffectGraphPin* endPin = asset.FindPin(link.endPinId);

        // 存在しないピン参照はエラー。
        if (!startPin || !endPin) {
            compiled->errors.push_back("Link references a missing pin.");
            continue;
        }

        // Output -> Input 以外は禁止。
        if (startPin->kind != EffectPinKind::Output || endPin->kind != EffectPinKind::Input) {
            compiled->errors.push_back("Only Output -> Input links are allowed.");
        }

        // 型不一致接続は禁止。
        if (!IsCompatibleValueType(startPin->valueType, endPin->valueType)) {
            compiled->errors.push_back("Link value types do not match.");
        }

        const uint32_t startNode = pinToNode[link.startPinId];
        const uint32_t endNode = pinToNode[link.endPinId];

        // トポロジカルソート用の有向辺を登録する。
        adjacency[startNode].push_back(endNode);
        ++incomingEdgeCount[endNode];

        // 副作用ノードの Flow 出力は 1 本だけ許可する。
        const auto* startNodePtr = asset.FindNode(startNode);
        if (startPin->valueType == EffectValueType::Flow && startNodePtr && IsEffectSideEffectNode(startNodePtr->type)) {
            ++sideEffectFlowFanOut[startNode];
        }
    }

    // 副作用ノードの fan-out 違反をチェックする。
    for (const auto& [nodeId, fanOut] : sideEffectFlowFanOut) {
        (void)nodeId;
        if (fanOut > 1) {
            compiled->errors.push_back("Side-effect nodes may drive only one flow output.");
        }
    }

    // 入次数 0 のノードからトポロジカルソートを開始する。
    std::queue<uint32_t> ready;
    for (const auto& [nodeId, count] : incomingEdgeCount) {
        if (count == 0) {
            ready.push(nodeId);
        }
    }

    std::vector<uint32_t> topoOrder;
    topoOrder.reserve(asset.nodes.size());

    while (!ready.empty()) {
        const uint32_t nodeId = ready.front();
        ready.pop();

        topoOrder.push_back(nodeId);

        auto adjacencyIt = adjacency.find(nodeId);
        if (adjacencyIt == adjacency.end()) {
            continue;
        }

        for (uint32_t next : adjacencyIt->second) {
            auto countIt = incomingEdgeCount.find(next);
            if (countIt == incomingEdgeCount.end()) {
                continue;
            }

            if (--countIt->second == 0) {
                ready.push(next);
            }
        }
    }

    // 全ノードを処理できなかったなら cycle がある。
    if (topoOrder.size() != asset.nodes.size()) {
        compiled->errors.push_back("Cycle detected. The graph must remain a DAG.");
    }

    // 接続済み pin を収集する。
    std::unordered_set<uint32_t> connectedPins;
    for (const auto& link : asset.links) {
        connectedPins.insert(link.startPinId);
        connectedPins.insert(link.endPinId);
    }

    // Flow 入力が未接続のノードへ warning を出す。
    for (const auto& pin : asset.pins) {
        if (pin.kind == EffectPinKind::Input && pin.valueType == EffectValueType::Flow) {
            const auto* node = asset.FindNode(pin.nodeId);
            if (node && node->type != EffectGraphNodeType::Output && connectedPins.find(pin.id) == connectedPins.end()) {
                compiled->warnings.push_back(node->title + " has an unconnected flow input.");
            }
        }
    }

    // ノードを CompiledNode として stage 別リストへ追加する補助ラムダ。
    auto addCompiledNode = [&](const EffectGraphNode& node) {
        EffectCompiledNode compiledNode;
        compiledNode.sourceNodeId = node.id;
        compiledNode.type = node.type;
        compiledNode.stage = GetEffectNodeStage(node.type);
        compiledNode.debugLabel = node.title;

        switch (compiledNode.stage) {
        case EffectNodeStage::Spawn:
            compiled->executionPlan.spawnNodeIds.push_back(node.id);
            compiled->spawnNodeList.push_back(compiledNode);
            break;

        case EffectNodeStage::Update:
            compiled->executionPlan.updateNodeIds.push_back(node.id);
            compiled->updateNodeList.push_back(compiledNode);
            break;

        case EffectNodeStage::Render:
            compiled->executionPlan.renderNodeIds.push_back(node.id);
            compiled->renderNodeList.push_back(compiledNode);
            break;

        default:
            break;
        }
        };

    // 指定ノードへ wantedType の入力としてつながっている上流ノードを 1 つ探す。
    const auto findConnectedNode = [&](uint32_t targetNodeId, EffectValueType wantedType) -> const EffectGraphNode* {
        for (const auto& link : asset.links) {
            const EffectGraphPin* endPin = asset.FindPin(link.endPinId);
            if (!endPin || endPin->nodeId != targetNodeId || endPin->valueType != wantedType) {
                continue;
            }

            const EffectGraphPin* startPin = asset.FindPin(link.startPinId);
            if (!startPin || startPin->valueType != wantedType) {
                continue;
            }

            return asset.FindNode(startPin->nodeId);
        }
        return nullptr;
        };

    // 指定型のノードをグラフ全体から最初の 1 個だけ探す。
    const auto findFirstNodeOfType = [&](EffectGraphNodeType type) -> const EffectGraphNode* {
        for (const auto& node : asset.nodes) {
            if (node.type == type) {
                return &node;
            }
        }
        return nullptr;
        };

    bool hasColorInputForMesh = false;

    // トポロジカル順にノードを処理して compiled へ落とし込む。
    for (uint32_t nodeId : topoOrder) {
        const auto* node = asset.FindNode(nodeId);
        if (!node) {
            continue;
        }

        addCompiledNode(*node);

        // -------------------------------------------------
        // Lifetime ノード
        // -------------------------------------------------
        if (node->type == EffectGraphNodeType::Lifetime) {
            compiled->duration = node->scalar > 0.0f ? node->scalar : asset.previewDefaults.duration;
        }

        // -------------------------------------------------
        // MeshRenderer ノード
        // -------------------------------------------------
        else if (node->type == EffectGraphNodeType::MeshRenderer) {
            compiled->meshRenderer.enabled = true;

            // MeshRenderer ノードの stringValue は Texture パスとして authoring される。
            // したがって materialAssetPath へ入れてはいけない。
            compiled->meshRenderer.materialAssetPath = asset.previewDefaults.previewMaterialPath;
            compiled->meshRenderer.blendState = static_cast<BlendState>(node->intValue);
            compiled->meshRenderer.depthState = DepthState::TestOnly;
            compiled->meshRenderer.rasterizerState = RasterizerState::SolidCullBack;
            compiled->meshRenderer.tint = node->vectorValue;

            // バリアント用パラメータ群をノード値から組み立てる。
            {
                auto& vp = compiled->meshRenderer.variantParams;

                // B1: shader flags
                vp.shaderFlags = static_cast<uint32_t>(node->intValue2);

                // Base texture は stringValue。
                vp.baseTexturePath = node->stringValue;

                // vectorValue2: {dissolveAmount, dissolveEdge, fresnelPower, flowStrength}
                vp.constants.dissolveAmount = node->vectorValue2.x;
                vp.constants.dissolveEdge = node->vectorValue2.y > 0.0f ? node->vectorValue2.y : 0.05f;
                vp.constants.fresnelPower = node->vectorValue2.z > 0.0f ? node->vectorValue2.z : 3.0f;
                vp.constants.flowStrength = node->vectorValue2.w;

                // vectorValue3: {flowSpeed.x, flowSpeed.y, scrollSpeed.x, scrollSpeed.y}
                vp.constants.flowSpeed = { node->vectorValue3.x, node->vectorValue3.y };
                vp.constants.scrollSpeed = { node->vectorValue3.z, node->vectorValue3.w };

                // vectorValue4: {rimPower, emissionIntensity, distortStrength, alphaFade}
                vp.constants.rimPower = node->vectorValue4.x > 0.0f ? node->vectorValue4.x : 2.0f;
                vp.constants.emissionIntensity = node->vectorValue4.y;
                vp.constants.distortStrength = node->vectorValue4.z;

                // vectorValue5-8: 各種色定数
                if (node->vectorValue5.w > 0.0f || node->vectorValue5.x > 0.0f)
                    vp.constants.dissolveGlowColor = node->vectorValue5;
                if (node->vectorValue6.w > 0.0f || node->vectorValue6.x > 0.0f)
                    vp.constants.fresnelColor = node->vectorValue6;
                if (node->vectorValue7.w > 0.0f || node->vectorValue7.x > 0.0f)
                    vp.constants.rimColor = node->vectorValue7;
                if (node->vectorValue8.w > 0.0f || node->vectorValue8.x > 0.0f)
                    vp.constants.emissionColor = node->vectorValue8;

                // 追加テクスチャ群
                vp.maskTexturePath = node->stringValue2;
                vp.normalMapPath = node->stringValue3;
                vp.flowMapPath = node->stringValue4;
                vp.subTexturePath = node->stringValue5;
                vp.emissionTexPath = node->stringValue6;
            }

            // Mesh 入力がつながっていればそのパスを使う。
            if (const auto* meshNode = findConnectedNode(node->id, EffectValueType::Mesh)) {
                compiled->meshRenderer.meshAssetPath = meshNode->stringValue;
            }

            // 無ければ preview default を使う。
            if (compiled->meshRenderer.meshAssetPath.empty()) {
                compiled->meshRenderer.meshAssetPath = asset.previewDefaults.previewMeshPath;
            }

            // Color 入力があれば tint を上書きする。
            if (const auto* colorNode = findConnectedNode(node->id, EffectValueType::Color)) {
                compiled->meshRenderer.tint = colorNode->vectorValue;
                hasColorInputForMesh = true;
            }
        }

        // -------------------------------------------------
        // ParticleEmitter ノード
        // -------------------------------------------------
        else if (node->type == EffectGraphNodeType::ParticleEmitter) {
            compiled->particleRenderer.enabled = true;

            compiled->particleRenderer.spawnRate = node->scalar > 0.0f ? node->scalar : 32.0f;
            compiled->particleRenderer.burstCount = node->scalar2 > 0.0f ? static_cast<uint32_t>(node->scalar2) : 0u;

            compiled->particleRenderer.particleLifetime = node->vectorValue.x > 0.0f
                ? node->vectorValue.x
                : std::clamp(compiled->duration * 0.45f, 0.35f, 1.5f);

            compiled->particleRenderer.maxParticles = ResolveEffectParticleMaxParticles(
                node->intValue,
                compiled->particleRenderer.spawnRate,
                compiled->particleRenderer.burstCount,
                compiled->particleRenderer.particleLifetime,
                compiled->duration);

            compiled->particleRenderer.startSize = node->vectorValue.y > 0.0f ? node->vectorValue.y : 0.18f;
            compiled->particleRenderer.endSize = node->vectorValue.z >= 0.0f ? node->vectorValue.z : 0.04f;
            compiled->particleRenderer.speed = node->vectorValue.w > 0.0f ? node->vectorValue.w : 1.0f;

            compiled->particleRenderer.acceleration = { node->vectorValue2.x, node->vectorValue2.y, node->vectorValue2.z };
            compiled->particleRenderer.drag = (std::max)(node->vectorValue2.w, 0.0f);
            compiled->particleRenderer.shapeType = static_cast<EffectSpawnShapeType>(node->intValue2);
            compiled->particleRenderer.shapeParameters = { node->vectorValue3.x, node->vectorValue3.y, node->vectorValue3.z };
            compiled->particleRenderer.spinRate = node->vectorValue3.w;
            compiled->particleRenderer.curlNoiseStrength = (std::max)(0.0f, node->vectorValue4.x);
            compiled->particleRenderer.curlNoiseScale = (std::max)(0.01f, node->vectorValue4.y > 0.0f ? node->vectorValue4.y : 0.18f);
            compiled->particleRenderer.curlNoiseScrollSpeed = (std::max)(0.0f, node->vectorValue4.z);
            compiled->particleRenderer.vortexStrength = node->vectorValue4.w;

            // Phase 2: Attractor 設定
            {
                compiled->particleRenderer.attractors[0] = node->vectorValue5;
                compiled->particleRenderer.attractors[1] = node->vectorValue6;

                int aCount = 0;
                if (std::abs(node->vectorValue5.w) > 0.001f) aCount = 1;
                if (std::abs(node->vectorValue6.w) > 0.001f) aCount = 2;
                compiled->particleRenderer.attractorCount = static_cast<uint32_t>(aCount);

                compiled->particleRenderer.attractorRadii = {
                    node->vectorValue7.x > 0.0f ? node->vectorValue7.x : 5.0f,
                    node->vectorValue7.y > 0.0f ? node->vectorValue7.y : 5.0f,
                    5.0f, 5.0f
                };

                compiled->particleRenderer.attractorFalloff = {
                    node->vectorValue7.z, node->vectorValue7.w, 1.0f, 1.0f
                };
            }

            // Phase 2: Collision 設定
            {
                compiled->particleRenderer.collisionPlane = node->vectorValue8;
                compiled->particleRenderer.collisionRestitution = node->vectorValue9.x > 0.0f ? node->vectorValue9.x : 0.5f;
                compiled->particleRenderer.collisionFriction = node->vectorValue9.y >= 0.0f ? node->vectorValue9.y : 0.3f;

                const float planeLen = node->vectorValue8.x * node->vectorValue8.x
                    + node->vectorValue8.y * node->vectorValue8.y
                    + node->vectorValue8.z * node->vectorValue8.z;

                compiled->particleRenderer.collisionEnabled =
                    (planeLen > 0.5f) || (node->vectorValue9.z > 0.0f);

                compiled->particleRenderer.collisionSphereCount = 0;
            }

            // 想定上限より maxParticles が小さすぎるなら warning を出す。
            if (IsEffectParticleMaxParticlesTooLow(
                node->intValue,
                compiled->particleRenderer.spawnRate,
                compiled->particleRenderer.burstCount,
                compiled->particleRenderer.particleLifetime,
                compiled->duration)) {
                compiled->warnings.push_back(
                    "Particle emitter max particles is lower than the recommended capacity for its spawn/lifetime settings.");
            }
        }

        // -------------------------------------------------
        // SpriteRenderer ノード
        // -------------------------------------------------
        else if (node->type == EffectGraphNodeType::SpriteRenderer) {
            compiled->particleRenderer.enabled = true;

            const float alphaScale = node->vectorValue2.z > 0.0f ? node->vectorValue2.z : 1.0f;

            compiled->particleRenderer.drawMode = static_cast<EffectParticleDrawMode>(node->intValue);
            compiled->particleRenderer.sortMode = static_cast<EffectParticleSortMode>(node->intValue2);
            compiled->particleRenderer.texturePath = node->stringValue.empty()
                ? "Data/Effect/particle/particle.png"
                : node->stringValue;

            compiled->particleRenderer.tint = node->vectorValue;
            compiled->particleRenderer.tintEnd = { node->vectorValue.x, node->vectorValue.y, node->vectorValue.z, 0.0f };
            compiled->particleRenderer.ribbonWidth = (std::max)(0.02f, node->vectorValue2.x > 0.0f ? node->vectorValue2.x : compiled->particleRenderer.startSize * 0.45f);
            compiled->particleRenderer.ribbonVelocityStretch = (std::max)(0.05f, node->vectorValue2.y > 0.0f ? node->vectorValue2.y : 0.32f);
            compiled->particleRenderer.sizeCurveBias = (std::max)(0.05f, node->vectorValue3.x > 0.0f ? node->vectorValue3.x : 1.0f);
            compiled->particleRenderer.alphaCurveBias = (std::max)(0.05f, node->vectorValue3.y > 0.0f ? node->vectorValue3.y : 1.0f);
            compiled->particleRenderer.subUvColumns = (std::max)(1u, static_cast<uint32_t>(std::round(node->vectorValue3.z > 0.0f ? node->vectorValue3.z : 1.0f)));
            compiled->particleRenderer.subUvRows = (std::max)(1u, static_cast<uint32_t>(std::round(node->vectorValue3.w > 0.0f ? node->vectorValue3.w : 1.0f)));
            compiled->particleRenderer.subUvFrameRate = (std::max)(0.0f, node->vectorValue2.w);
            compiled->particleRenderer.softParticleEnabled = node->boolValue;
            compiled->particleRenderer.softParticleScale = (std::max)(0.0f, node->scalar);

            // scalar2 を blend mode enum として解釈する。
            {
                int blendInt = static_cast<int>(node->scalar2);
                if (blendInt >= 0 && blendInt < static_cast<int>(EffectParticleBlendMode::EnumCount)) {
                    compiled->particleRenderer.blendMode = static_cast<EffectParticleBlendMode>(blendInt);
                }
            }

            compiled->particleRenderer.randomSpeedRange = std::clamp(node->vectorValue4.x, 0.0f, 1.0f);
            compiled->particleRenderer.randomSizeRange = std::clamp(node->vectorValue4.y, 0.0f, 1.0f);
            compiled->particleRenderer.randomLifeRange = std::clamp(node->vectorValue4.z, 0.0f, 1.0f);
            compiled->particleRenderer.windStrength = node->vectorValue4.w;

            // Color 入力ノードを探す。
            const EffectGraphNode* colorNode = findConnectedNode(node->id, EffectValueType::Color);
            if (!colorNode) {
                colorNode = findFirstNodeOfType(EffectGraphNodeType::Color);
            }

            if (colorNode) {
                compiled->particleRenderer.tint = colorNode->vectorValue;
                compiled->particleRenderer.tintEnd = colorNode->vectorValue2;

                // Phase 1C: 4キー gradient 対応
                const int gradKeys = colorNode->intValue;
                if (gradKeys >= 3 && gradKeys <= 4) {
                    compiled->particleRenderer.gradientKeyCount = static_cast<uint32_t>(gradKeys);
                    compiled->particleRenderer.gradientColor0 = colorNode->vectorValue;
                    compiled->particleRenderer.gradientColor1 = colorNode->vectorValue3;
                    compiled->particleRenderer.gradientColor2 = colorNode->vectorValue4;
                    compiled->particleRenderer.gradientColor3 = colorNode->vectorValue2;
                    compiled->particleRenderer.gradientMidTimes = {
                        (std::max)(0.01f, colorNode->scalar > 0.0f ? colorNode->scalar : 0.33f),
                        (std::max)(0.02f, colorNode->scalar2 > 0.0f ? colorNode->scalar2 : 0.66f)
                    };
                }
                else {
                    // 2キー gradient 扱い
                    compiled->particleRenderer.gradientKeyCount = 2;
                    compiled->particleRenderer.gradientColor0 = colorNode->vectorValue;
                    compiled->particleRenderer.gradientColor3 = colorNode->vectorValue2;
                    compiled->particleRenderer.gradientColor1 = colorNode->vectorValue;
                    compiled->particleRenderer.gradientColor2 = colorNode->vectorValue2;
                    compiled->particleRenderer.gradientMidTimes = { 0.0f, 1.0f };
                }
            }

            // alphaScale を tint/tintEnd の alpha に掛ける。
            compiled->particleRenderer.tint.w *= alphaScale;
            compiled->particleRenderer.tintEnd.w *= alphaScale;

            // Phase 1C: Size curve 4キー対応
            {
                const int sizeKeys = static_cast<int>(node->vectorValue6.w);
                if (sizeKeys >= 3 && sizeKeys <= 4) {
                    compiled->particleRenderer.sizeCurveKeyCount = static_cast<uint32_t>(sizeKeys);
                    compiled->particleRenderer.sizeCurveValues = node->vectorValue5;
                    compiled->particleRenderer.sizeCurveTimes = {
                        0.0f,
                        (std::max)(0.01f, node->vectorValue6.x),
                        (std::max)(0.02f, node->vectorValue6.y),
                        1.0f
                    };
                }
                else {
                    compiled->particleRenderer.sizeCurveKeyCount = 2;
                    compiled->particleRenderer.sizeCurveValues = {
                        compiled->particleRenderer.startSize,
                        compiled->particleRenderer.startSize,
                        compiled->particleRenderer.endSize,
                        compiled->particleRenderer.endSize
                    };
                    compiled->particleRenderer.sizeCurveTimes = { 0.0f, 0.0f, 1.0f, 1.0f };
                }
            }

            // Mesh draw mode の場合は preview default mesh を使う。
            if (compiled->particleRenderer.drawMode == EffectParticleDrawMode::Mesh) {
                compiled->particleRenderer.meshAssetPath = asset.previewDefaults.previewMeshPath;
            }
        }
    }

    // Exposed Parameter を compiled 側へ反映する。
    for (const auto& parameter : asset.exposedParameters) {
        EffectCompiledParameter compiledParameter;
        compiledParameter.name = parameter.name;
        compiledParameter.valueType = parameter.valueType;
        compiledParameter.defaultValue = parameter.defaultValue;
        compiled->constantParameters.push_back(std::move(compiledParameter));

        // Float parameter は各 descriptor に適用する。
        if (parameter.valueType == EffectValueType::Float) {
            EffectParameterBindings::ApplyFloatParameter(
                parameter.name,
                parameter.defaultValue.x,
                compiled->meshRenderer,
                compiled->particleRenderer,
                compiled->duration);
        }
        // Color parameter は各 descriptor に適用する。
        else if (parameter.valueType == EffectValueType::Color) {
            EffectParameterBindings::ApplyColorParameter(
                parameter.name,
                parameter.defaultValue,
                compiled->meshRenderer,
                compiled->particleRenderer);
        }
    }

    // MeshRenderer が有効なら必要リソースと shader variant key を登録する。
    if (compiled->meshRenderer.enabled) {
        compiled->meshRenderer.shaderVariantKey = BuildShaderVariantKey(compiled->meshRenderer, hasColorInputForMesh);
        compiled->shaderVariantKeys.push_back(compiled->meshRenderer.shaderVariantKey);

        if (!compiled->meshRenderer.meshAssetPath.empty()) {
            compiled->requiredAssetReferences.push_back(compiled->meshRenderer.meshAssetPath);
        }
        if (!compiled->meshRenderer.materialAssetPath.empty()) {
            compiled->requiredAssetReferences.push_back(compiled->meshRenderer.materialAssetPath);
        }
    }

    // ParticleRenderer が有効なら必要リソースと variant key を登録する。
    if (compiled->particleRenderer.enabled) {
        compiled->shaderVariantKeys.push_back(1u << 8);

        if (!compiled->particleRenderer.texturePath.empty()) {
            compiled->requiredAssetReferences.push_back(compiled->particleRenderer.texturePath);
        }
        if (!compiled->particleRenderer.meshAssetPath.empty()) {
            compiled->requiredAssetReferences.push_back(compiled->particleRenderer.meshAssetPath);
        }

        // Billboard 方式で maxParticles が仕様下限より小さいなら warning を出す。
        if (compiled->particleRenderer.drawMode == EffectParticleDrawMode::Billboard &&
            compiled->particleRenderer.maxParticles < 5000000u) {
            compiled->warnings.push_back(
                "Billboard particle budget is below the 5,000,000 active minimum defined by the DX12 overhaul spec.");
        }
    }

    // エラーが無ければ valid。
    compiled->valid = compiled->errors.empty();

    // コンパイル結果をログへ出す。
    LOG_INFO("[EffectCompile] valid=%d errors=%zu mesh.enabled=%d mesh='%s' mat='%s' variantKey=0x%08X particle.enabled=%d duration=%.2f",
        compiled->valid ? 1 : 0,
        compiled->errors.size(),
        compiled->meshRenderer.enabled ? 1 : 0,
        compiled->meshRenderer.meshAssetPath.c_str(),
        compiled->meshRenderer.materialAssetPath.c_str(),
        compiled->meshRenderer.shaderVariantKey,
        compiled->particleRenderer.enabled ? 1 : 0,
        compiled->duration);

    // エラー詳細も個別にログへ出す。
    for (const auto& err : compiled->errors) {
        LOG_ERROR("[EffectCompile] error: %s", err.c_str());
    }

    return compiled;
}