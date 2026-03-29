#pragma once

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <DirectXMath.h>
#include "Actor/Actor.h"
#include "Undo/IUndoAction.h"

// =========================================================
// =========================================================
class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual const char* GetName() const = 0;
};

// =========================================================
// =========================================================
class UndoSystem
{
public:
    static UndoSystem& Instance() { static UndoSystem s; return s; }

    void Execute(std::shared_ptr<ICommand> cmd)
    {
        cmd->Execute();
        undoStack.push_back(cmd);
        redoStack.clear();
        std::cout << "[UndoSystem] Executed: " << cmd->GetName() << std::endl;
    }

    void Record(std::shared_ptr<ICommand> cmd)
    {
        undoStack.push_back(cmd);
        redoStack.clear();
        std::cout << "[UndoSystem] Recorded: " << cmd->GetName() << std::endl;
    }

    void Undo()
    {
        if (undoStack.empty()) return;
        auto cmd = undoStack.back();
        cmd->Undo();
        undoStack.pop_back();
        redoStack.push_back(cmd);
        std::cout << "[UndoSystem] Undo: " << cmd->GetName() << std::endl;
    }

    void Redo()
    {
        if (redoStack.empty()) return;
        auto cmd = redoStack.back();
        cmd->Execute();
        redoStack.pop_back();
        undoStack.push_back(cmd);
        std::cout << "[UndoSystem] Redo: " << cmd->GetName() << std::endl;
    }

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
    std::vector<std::shared_ptr<ICommand>> undoStack;
    std::vector<std::shared_ptr<ICommand>> redoStack;
    std::vector<std::unique_ptr<IUndoAction>> ecsUndoStack;
    std::vector<std::unique_ptr<IUndoAction>> ecsRedoStack;
    uint64_t m_ecsRevision = 0;
};

// =========================================================
// =========================================================

// ---------------------------------------------------------
// ---------------------------------------------------------
struct ActorTransformData
{
    std::weak_ptr<Actor> target;
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 rot;
    DirectX::XMFLOAT3 scale;
};

class CmdTransform : public ICommand
{
public:
    CmdTransform(const std::vector<std::shared_ptr<Actor>>& actors)
    {
        for (const auto& actor : actors)
        {
            ActorTransformData data;
            data.target = actor;
            data.pos = actor->GetPosition();
            data.rot = actor->GetRotation();
            data.scale = actor->GetScale();
            newStates.push_back(data);
            oldStates.push_back(data);
        }
    }

    void SetOldState(int index, const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT4& r, const DirectX::XMFLOAT3& s)
    {
        if (index >= 0 && index < (int)oldStates.size()) {
            oldStates[index].pos = p;
            oldStates[index].rot = r;
            oldStates[index].scale = s;
        }
    }

    void Execute() override { Apply(newStates); }
    void Undo() override { Apply(oldStates); }
    const char* GetName() const override { return "Transform Actor(s)"; }

private:
    void Apply(const std::vector<ActorTransformData>& states)
    {
        for (const auto& data : states)
        {
            if (auto actor = data.target.lock())
            {
                actor->SetPosition(data.pos);
                actor->SetRotation(data.rot);
                actor->SetScale(data.scale);
                actor->UpdateTransform();
            }
        }
    }

    std::vector<ActorTransformData> oldStates;
    std::vector<ActorTransformData> newStates;
};

// ---------------------------------------------------------
// ---------------------------------------------------------
class CmdCreate : public ICommand
{
public:
    CmdCreate(const std::vector<std::shared_ptr<Actor>>& createdActors)
        : createdActors(createdActors) {}

    void Execute() override
    {
        for (auto& actor : createdActors)
        {
            ActorManager::Instance().AddActor(actor);
        }
    }

    void Undo() override
    {
        for (auto& actor : createdActors)
        {
            ActorManager::Instance().Remove(actor);
        }
    }

    const char* GetName() const override { return "Create Actor(s)"; }

private:
    std::vector<std::shared_ptr<Actor>> createdActors;
};

// ---------------------------------------------------------
// ---------------------------------------------------------
class CmdDelete : public ICommand
{
public:
    CmdDelete(const std::vector<std::shared_ptr<Actor>>& targets)
        : deletedActors(targets) {}

    void Execute() override
    {
        for (auto& actor : deletedActors)
        {
            ActorManager::Instance().Remove(actor);
        }
    }

    void Undo() override
    {
        for (auto& actor : deletedActors)
        {
            ActorManager::Instance().AddActor(actor);
        }
    }

    const char* GetName() const override { return "Delete Actor(s)"; }

private:
    std::vector<std::shared_ptr<Actor>> deletedActors;
};

class CmdParent : public ICommand
{
public:
    CmdParent(std::shared_ptr<Actor> child, std::shared_ptr<Actor> newParent)
    {
        this->child = child;
        this->newParent = newParent;
        this->oldParent = child->GetParent();
    }

    void Execute() override
    {
        if (auto c = child.lock())
        {
            std::shared_ptr<Actor> p = newParent.lock();
            c->SetParent(p);
        }
    }

    void Undo() override
    {
        if (auto c = child.lock())
        {
            std::shared_ptr<Actor> p = oldParent.lock();
            c->SetParent(p);
        }
    }

    const char* GetName() const override { return "Change Parent"; }

private:
    std::weak_ptr<Actor> child;
    std::weak_ptr<Actor> newParent;
    std::weak_ptr<Actor> oldParent;
};



