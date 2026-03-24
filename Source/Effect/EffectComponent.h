#pragma once
#include "Component/Component.h"
#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>

class EffectInstance;

class EffectComponent : public Component
{
public:
    const char* GetName() const override { return "EffectComponent"; }


    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;

    //void Start() override;


    void Update(float dt) override;

    // ---------------------------------------------------------
    // ---------------------------------------------------------
    std::weak_ptr<EffectInstance> Play(
        const std::string& effectName,
        const std::string& boneName = "",
        const DirectX::XMFLOAT3& offset = { 0,0,0 },
        const DirectX::XMFLOAT3& rotation = { 0,0,0 },
        const DirectX::XMFLOAT3& scale = { 1,1,1 },
        bool loop = false
    );

    void SetEffectTransform(
        const std::weak_ptr<EffectInstance>& handle,
        const DirectX::XMFLOAT3& offset,
        const DirectX::XMFLOAT3& rotation,
        const DirectX::XMFLOAT3& scale
    );

    void Stop(const std::weak_ptr<EffectInstance>& handle);
    void StopAll();

    void OnGUI() override;
private:
    struct ActiveEffect
    {
        std::weak_ptr<EffectInstance> instance;
        std::string targetBoneName;
        int targetBoneIndex = -1;

        DirectX::XMFLOAT3 localOffsetPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 localOffsetRotation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 localOffsetScale = { 1,1,1 };

        std::string effectPath;
        bool loop = true;
    };

    std::vector<ActiveEffect> activeEffects;
    std::vector<std::weak_ptr<EffectInstance>> managedEffects;
};
