#pragma once
#include "Registry/Registry.h"
#include "ResolvedInputStateComponent.h"

class IInputBackend;

class EditorInputBridge {
public:
    void Initialize(Registry& registry);
    void Finalize(Registry& registry);

    void UpdateHoverState(bool sceneViewHovered);
    void UpdateTextInputState(bool wantTextInput, Registry& registry);
    void OnPlayStarted(Registry& registry);
    void OnPlayStopped(Registry& registry);

    EntityID GetEditorUserEntity() const { return m_editorUser; }
    const ResolvedInputStateComponent* GetEditorInput(Registry& registry) const;

private:
    EntityID m_editorUser = 0;
    EntityID m_editorGlobalCtx = 0;
    EntityID m_sceneViewCtx = 0;
    EntityID m_textInputCtx = 0;
    bool m_sceneViewHovered = false;
    bool m_textInputActive = false;
};
