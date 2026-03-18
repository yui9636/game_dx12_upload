#include "StageInfoComponent.h"
#include <imgui.h>

void StageInfoComponent::OnGUI()
{
 
        // 半径スライダー (1m ~ 1000m)
        ImGui::DragFloat("Radius", &radius, 0.1f, 1.0f, 1000.0f, "%.1fm");

    
}

void StageInfoComponent::Serialize(json& outJson) const
{
    // 半径をJSONに保存
    outJson["radius"] = radius;
}

void StageInfoComponent::Deserialize(const json& inJson)
{
    // JSONから半径を復元 (キーが無ければデフォルト維持)
    if (inJson.contains("radius"))
    {
        radius = inJson["radius"];
    }
}