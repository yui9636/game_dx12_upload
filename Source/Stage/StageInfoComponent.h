#pragma once
#include "Component/Component.h"

class StageInfoComponent : public Component
{
public:
    StageInfoComponent() = default;
    ~StageInfoComponent() override = default;

    float radius = 50.0f;


    const char* GetName() const override { return "StageInfo"; }

    void OnGUI() override;

    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;
};
