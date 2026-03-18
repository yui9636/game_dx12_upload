#pragma once

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <DirectXMath.h>
#include "Actor/Actor.h"

// =========================================================
// 基底コマンドインターフェース
// =========================================================
class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0; // Redo (やり直し / 初回実行)
    virtual void Undo() = 0;    // Undo (元に戻す)
    virtual const char* GetName() const = 0;
};

// =========================================================
// Undo/Redo 管理システム (シングルトン)
// =========================================================
class UndoSystem
{
public:
    static UndoSystem& Instance() { static UndoSystem s; return s; }

    // コマンドを登録して実行
    void Execute(std::shared_ptr<ICommand> cmd)
    {
        cmd->Execute();
        undoStack.push_back(cmd);
        redoStack.clear(); // 新しい操作をしたらRedoスタックは消える
        std::cout << "[UndoSystem] Executed: " << cmd->GetName() << std::endl;
    }

    // 履歴として登録するだけ（すでに実行済みの処理の場合）
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

private:
    std::vector<std::shared_ptr<ICommand>> undoStack;
    std::vector<std::shared_ptr<ICommand>> redoStack;
};

// =========================================================
// 具体的なコマンド群
// =========================================================

// ---------------------------------------------------------
// 1. Transformコマンド (移動・回転・拡縮)
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
        // 現在の状態を「変更後(New)」として保存、変更前(Old)は別途セットする前提
        for (const auto& actor : actors)
        {
            ActorTransformData data;
            data.target = actor;
            data.pos = actor->GetPosition();
            data.rot = actor->GetRotation();
            data.scale = actor->GetScale();
            newStates.push_back(data);
            oldStates.push_back(data); // 初期値として同じものを入れておく
        }
    }

    // 変更前の状態を上書き設定する（操作開始時のスナップショット）
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
// 2. 生成コマンド (複製・Spawn)
// ---------------------------------------------------------
class CmdCreate : public ICommand
{
public:
    // 生成されたアクターを受け取って保持する
    CmdCreate(const std::vector<std::shared_ptr<Actor>>& createdActors)
        : createdActors(createdActors) {}

    void Execute() override
    {
        // Redo: マネージャーに登録し直す
        for (auto& actor : createdActors)
        {
            // ※ActorManagerの実装によっては重複登録ガードが必要ですが、
            // Removeでキューに入っただけなら再度AddActorで復活できます
            // ここでは簡易的に「Start済みのものはAddActorしない」等の制御が必要かもしれません
            // 今回のActorManagerならAddActorでstartActorsに入るのでOK
            ActorManager::Instance().AddActor(actor);
        }
    }

    void Undo() override
    {
        // Undo: マネージャーから削除する
        for (auto& actor : createdActors)
        {
            ActorManager::Instance().Remove(actor);
        }
    }

    const char* GetName() const override { return "Create Actor(s)"; }

private:
    // コマンド自体がshared_ptrを持つことで、マネージャーから消えてもメモリ上で生き残る
    std::vector<std::shared_ptr<Actor>> createdActors;
};

// ---------------------------------------------------------
// 3. 削除コマンド (Delete)
// ---------------------------------------------------------
class CmdDelete : public ICommand
{
public:
    CmdDelete(const std::vector<std::shared_ptr<Actor>>& targets)
        : deletedActors(targets) {}

    void Execute() override
    {
        // 削除実行
        for (auto& actor : deletedActors)
        {
            ActorManager::Instance().Remove(actor);
        }
    }

    void Undo() override
    {
        // 復活 (AddActor)
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
        this->oldParent = child->GetParent(); // 変更前の親を覚えておく
    }

    void Execute() override
    {
        if (auto c = child.lock())
        {
            // newParentが期限切れ(nullptr)なら親解除、生きていれば親設定
            std::shared_ptr<Actor> p = newParent.lock();
            c->SetParent(p);
        }
    }

    void Undo() override
    {
        if (auto c = child.lock())
        {
            // 元の親に戻す
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



