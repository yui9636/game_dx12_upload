#pragma once

#include <optional>
#include <type_traits>

#include "Undo/IUndoAction.h"
#include "Component/TransformComponent.h"

template<typename T>
class ComponentUndoAction : public IUndoAction {
    EntityID m_target;
    T m_oldState;
    T m_newState;

public:
    ComponentUndoAction(EntityID entity, const T& oldState, const T& newState)
        : m_target(entity), m_oldState(oldState), m_newState(newState) {
    }

    void Undo(Registry& registry) override {
        auto* comp = registry.GetComponent<T>(m_target);
        if (comp) {
            *comp = m_oldState;
            SetDirtyIfPossible(comp);
        }
    }

    void Redo(Registry& registry) override {
        auto* comp = registry.GetComponent<T>(m_target);
        if (comp) {
            *comp = m_newState;
            SetDirtyIfPossible(comp);
        }
    }

    const char* GetName() const override { return "Component Change"; }

private:
    void SetDirtyIfPossible(T* comp) {
        if constexpr (std::is_same_v<T, TransformComponent>) {
            comp->isDirty = true;
        }
    }
};


template<typename T>
class OptionalComponentUndoAction : public IUndoAction {
    EntityID m_target;
    std::optional<T> m_oldState;
    std::optional<T> m_newState;
    const char* m_name;

public:
    OptionalComponentUndoAction(EntityID entity,
                                std::optional<T> oldState,
                                std::optional<T> newState,
                                const char* name = "Component Change")
        : m_target(entity), m_oldState(std::move(oldState)), m_newState(std::move(newState)), m_name(name) {
    }

    void Undo(Registry& registry) override { Apply(registry, m_oldState); }
    void Redo(Registry& registry) override { Apply(registry, m_newState); }
    const char* GetName() const override { return m_name; }

private:
    void Apply(Registry& registry, const std::optional<T>& state) {
        if (state.has_value()) {
            registry.AddComponent<T>(m_target, *state);
            if (auto* comp = registry.GetComponent<T>(m_target)) {
                SetDirtyIfPossible(comp);
            }
        } else {
            registry.RemoveComponent<T>(m_target);
        }
    }

    void SetDirtyIfPossible(T* comp) {
        if constexpr (std::is_same_v<T, TransformComponent>) {
            comp->isDirty = true;
        }
    }
};
