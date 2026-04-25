#include "EffectEditorTemplates.h"
#include "EffectEditorPanel.h"
#include "EffectEditorPanelInternal.h"

#include <algorithm>
#include <vector>
#include <imgui.h>

#include "EffectRuntime/EffectGraphAsset.h"

void EffectEditorTemplates::DrawMenuContents(EffectEditorPanel& panel)
{
    const auto applyTemplate = [&](const char* effectName,
                                   float duration,
                                   float spawnRate,
                                   uint32_t burstCount,
                                   float particleLifetime,
                                   float startSize,
                                   float endSize,
                                   float speed,
                                   EffectSpawnShapeType shapeType,
                                   const DirectX::XMFLOAT3& acceleration,
                                   float drag,
                                   const DirectX::XMFLOAT3& shapeParams,
                                   float spinRate,
                                   EffectParticleDrawMode drawMode,
                                   EffectParticleSortMode sortMode,
                                   float ribbonWidth,
                                   float ribbonStretch,
                                   const char* texturePath,
                                   uint32_t subUvColumns,
                                   uint32_t subUvRows,
                                   float subUvFrameRate,
                                   float curlNoiseStrength,
                                   float curlNoiseScale,
                                   float curlNoiseScrollSpeed,
                                   float vortexStrength,
                                   const DirectX::XMFLOAT4& startColor,
                                   const DirectX::XMFLOAT4& endColor) {
        const auto resolveTemplateMaxParticles = [](float duration, float spawnRate, uint32_t burstCount, float particleLifetime, EffectParticleDrawMode /*drawMode*/) {
            return static_cast<int>(ResolveEffectParticleMaxParticles(
                0, spawnRate, burstCount, particleLifetime, duration));
        };

        panel.m_asset.name = effectName;
        panel.m_asset.previewDefaults.duration = duration;

        // Remove mesh-only nodes so particle graph has no side-effect fan-out on Lifetime.
        const auto removeNodeIfExists = [&](EffectGraphNodeType type) {
            while (EffectGraphNode* n = panel.FindNodeByType(type)) {
                const uint32_t nid = n->id;
                std::vector<uint32_t> pinIds;
                for (const auto& p : panel.m_asset.pins)
                    if (p.nodeId == nid) pinIds.push_back(p.id);
                panel.m_asset.links.erase(std::remove_if(panel.m_asset.links.begin(), panel.m_asset.links.end(),
                    [&](const EffectGraphLink& l) {
                        return std::find(pinIds.begin(), pinIds.end(), l.startPinId) != pinIds.end() ||
                               std::find(pinIds.begin(), pinIds.end(), l.endPinId) != pinIds.end();
                    }), panel.m_asset.links.end());
                panel.m_asset.pins.erase(std::remove_if(panel.m_asset.pins.begin(), panel.m_asset.pins.end(),
                    [nid](const EffectGraphPin& p) { return p.nodeId == nid; }), panel.m_asset.pins.end());
                panel.m_asset.nodes.erase(std::remove_if(panel.m_asset.nodes.begin(), panel.m_asset.nodes.end(),
                    [nid](const EffectGraphNode& nd) { return nd.id == nid; }), panel.m_asset.nodes.end());
            }
        };
        removeNodeIfExists(EffectGraphNodeType::MeshRenderer);
        removeNodeIfExists(EffectGraphNodeType::MeshSource);
        // Ensure all nodes exist first. Pointers returned by EnsureNodeByType
        // can be invalidated by subsequent push_back into m_asset.nodes, so we
        // re-query via FindNodeByType after every node has been created.
        panel.EnsureNodeByType(EffectGraphNodeType::Spawn);
        panel.EnsureNodeByType(EffectGraphNodeType::Output);
        panel.EnsureNodeByType(EffectGraphNodeType::Lifetime);
        panel.EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
        panel.EnsureNodeByType(EffectGraphNodeType::Color);
        panel.EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);

        EffectGraphNode* lifetimeNode = panel.FindNodeByType(EffectGraphNodeType::Lifetime);
        EffectGraphNode* emitterNode  = panel.FindNodeByType(EffectGraphNodeType::ParticleEmitter);
        EffectGraphNode* colorNode    = panel.FindNodeByType(EffectGraphNodeType::Color);
        EffectGraphNode* spriteNode   = panel.FindNodeByType(EffectGraphNodeType::SpriteRenderer);
        if (!lifetimeNode || !emitterNode || !colorNode || !spriteNode) {
            return;
        }

        lifetimeNode->scalar = duration;
        emitterNode->scalar = spawnRate;
        emitterNode->scalar2 = static_cast<float>(burstCount);
        emitterNode->intValue = resolveTemplateMaxParticles(duration, spawnRate, burstCount, particleLifetime, drawMode);
        emitterNode->vectorValue = { particleLifetime, startSize, endSize, speed };
        emitterNode->vectorValue2 = { acceleration.x, acceleration.y, acceleration.z, drag };
        emitterNode->vectorValue3 = { shapeParams.x, shapeParams.y, shapeParams.z, spinRate };
        emitterNode->vectorValue4 = { curlNoiseStrength, curlNoiseScale, curlNoiseScrollSpeed, vortexStrength };
        emitterNode->intValue2 = static_cast<int>(shapeType);

        colorNode->vectorValue = startColor;
        colorNode->vectorValue2 = endColor;

        spriteNode->intValue = static_cast<int>(drawMode);
        spriteNode->intValue2 = static_cast<int>(sortMode);
        spriteNode->vectorValue = startColor;
        spriteNode->vectorValue2 = { ribbonWidth, ribbonStretch, 1.0f, subUvFrameRate };
        spriteNode->vectorValue3.z = static_cast<float>((std::max)(subUvColumns, 1u));
        spriteNode->vectorValue3.w = static_cast<float>((std::max)(subUvRows, 1u));
        spriteNode->stringValue = texturePath && texturePath[0] != '\0'
            ? texturePath
            : "Data/Effect/particle/particle.png";

        EffectEditorInternal::EnsureGuiAuthoringLinks(panel.m_asset);
        EffectEditorInternal::SanitizeGraphAsset(panel.m_asset);
        EffectEditorInternal::LogGraphStructure(panel.m_asset, "applyTemplate");
        panel.m_compileDirty = true;
        panel.m_syncNodePositions = true;
        panel.StopPreview();
        ImGui::CloseCurrentPopup();
    };

    if (ImGui::MenuItem("Spark Fountain")) {
        applyTemplate(
            "Spark Fountain", 3.2f, 70000.0f, 0u, 1.45f, 0.08f, 0.025f, 4.2f,
            EffectSpawnShapeType::Sphere, { 0.0f, -2.2f, 0.0f }, 0.02f, { 0.26f, 0.26f, 0.26f }, 13.0f,
            EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront, 0.08f, 0.35f,
            "Data/Effect/particle/spark_03.png", 1u, 1u, 0.0f,
            0.30f, 0.35f, 0.30f, 0.0f,
            { 0.65f, 0.95f, 1.00f, 1.0f }, { 0.15f, 0.75f, 1.00f, 0.0f });
    }
    if (ImGui::MenuItem("Smoke Plume")) {
        applyTemplate(
            "Smoke Plume", 6.0f, 24000.0f, 0u, 4.2f, 0.18f, 1.55f, 0.95f,
            EffectSpawnShapeType::Sphere, { 0.0f, 0.18f, 0.0f }, 0.12f, { 0.65f, 0.65f, 0.65f }, 1.2f,
            EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront, 0.14f, 0.28f,
            "Data/Effect/particle/smoke_03.png", 1u, 1u, 0.0f,
            0.75f, 0.10f, 0.07f, 0.28f,
            { 0.42f, 0.43f, 0.45f, 0.82f }, { 0.10f, 0.10f, 0.10f, 0.0f });
    }
    if (ImGui::MenuItem("Magic Burst")) {
        applyTemplate(
            "Magic Burst", 2.1f, 90000.0f, 0u, 1.10f, 0.14f, 0.028f, 7.8f,
            EffectSpawnShapeType::Sphere, { 0.0f, -0.55f, 0.0f }, 0.01f, { 0.22f, 0.22f, 0.22f }, 22.0f,
            EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront, 0.08f, 0.30f,
            "Data/Effect/particle/magic_03.png", 1u, 1u, 0.0f,
            0.55f, 0.26f, 0.40f, 1.65f,
            { 0.95f, 0.45f, 1.00f, 1.0f }, { 0.35f, 0.15f, 1.00f, 0.0f });
    }
    if (ImGui::MenuItem("Ribbon Trail")) {
        applyTemplate(
            "Ribbon Trail", 2.8f, 3000.0f, 0u, 1.20f, 0.08f, 0.025f, 1.9f,
            EffectSpawnShapeType::Line, { 0.0f, 0.0f, 0.0f }, 0.03f, { 0.40f, 0.0f, 0.0f }, 3.0f,
            EffectParticleDrawMode::Ribbon, EffectParticleSortMode::BackToFront, 0.12f, 1.75f,
            "Data/Effect/particle/trace_03.png", 1u, 1u, 0.0f,
            0.16f, 0.22f, 0.24f, 2.10f,
            { 0.30f, 0.95f, 1.00f, 0.95f }, { 0.08f, 0.28f, 1.00f, 0.0f });
    }

    ImGui::Separator();
    ImGui::TextDisabled("-- Mesh Effects --");

    // applyMeshTemplate: sets up MeshSource + MeshRenderer nodes
    const auto applyMeshTemplate = [&](
        const char* effectName,
        float duration,
        const char* meshPath,
        int blendState,          // 2=Additive
        int shaderFlags,
        const DirectX::XMFLOAT4& tint,
        // vectorValue2: {dissolveAmount, dissolveEdge, fresnelPower, flowStrength}
        float dissolveAmount, float dissolveEdge, float fresnelPower, float flowStrength,
        // vectorValue3: {flowSpeedX, flowSpeedY, scrollSpeedX, scrollSpeedY}
        float flowSpeedX, float flowSpeedY, float scrollSpeedX, float scrollSpeedY,
        // colors
        const DirectX::XMFLOAT4& dissolveGlowColor,
        const DirectX::XMFLOAT4& fresnelColor,
        // textures
        const char* baseTexPath,
        const char* maskTexPath,
        const char* flowMapPath)
    {
        panel.m_asset.name = effectName;
        panel.m_asset.previewDefaults.duration = duration;
        panel.m_asset.previewDefaults.previewMeshPath = meshPath ? meshPath : "";

        const auto removeNodeIfExists = [&](EffectGraphNodeType type) {
            while (EffectGraphNode* n = panel.FindNodeByType(type)) {
                const uint32_t nid = n->id;
                std::vector<uint32_t> pinIds;
                for (const auto& p : panel.m_asset.pins)
                    if (p.nodeId == nid) pinIds.push_back(p.id);
                panel.m_asset.links.erase(std::remove_if(panel.m_asset.links.begin(), panel.m_asset.links.end(),
                    [&](const EffectGraphLink& l) {
                        return std::find(pinIds.begin(), pinIds.end(), l.startPinId) != pinIds.end() ||
                               std::find(pinIds.begin(), pinIds.end(), l.endPinId) != pinIds.end();
                    }), panel.m_asset.links.end());
                panel.m_asset.pins.erase(std::remove_if(panel.m_asset.pins.begin(), panel.m_asset.pins.end(),
                    [nid](const EffectGraphPin& p) { return p.nodeId == nid; }), panel.m_asset.pins.end());
                panel.m_asset.nodes.erase(std::remove_if(panel.m_asset.nodes.begin(), panel.m_asset.nodes.end(),
                    [nid](const EffectGraphNode& nd) { return nd.id == nid; }), panel.m_asset.nodes.end());
            }
        };
        removeNodeIfExists(EffectGraphNodeType::ParticleEmitter);
        removeNodeIfExists(EffectGraphNodeType::SpriteRenderer);
        removeNodeIfExists(EffectGraphNodeType::Color);
        panel.EnsureNodeByType(EffectGraphNodeType::Spawn);
        panel.EnsureNodeByType(EffectGraphNodeType::Output);
        panel.EnsureNodeByType(EffectGraphNodeType::Lifetime);
        panel.EnsureNodeByType(EffectGraphNodeType::MeshSource);
        panel.EnsureNodeByType(EffectGraphNodeType::MeshRenderer);

        EffectGraphNode* lifetimeNode = panel.FindNodeByType(EffectGraphNodeType::Lifetime);
        EffectGraphNode* meshSrcNode  = panel.FindNodeByType(EffectGraphNodeType::MeshSource);
        EffectGraphNode* meshRendNode = panel.FindNodeByType(EffectGraphNodeType::MeshRenderer);
        if (!lifetimeNode || !meshSrcNode || !meshRendNode) {
            return;
        }

        lifetimeNode->scalar = duration;

        meshSrcNode->stringValue = meshPath ? meshPath : "";

        meshRendNode->intValue  = blendState;
        meshRendNode->intValue2 = shaderFlags;
        meshRendNode->vectorValue  = tint;
        meshRendNode->vectorValue2 = { dissolveAmount, dissolveEdge, fresnelPower, flowStrength };
        meshRendNode->vectorValue3 = { flowSpeedX, flowSpeedY, scrollSpeedX, scrollSpeedY };
        meshRendNode->vectorValue4 = { 0.0f, 0.0f, 0.0f, 0.0f };
        meshRendNode->vectorValue5 = dissolveGlowColor;
        meshRendNode->vectorValue6 = fresnelColor;
        meshRendNode->vectorValue7 = { 0.0f, 0.0f, 0.0f, 0.0f };
        meshRendNode->vectorValue8 = { 0.0f, 0.0f, 0.0f, 0.0f };
        meshRendNode->stringValue  = baseTexPath  ? baseTexPath  : "";
        meshRendNode->stringValue2 = maskTexPath  ? maskTexPath  : "";
        meshRendNode->stringValue3.clear();
        meshRendNode->stringValue4 = flowMapPath  ? flowMapPath  : "";
        meshRendNode->stringValue5.clear();
        meshRendNode->stringValue6.clear();

        EffectEditorInternal::EnsureGuiAuthoringLinks(panel.m_asset);
        EffectEditorInternal::SanitizeGraphAsset(panel.m_asset);
        EffectEditorInternal::LogGraphStructure(panel.m_asset, "applyMeshTemplate");
        panel.m_compileDirty = true;
        panel.m_syncNodePositions = true;
        panel.StopPreview();
        ImGui::CloseCurrentPopup();
    };

    // Texture | Dissolve | DissolveGlow | AlphaFade | Scroll | FlowMap
    static constexpr int kMeshFlag_SlashGlow  = 0x001 | 0x002 | 0x200 | 0x4000 | 0x100000 | 0x1000;
    // Texture | FlowMap | Scroll | AlphaFade
    static constexpr int kMeshFlag_MagicCircle = 0x001 | 0x1000 | 0x100000 | 0x4000;
    // Texture | Dissolve | AlphaFade
    static constexpr int kMeshFlag_Shockwave  = 0x001 | 0x002 | 0x4000;
    // Texture | Fresnel | FlowMap | AlphaFade
    static constexpr int kMeshFlag_TornadoAura = 0x001 | 0x020 | 0x1000 | 0x4000;

    if (ImGui::MenuItem("Sword Slash Glow")) {
        applyMeshTemplate(
            "Sword Slash Glow", 1.2f,
            "Data/Model/Slash/fbx_slash_001_1.fbx",
            2, kMeshFlag_SlashGlow,
            { 1.0f, 0.9f, 0.35f, 1.0f },
            0.0f, 0.10f, 1.0f, 0.20f,
            0.0f, 0.0f, 2.5f, 0.0f,
            { 1.0f, 0.5f, 0.1f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },
            "Data/Effect/Effect/Aura01_T.png",
            "Data/Effect/Mask/dissolve_animation.png",
            "Data/Effect/Flow/Flow.png");
    }
    if (ImGui::MenuItem("Magic Circle")) {
        applyMeshTemplate(
            "Magic Circle", 3.0f,
            "Data/Model/ring/fbx_ring_002.fbx",
            2, kMeshFlag_MagicCircle,
            { 0.55f, 0.25f, 1.0f, 1.0f },
            0.0f, 0.05f, 1.0f, 0.3f,
            0.2f, 0.1f, 0.15f, 0.0f,
            { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.6f, 0.3f, 1.0f, 1.0f },
            "Data/Effect/Effect/AuroraRing.png",
            nullptr,
            "Data/Effect/Flow/Flow.png");
    }
    if (ImGui::MenuItem("Shockwave")) {
        applyMeshTemplate(
            "Shockwave", 0.6f,
            "Data/Model/shockwave/fbx_shockwave_001.fbx",
            2, kMeshFlag_Shockwave,
            { 0.75f, 0.95f, 1.0f, 1.0f },
            0.0f, 0.05f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },
            "Data/Effect/Effect/Burst01.png",
            "Data/Effect/Mask/dissolve_animation.png",
            nullptr);
    }
    if (ImGui::MenuItem("Tornado Aura")) {
        applyMeshTemplate(
            "Tornado Aura", 4.0f,
            "Data/Model/cylinderTornade/fbx_cylinderTornade_001.fbx",
            2, kMeshFlag_TornadoAura,
            { 0.25f, 1.0f, 0.45f, 1.0f },
            0.0f, 0.05f, 2.5f, 0.4f,
            0.1f, 0.3f, 0.0f, 0.0f,
            { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.2f, 1.0f, 0.5f, 1.0f },
            "Data/Effect/Effect/Aura.png",
            nullptr,
            "Data/Effect/Flow/Flow.png");
    }

    ImGui::Separator();
    ImGui::TextDisabled("-- Mesh Particle Effects --");

    // Mesh-mode particles: SoA v3. Reuses applyTemplate (drawMode=Mesh) and
    // stamps mesh-only slots on the SpriteRenderer node afterwards.
    const auto applyMeshParticleTemplate = [&](
        const char* effectName,
        float duration,
        float spawnRate,
        uint32_t burstCount,
        float particleLifetime,
        float speed,
        EffectSpawnShapeType shapeType,
        const DirectX::XMFLOAT3& acceleration,
        float drag,
        const DirectX::XMFLOAT3& shapeParams,
        const char* meshPath,
        const DirectX::XMFLOAT3& meshScale,
        float meshScaleRandom,
        const DirectX::XMFLOAT3& angularAxis,
        float angularSpeed,
        const DirectX::XMFLOAT3& angularOrientRandom,
        float angularSpeedRandom,
        const DirectX::XMFLOAT4& startColor,
        const DirectX::XMFLOAT4& endColor)
    {
        applyTemplate(
            effectName, duration, spawnRate, burstCount,
            particleLifetime, 1.0f, 1.0f, speed,
            shapeType, acceleration, drag, shapeParams, 0.0f,
            EffectParticleDrawMode::Mesh, EffectParticleSortMode::BackToFront,
            0.08f, 0.30f,
            "", 1u, 1u, 0.0f,
            0.0f, 0.18f, 0.20f, 0.0f,
            startColor, endColor);

        EffectGraphNode* spriteNode = panel.FindNodeByType(EffectGraphNodeType::SpriteRenderer);
        if (!spriteNode) {
            return;
        }
        spriteNode->stringValue2 = meshPath ? meshPath : "";
        spriteNode->vectorValue5 = { meshScale.x, meshScale.y, meshScale.z, std::clamp(meshScaleRandom, 0.0f, 1.0f) };
        spriteNode->vectorValue6 = { angularAxis.x, angularAxis.y, angularAxis.z, angularSpeed };
        spriteNode->vectorValue7 = { angularOrientRandom.x, angularOrientRandom.y, angularOrientRandom.z,
                                     std::clamp(angularSpeedRandom, 0.0f, 1.0f) };
        panel.m_compileDirty = true;
    };

    if (ImGui::MenuItem("Debris (Mesh)")) {
        applyMeshParticleTemplate(
            "Debris (Mesh)", 2.5f, 0.0f, 48u, 1.8f, 3.2f,
            EffectSpawnShapeType::Sphere, { 0.0f, -6.5f, 0.0f }, 0.12f, { 0.18f, 0.18f, 0.18f },
            "Data/Model/Cube/cube.fbx",
            { 0.12f, 0.12f, 0.12f }, 0.35f,
            { 0.0f, 1.0f, 0.0f }, 3.5f,
            { 0.25f, 0.25f, 0.25f }, 0.4f,
            { 0.78f, 0.62f, 0.45f, 1.0f }, { 0.35f, 0.28f, 0.22f, 0.0f });
    }
    if (ImGui::MenuItem("Spinning Shards")) {
        applyMeshParticleTemplate(
            "Spinning Shards", 1.8f, 0.0f, 32u, 1.4f, 5.5f,
            EffectSpawnShapeType::Sphere, { 0.0f, -1.8f, 0.0f }, 0.05f, { 0.22f, 0.22f, 0.22f },
            "Data/Model/Cube/cube.fbx",
            { 0.08f, 0.08f, 0.18f }, 0.4f,
            { 0.0f, 1.0f, 0.0f }, 14.0f,
            { 1.0f, 1.0f, 1.0f }, 0.6f,
            { 0.65f, 0.85f, 1.00f, 1.0f }, { 0.25f, 0.45f, 1.00f, 0.0f });
    }

    ImGui::Separator();
    // ---- CharacterTrailEffects templates (spec 2026-04-25, reworked) ----
    // Pair these with EffectAttachmentComponent on the entity to follow
    // a socket; enable velocityModulate to drive spawn rate by speed.
    const auto stampBlendMode = [&](EffectParticleBlendMode mode) {
        if (EffectGraphNode* sn = panel.FindNodeByType(EffectGraphNodeType::SpriteRenderer)) {
            sn->scalar2 = static_cast<float>(mode);
            panel.m_compileDirty = true;
        }
    };
    if (ImGui::MenuItem("Sword Trail")) {
        applyTemplate(
            "Sword Trail", 4.0f, 1200.0f, 0u,
            0.30f, 0.20f, 0.04f,
            1.4f, EffectSpawnShapeType::Line,
            { 0.0f, 0.0f, 0.0f }, 0.0f,
            { 0.30f, 0.0f, 0.0f }, 0.0f,
            EffectParticleDrawMode::Ribbon, EffectParticleSortMode::BackToFront,
            0.16f, 1.10f,
            "Data/Effect/particle/trace_03.png", 1u, 1u, 0.0f,
            0.10f, 0.25f, 0.50f,
            0.0f,
            { 0.85f, 0.95f, 1.00f, 1.0f },
            { 0.30f, 0.70f, 1.00f, 0.0f });
        stampBlendMode(EffectParticleBlendMode::Additive);
    }
    if (ImGui::MenuItem("Lightning Trail")) {
        applyTemplate(
            "Lightning Trail", 4.0f, 8000.0f, 24u,
            0.22f, 0.14f, 0.02f,
            2.4f, EffectSpawnShapeType::Sphere,
            { 0.0f, 0.6f, 0.0f }, 0.05f,
            { 0.06f, 0.06f, 0.06f }, 0.0f,
            EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront,
            0.08f, 0.30f,
            "Data/Effect/particle/spark_03.png", 1u, 1u, 0.0f,
            0.65f, 0.18f, 1.80f,
            0.0f,
            { 1.00f, 0.95f, 0.50f, 1.0f },
            { 0.30f, 0.55f, 1.00f, 0.0f });
        stampBlendMode(EffectParticleBlendMode::Additive);
    }
    if (ImGui::MenuItem("Afterimage Trail")) {
        applyTemplate(
            "Afterimage Trail", 4.0f, 0.0f, 0u,
            0.55f, 0.55f, 0.20f,
            0.0f, EffectSpawnShapeType::Sphere,
            { 0.0f, 0.0f, 0.0f }, 0.0f,
            { 0.10f, 0.10f, 0.10f }, 0.0f,
            EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront,
            0.08f, 0.30f,
            "Data/Effect/particle/Particle04_bokashi_soft.png", 1u, 1u, 0.0f,
            0.0f, 0.18f, 0.10f, 0.0f,
            { 0.55f, 0.85f, 1.00f, 0.85f },
            { 0.55f, 0.85f, 1.00f, 0.00f });
        stampBlendMode(EffectParticleBlendMode::Additive);
    }
}
