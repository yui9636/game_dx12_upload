#include "InputTextSystem.h"
#include "InputTextFieldComponent.h"
#include "IInputBackend.h"
#include "InputEventQueue.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <cstring>

void InputTextSystem::Update(Registry& registry, const InputEventQueue& queue, IInputBackend& backend) {
    Signature sig = CreateSignature<InputTextFieldComponent>();
    auto archetypes = registry.GetAllArchetypes();

    bool anyFocused = false;

    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputTextFieldComponent>());
        if (!col) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& field = *static_cast<InputTextFieldComponent*>(col->Get(i));
            if (!field.isFocused) continue;

            anyFocused = true;

            // Process text/composition events
            for (auto& ev : queue.GetEvents()) {
                if (ev.type == InputEventType::TextComposition && field.compositionEnabled) {
                    strncpy(field.compositionText, ev.textComposition.text,
                            sizeof(field.compositionText) - 1);
                    field.compositionText[sizeof(field.compositionText) - 1] = '\0';
                    field.compositionCursor = ev.textComposition.cursor;
                    field.compositionSelectionLen = ev.textComposition.selectionLen;
                }
            }
        }
    }

    // Manage SDL text input state
    if (anyFocused && !backend.IsTextInputActive()) {
        backend.StartTextInput();
    } else if (!anyFocused && backend.IsTextInputActive()) {
        backend.StopTextInput();
    }
}
