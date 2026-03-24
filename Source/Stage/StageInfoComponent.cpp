#include "StageInfoComponent.h"
#include <imgui.h>

void StageInfoComponent::OnGUI()
{
 
        ImGui::DragFloat("Radius", &radius, 0.1f, 1.0f, 1000.0f, "%.1fm");

    
}

void StageInfoComponent::Serialize(json& outJson) const
{
    outJson["radius"] = radius;
}

void StageInfoComponent::Deserialize(const json& inJson)
{
    if (inJson.contains("radius"))
    {
        radius = inJson["radius"];
    }
}
