#pragma once
// Registry はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Registry;

class NodeAttachmentSystem
{
public:
    static void Update(Registry& registry);
};
