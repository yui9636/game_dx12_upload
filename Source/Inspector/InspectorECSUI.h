#pragma once

class Registry;

class InspectorECSUI {
public:
	static void Render(Registry* registry, bool* p_open = nullptr, bool* outFocused = nullptr);
};
