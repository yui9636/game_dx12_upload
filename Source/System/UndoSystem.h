#pragma once

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include "Undo/IUndoAction.h"

// =========================================================
// =========================================================
class UndoSystem
{
public:
    static UndoSystem& Instance() { static UndoSystem s; return s; }

    void ExecuteAction(std::unique_ptr<IUndoAction> action, Registry& registry)
    {
        if (!action) return;
        action->Redo(registry);
        std::cout << "[UndoSystem] Executed ECS: " << action->GetName() << std::endl;
        ecsUndoStack.push_back(std::move(action));
        ecsRedoStack.clear();
        ++m_ecsRevision;
    }

    void RecordAction(std::unique_ptr<IUndoAction> action)
    {
        if (!action) return;
        std::cout << "[UndoSystem] Recorded ECS: " << action->GetName() << std::endl;
        ecsUndoStack.push_back(std::move(action));
        ecsRedoStack.clear();
        ++m_ecsRevision;
    }

    void Undo(Registry& registry)
    {
        if (ecsUndoStack.empty()) return;
        auto action = std::move(ecsUndoStack.back());
        ecsUndoStack.pop_back();
        action->Undo(registry);
        std::cout << "[UndoSystem] Undo ECS: " << action->GetName() << std::endl;
        ecsRedoStack.push_back(std::move(action));
        ++m_ecsRevision;
    }

    void Redo(Registry& registry)
    {
        if (ecsRedoStack.empty()) return;
        auto action = std::move(ecsRedoStack.back());
        ecsRedoStack.pop_back();
        action->Redo(registry);
        std::cout << "[UndoSystem] Redo ECS: " << action->GetName() << std::endl;
        ecsUndoStack.push_back(std::move(action));
        ++m_ecsRevision;
    }

    void ClearECSHistory()
    {
        ecsUndoStack.clear();
        ecsRedoStack.clear();
        ++m_ecsRevision;
    }

    uint64_t GetECSRevision() const
    {
        return m_ecsRevision;
    }

    bool CanUndoECS() const
    {
        return !ecsUndoStack.empty();
    }

    bool CanRedoECS() const
    {
        return !ecsRedoStack.empty();
    }

private:
    std::vector<std::unique_ptr<IUndoAction>> ecsUndoStack;
    std::vector<std::unique_ptr<IUndoAction>> ecsRedoStack;
    uint64_t m_ecsRevision = 0;
};

