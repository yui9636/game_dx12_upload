#pragma once
// Registry はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Registry;

class RootMotionSystem
{
public:
    static void Update(Registry& registry, float dt);
};
