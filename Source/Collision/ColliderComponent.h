#pragma once
#include "Component/Component.h"
#include <vector>
#include <string>
#include <cstdint>
#include <SimpleMath.h>
#include "Collision.h" 

class NodeAttachComponent;
class Gizmos;
class Actor;

class ColliderComponent final : public Component
{
public:
    const char* GetName() const override { return "Collider"; }

    struct Settings
    {
        bool enabled = true;
        bool drawGizmo = true;
    };

    enum class ShapeType : uint32_t { Sphere = 0, Capsule = 1, Box = 2 };

    struct Element
    {
        ShapeType type = ShapeType::Sphere;
        bool enabled = true;

        int nodeIndex = -1;
        DirectX::SimpleMath::Vector3 offsetLocal{ 0,0,0 };

        float radius = 0.5f;
        float height = 1.0f;

        DirectX::SimpleMath::Vector3 size{ 1.0f, 1.0f, 1.0f };
        DirectX::SimpleMath::Vector4 color{ 1,0,0,0.35f };

        int runtimeTag = 0;
        std::string label = "Collider";

        uint32_t registeredId = 0;

        ColliderAttribute attribute = ColliderAttribute::Body;
    };

public:
    ~ColliderComponent() override;

    void Start() override;
    void Update(float) override;

    void OnGUI() override;

    void Render() override;

    std::shared_ptr<Component> Clone() override;

    Settings& GetSettings() { return settings; }
    const Settings& GetSettings() const { return settings; }

    std::vector<Element>& GetElements() { return elements; }
    const std::vector<Element>& GetElements() const { return elements; }

    void AddSphere(const DirectX::SimpleMath::Vector3& offset, float radius, ColliderAttribute attr = ColliderAttribute::Body);
    void AddCapsule(const DirectX::SimpleMath::Vector3& offset, float radius, float height, ColliderAttribute attr = ColliderAttribute::Body);
    void AddBox(const DirectX::SimpleMath::Vector3& offset, const DirectX::SimpleMath::Vector3& size, ColliderAttribute attr = ColliderAttribute::Body);

    float GetMaxRadiusXZ() const;

    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;
private:
    DirectX::XMFLOAT3 ComputeWorldCenter(const Element& e);
    static inline float ClampMin(float v, float m) { return (v < m) ? m : v; }

    void RegisterToManager(Element& e);
    void UpdateToManager(Element& e);
    void UnregisterFromManager(Element& e);

private:
    Settings settings{};
    std::vector<Element> elements;
};
