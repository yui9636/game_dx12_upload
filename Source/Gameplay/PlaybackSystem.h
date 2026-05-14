// PlaybackSystem のシステム宣言をまとめます。
#pragma once
class Registry;
class PlaybackSystem {
public:
    static void Update(Registry& registry, float dt);
};
