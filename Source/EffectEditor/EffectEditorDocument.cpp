// Document persistence + runtime preview lifecycle for EffectEditorPanel.
// Extracted from EffectEditorPanel.cpp; method bodies stay on the panel class.

#include "EffectEditorPanel.h"
#include "EffectEditorPanelInternal.h"

#include <cstring>
#include <filesystem>

#include "Component/EffectAssetComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/EffectSpawnRequestComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "EffectRuntime/EffectCompiler.h"
#include "EffectRuntime/EffectGraphSerializer.h"
#include "EffectRuntime/EffectRuntimeRegistry.h"
#include "Entity/Entity.h"
#include "Registry/Registry.h"

using namespace EffectEditorInternal;

void EffectEditorPanel::NewDocument()
{
    StopPreview();
    m_asset = CreateDefaultEffectGraphAsset();
    m_compiled.reset();
    m_selectedNodeId = 0;
    m_selectedLinkId = 0;
    m_compileDirty = true;
    m_syncNodePositions = true;
}

bool EffectEditorPanel::SaveDocument()
{
    m_documentPath = m_documentPathBuffer;
    if (!std::filesystem::path(m_documentPath).has_parent_path()) {
        m_documentPath = "Data/EffectGraph/" + m_documentPath;
    }
    strcpy_s(m_documentPathBuffer, m_documentPath.c_str());
    return EffectGraphSerializer::Save(m_documentPath, m_asset);
}

bool EffectEditorPanel::LoadDocument()
{
    m_documentPath = m_documentPathBuffer;
    EffectGraphAsset loadedAsset;
    if (!EffectGraphSerializer::Load(m_documentPath, loadedAsset)) {
        return false;
    }
    SanitizeGraphAsset(loadedAsset);
    StopPreview();
    m_asset = std::move(loadedAsset);
    strcpy_s(m_documentPathBuffer, m_documentPath.c_str());
    m_selectedNodeId = 0;
    m_selectedLinkId = 0;
    m_compiled.reset();
    m_compileDirty = true;
    m_syncNodePositions = true;
    return true;
}

std::string EffectEditorPanel::BuildTransientAssetKey() const
{
    return "editor://effect/" + m_asset.graphId;
}

std::string EffectEditorPanel::GetActiveAssetKey() const
{
    // Always use the transient key: CompileDocument() registers the freshly
    // compiled asset only under the transient key. Returning the document
    // path here would cause the registry's disk-backed cache to be used,
    // which ignores live template/graph edits.
    return BuildTransientAssetKey();
}

bool EffectEditorPanel::CompileDocument()
{
    EnsureGuiAuthoringLinks(m_asset);
    EffectGraphAsset graphToCompile = m_asset;
    EnsureGuiAuthoringLinks(graphToCompile);
    SanitizeGraphAsset(graphToCompile);
    SanitizeGraphAsset(m_asset);
    if (graphToCompile.previewDefaults.previewMeshPath.empty() && !m_selectedMeshPath.empty()) {
        graphToCompile.previewDefaults.previewMeshPath = m_selectedMeshPath;
    }

    LogGraphStructure(graphToCompile, "EffectCompile:graphToCompile");

    m_compiled = EffectCompiler::Compile(graphToCompile, m_documentPath);
    EffectRuntimeRegistry::Instance().RegisterTransientAsset(BuildTransientAssetKey(), m_compiled);
    m_compileDirty = false;
    return m_compiled && m_compiled->valid;
}

void EffectEditorPanel::QueuePreviewSpawn()
{
    QueuePreviewSpawnAt(0.0f, false);
}

void EffectEditorPanel::QueuePreviewSpawnAt(float startTime, bool pausedOnSpawn)
{
    if (!m_registry || !m_compiled || !m_compiled->valid) {
        return;
    }

    if (Entity::IsNull(m_previewEntity) || !m_registry->IsAlive(m_previewEntity)) {
        m_previewEntity = m_registry->CreateEntity();
        m_registry->AddComponent(m_previewEntity, NameComponent{ "Effect Preview" });
        m_registry->AddComponent(m_previewEntity, HierarchyComponent{});
        m_registry->AddComponent(m_previewEntity, TransformComponent{});
        m_registry->AddComponent(m_previewEntity, EffectPreviewTagComponent{});
    }

    auto* transform = m_registry->GetComponent<TransformComponent>(m_previewEntity);
    if (transform) {
        transform->localPosition = m_previewAnchor;
        transform->localScale = { 1.0f, 1.0f, 1.0f };
        transform->isDirty = true;
    }

    EffectAssetComponent assetComponent;
    assetComponent.assetPath = GetActiveAssetKey();
    assetComponent.autoPlay = true;
    assetComponent.loop = true;
    assetComponent.useSelectedMeshFallback = true;

    EffectPlaybackComponent playbackComponent;
    playbackComponent.seed = m_asset.previewDefaults.seed;
    playbackComponent.loop = true;
    playbackComponent.currentTime = startTime;
    playbackComponent.isPaused = pausedOnSpawn;

    EffectSpawnRequestComponent requestComponent;
    requestComponent.pending = true;
    requestComponent.restartIfActive = true;
    requestComponent.startTime = startTime;

    m_registry->AddComponent(m_previewEntity, assetComponent);
    m_registry->AddComponent(m_previewEntity, playbackComponent);
    m_registry->AddComponent(m_previewEntity, requestComponent);
    EnsureRuntimeOverrideComponent(m_previewEntity);
}

void EffectEditorPanel::StopPreview()
{
    if (!m_registry || Entity::IsNull(m_previewEntity) || !m_registry->IsAlive(m_previewEntity)) {
        m_previewEntity = Entity::NULL_ID;
        return;
    }
    // Destroy the runtime instance BEFORE destroying the entity, otherwise the
    // runtime is leaked in EffectRuntimeRegistry::m_instances. GPU allocation
    // sticks around for up to 240 frames regardless, but CPU state is dropped.
    if (auto* playback = m_registry->GetComponent<EffectPlaybackComponent>(m_previewEntity)) {
        if (playback->runtimeInstanceId != 0) {
            EffectRuntimeRegistry::Instance().Destroy(playback->runtimeInstanceId);
            playback->runtimeInstanceId = 0;
        }
    }
    m_registry->DestroyEntity(m_previewEntity);
    m_previewEntity = Entity::NULL_ID;
}
