#pragma once

#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <algorithm>
#include "Effect/EffectCurve.h"



struct RenderContext;


struct EffectTransform
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
};

enum class EffectNodeType
{
    Empty,
    MeshEmitter,
    Particle
};

class EffectNode
{
public:
    EffectNode() = default;
    virtual ~EffectNode() = default;

    // --------------------------------------------------------
    // --------------------------------------------------------
    std::string name = "Node";
    EffectNode* parent = nullptr;

    EffectNodeType type = EffectNodeType::Empty;

    std::vector<std::shared_ptr<EffectNode>> children;

    void AddChild(std::shared_ptr<EffectNode> child)
    {
        child->parent = this;
        children.push_back(child);
    }

    // --------------------------------------------------------
    // --------------------------------------------------------

    EffectTransform localTransform;

    EffectCurve positionCurves[3];
    EffectCurve rotationCurves[3];
    EffectCurve scaleCurves[3];

    DirectX::XMFLOAT4X4 worldMatrix = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };

    // --------------------------------------------------------
    // --------------------------------------------------------

    void UpdateTransform(const DirectX::XMMATRIX& parentMatrix)
    {
        using namespace DirectX;

        XMMATRIX S = XMMatrixScaling(localTransform.scale.x, localTransform.scale.y, localTransform.scale.z);

        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(localTransform.rotation.x),
            XMConvertToRadians(localTransform.rotation.y),
            XMConvertToRadians(localTransform.rotation.z)
        );

        XMMATRIX T = XMMatrixTranslation(localTransform.position.x, localTransform.position.y, localTransform.position.z);

        XMMATRIX localM = S * R * T;

        XMMATRIX worldM = localM * parentMatrix;

        XMStoreFloat4x4(&worldMatrix, worldM);

        for (auto& child : children)
        {
            child->UpdateTransform(worldM);
        }
    }

    // --------------------------------------------------------
    // --------------------------------------------------------

    virtual EffectNodeType GetType() const { return EffectNodeType::Empty; }

 /*   virtual void Update(float deltaTime) {
   
    }


    virtual void Render(RenderContext& rc) {}
*/
    virtual void Update(float deltaTime) {
        for (auto& child : children) {
            child->Update(deltaTime);
        }
    }

    virtual void Render(RenderContext& rc) {
        for (auto& child : children) {
            child->Render(rc);
        }
    }

    virtual void Reset() {

        for (auto& child : children) {
            child->Reset();
        }
    }

    virtual void UpdateWithAge(float age, float lifeTime) {
        float nodeLocalTime = age - startTime;
        float t = 0.0f;
        if (duration > 0.001f) {
            t = nodeLocalTime / duration;
        }

        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;


        // Position
        if (positionCurves[0].IsValid()) localTransform.position.x = positionCurves[0].Evaluate(t);
        if (positionCurves[1].IsValid()) localTransform.position.y = positionCurves[1].Evaluate(t);
        if (positionCurves[2].IsValid()) localTransform.position.z = positionCurves[2].Evaluate(t);

        // Rotation
        if (rotationCurves[0].IsValid()) localTransform.rotation.x = rotationCurves[0].Evaluate(t);
        if (rotationCurves[1].IsValid()) localTransform.rotation.y = rotationCurves[1].Evaluate(t);
        if (rotationCurves[2].IsValid()) localTransform.rotation.z = rotationCurves[2].Evaluate(t);

        // Scale
        if (scaleCurves[0].IsValid()) localTransform.scale.x = scaleCurves[0].Evaluate(t);
        if (scaleCurves[1].IsValid()) localTransform.scale.y = scaleCurves[1].Evaluate(t);
        if (scaleCurves[2].IsValid()) localTransform.scale.z = scaleCurves[2].Evaluate(t);

        for (auto& child : children) {
            child->UpdateWithAge(age, lifeTime);
        }
    }

public:
    float startTime = 0.0f;
    float duration = 5.0f;
    bool isVisible = true;

};
