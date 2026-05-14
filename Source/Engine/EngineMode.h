#pragma once
// EngineMode はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

enum class EngineMode
{
    Editor,
    Play,
    Pause
};
