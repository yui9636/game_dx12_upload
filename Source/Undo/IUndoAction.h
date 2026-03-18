#pragma once
#include "Registry/Registry.h"

class IUndoAction {
public:
    virtual ~IUndoAction() = default;
    virtual void Undo(Registry& registry) = 0;
    virtual void Redo(Registry& registry) = 0;
};