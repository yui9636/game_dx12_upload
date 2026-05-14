#pragma once
// Registry はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Registry;

class InspectorECSUI {
public:
	static void Render(Registry* registry, bool* p_open = nullptr, bool* outFocused = nullptr);
};
