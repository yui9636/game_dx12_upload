#include "EditorInputBridge.h"
#include "InputUserComponent.h"
#include "InputContextComponent.h"
#include "InputBindingComponent.h"
#include "ResolvedInputStateComponent.h"
#include "InputDebugStateComponent.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/ComponentSignature.h"
#include <cstring>

void EditorInputBridge::Initialize(Registry& registry) {
    // Editor User entity
    m_editorUser = registry.CreateEntity();
    InputUserComponent user{};
    user.userId = 0;
    user.deviceMask = 0xFFFFFFFF;
    user.isEditorUser = true;
    user.isPrimary = true;
    strncpy(user.profileName, "Editor", sizeof(user.profileName) - 1);
    registry.AddComponent(m_editorUser, user);

    InputBindingComponent binding{};
    strncpy(binding.actionMapAssetPath, "Data/Input/EditorGlobal.inputmap", sizeof(binding.actionMapAssetPath) - 1);
    registry.AddComponent(m_editorUser, binding);

    registry.AddComponent(m_editorUser, ResolvedInputStateComponent{});
    registry.AddComponent(m_editorUser, InputDebugStateComponent{});

    // EditorGlobal context
    m_editorGlobalCtx = registry.CreateEntity();
    InputContextComponent globalCtx{};
    globalCtx.priority = InputContextPriority::EditorGlobal;
    globalCtx.enabled = true;
    globalCtx.consumeLowerPriority = false;
    globalCtx.pointerEnabled = true;
    registry.AddComponent(m_editorGlobalCtx, globalCtx);

    // SceneView context
    m_sceneViewCtx = registry.CreateEntity();
    InputContextComponent sceneCtx{};
    sceneCtx.priority = InputContextPriority::SceneView;
    sceneCtx.enabled = true;
    sceneCtx.consumeLowerPriority = false;
    sceneCtx.pointerEnabled = true;
    registry.AddComponent(m_sceneViewCtx, sceneCtx);

    // TextInput context (starts disabled)
    m_textInputCtx = registry.CreateEntity();
    InputContextComponent textCtx{};
    textCtx.priority = InputContextPriority::TextInput;
    textCtx.enabled = false;
    textCtx.consumeLowerPriority = true;
    textCtx.textInputEnabled = true;
    registry.AddComponent(m_textInputCtx, textCtx);
}

void EditorInputBridge::Finalize(Registry& /*registry*/) {
    m_editorUser = 0;
    m_editorGlobalCtx = 0;
    m_sceneViewCtx = 0;
    m_textInputCtx = 0;
}

void EditorInputBridge::UpdateHoverState(bool sceneViewHovered) {
    m_sceneViewHovered = sceneViewHovered;
}

void EditorInputBridge::UpdateTextInputState(bool wantTextInput, Registry& registry) {
    if (wantTextInput == m_textInputActive) return;
    m_textInputActive = wantTextInput;

    Signature sig = CreateSignature<InputContextComponent>();
    auto archetypes = registry.GetAllArchetypes();
    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
        if (!col) continue;
        auto entities = arch->GetEntities();
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            if (entities[i] == m_textInputCtx) {
                auto& ctx = *static_cast<InputContextComponent*>(col->Get(i));
                ctx.enabled = wantTextInput;
            }
        }
    }
}

void EditorInputBridge::OnPlayStarted(Registry& registry) {
    // Disable SceneView gamepad input during play
    Signature sig = CreateSignature<InputContextComponent>();
    auto archetypes = registry.GetAllArchetypes();
    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
        if (!col) continue;
        auto entities = arch->GetEntities();
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            if (entities[i] == m_sceneViewCtx) {
                static_cast<InputContextComponent*>(col->Get(i))->enabled = false;
            }
        }
    }
}

void EditorInputBridge::OnPlayStopped(Registry& registry) {
    // Re-enable SceneView
    Signature sig = CreateSignature<InputContextComponent>();
    auto archetypes = registry.GetAllArchetypes();
    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
        if (!col) continue;
        auto entities = arch->GetEntities();
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            if (entities[i] == m_sceneViewCtx) {
                static_cast<InputContextComponent*>(col->Get(i))->enabled = true;
            }
        }
    }
}

const ResolvedInputStateComponent* EditorInputBridge::GetEditorInput(Registry& registry) const {
    if (m_editorUser == 0) return nullptr;
    Signature sig = CreateSignature<ResolvedInputStateComponent>();
    auto archetypes = registry.GetAllArchetypes();
    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        if (!col) continue;
        auto entities = arch->GetEntities();
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            if (entities[i] == m_editorUser) {
                return static_cast<const ResolvedInputStateComponent*>(col->Get(i));
            }
        }
    }
    return nullptr;
}
