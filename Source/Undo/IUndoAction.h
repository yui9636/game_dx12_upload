#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Registry/Registry.h"

class IUndoAction {
public:
    virtual ~IUndoAction() = default;
    virtual void Undo(Registry& registry) = 0;
    virtual void Redo(Registry& registry) = 0;
    virtual const char* GetName() const = 0;
};

class CompositeUndoAction : public IUndoAction {
public:
    explicit CompositeUndoAction(std::string name)
        : m_name(std::move(name)) {
    }

    void Add(std::unique_ptr<IUndoAction> action) {
        if (action) {
            m_actions.push_back(std::move(action));
        }
    }

    bool Empty() const {
        return m_actions.empty();
    }

    void Undo(Registry& registry) override {
        for (auto it = m_actions.rbegin(); it != m_actions.rend(); ++it) {
            (*it)->Undo(registry);
        }
    }

    void Redo(Registry& registry) override {
        for (auto& action : m_actions) {
            action->Redo(registry);
        }
    }

    const char* GetName() const override { return m_name.c_str(); }

private:
    std::string m_name;
    std::vector<std::unique_ptr<IUndoAction>> m_actions;
};
