// NodeSocketComponent の ECS コンポーネント定義をまとめます。
#pragma once

#include <vector>

#include "Component/NodeSocket.h"

struct NodeSocketComponent
{
    std::vector<NodeSocket> sockets;
};
