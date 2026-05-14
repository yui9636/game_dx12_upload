#pragma once

#include "PlayerEditor/StateMachineAsset.h"
// StateMachineAssetComponent は asset を保持し、関連システムが実行時状態として参照する。

struct StateMachineAssetComponent
{
    StateMachineAsset asset;
};
