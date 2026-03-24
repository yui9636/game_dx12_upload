#pragma once

#include <memory>
#include <vector>
#include <string>
#include <d3d11.h>
#include <DirectXMath.h>
#include <algorithm>

class Component;
class Model;
struct RenderContext;
class ModelRenderer;
struct Collider; 
struct HitResult; 


class Actor : public std::enable_shared_from_this<Actor>
{
public:
    Actor() = default;
    virtual ~Actor() = default;

    virtual void Initialize(ID3D11Device* device) {}

    virtual void Start();
    virtual void Update(float dt);
    virtual void Render(ModelRenderer* renderer);
    virtual void OnGUI();

    virtual void OnTriggerEnter(Actor* other, const Collider* selfCol, const Collider* otherCol) {}

    virtual void OnCollisionEnter(Actor* other, const Collider* selfCol, const Collider* otherCol) {}
    virtual bool IsCharacter() const { return false; }

    void UpdateTransform();

    void SetParent(std::shared_ptr<Actor> newParent, bool keepWorldTransform = true);
    std::shared_ptr<Actor> GetParent() const;
    const std::vector<std::weak_ptr<Actor>>& GetChildren() const { return children; }

    template <class T>
    std::shared_ptr<T> AddComponent();

    template <class T>
    std::shared_ptr<T> GetComponent() const;

    template <class T>
    void RemoveComponent();

    const DirectX::XMFLOAT3& GetPosition() const { return position; }
    void SetPosition(const DirectX::XMFLOAT3& pos);

    const DirectX::XMFLOAT4& GetRotation() const { return rotation; }
    void SetRotation(const DirectX::XMFLOAT4& rot);

    const DirectX::XMFLOAT3& GetScale() const { return scale; }
    void SetScale(const DirectX::XMFLOAT3& s);

    const DirectX::XMFLOAT3& GetLocalPosition() const { return localPosition; }
    void SetLocalPosition(const DirectX::XMFLOAT3& pos);

    const DirectX::XMFLOAT4& GetLocalRotation() const { return localRotation; }
    void SetLocalRotation(const DirectX::XMFLOAT4& rot);

    const DirectX::XMFLOAT3& GetLocalScale() const { return localScale; }
    void SetLocalScale(const DirectX::XMFLOAT3& scale);

    const DirectX::XMFLOAT4X4& GetTransform() const { return transform; }

    void SetName(const std::string& name) { this->name = name; }
    const std::string& GetName() const { return name; }

    void LoadModel(const char* filename, float scaling = 1.0f);
    Model* GetModelRaw() const;

    std::shared_ptr<Actor> Clone(std::shared_ptr<Actor> parentToAttach = nullptr);

    const std::string& GetModelPath() const { return modelFilePath; }

    const std::vector<std::shared_ptr<Component>>& GetComponents() const { return components; }

    void RemoveComponent(std::shared_ptr<Component> component);
   
    void SetOverrideTypeName(const std::string& name) { overrideTypeName = name; }

    virtual void SetModel(std::shared_ptr<Model> model, const std::string& path, float scaling)
    {
        this->model = model;
        this->modelFilePath = path;

        DirectX::XMFLOAT3 s = { scaling, scaling, scaling };
        this->scale = s;
        this->localScale = s;
    }

    virtual std::string GetTypeName() const
    {
        if (!overrideTypeName.empty())
        {
            return overrideTypeName;
        }
        return "Actor";
    }


    bool isDebugModel = false;

protected:
    std::shared_ptr<Model> model;

    std::string name = "Actor";

    std::string overrideTypeName = "";

    std::string modelFilePath;

    DirectX::XMFLOAT3 position = { 0, 0, 0 };
    DirectX::XMFLOAT4 rotation = { 0, 0, 0, 1 }; // Quaternion
    DirectX::XMFLOAT3 scale = { 1, 1, 1 };

    DirectX::XMFLOAT4X4 transform = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    DirectX::XMFLOAT3 localPosition = { 0, 0, 0 };
    DirectX::XMFLOAT4 localRotation = { 0, 0, 0, 1 };
    DirectX::XMFLOAT3 localScale = { 1, 1, 1 };

    std::weak_ptr<Actor> parent;
    std::vector<std::weak_ptr<Actor>> children;

    void AddChild(std::shared_ptr<Actor> child);
    void RemoveChild(std::shared_ptr<Actor> child);

    std::vector<std::shared_ptr<Component>> components;

   
};


template <class T>
std::shared_ptr<T> Actor::AddComponent()
{
    static_assert(std::is_base_of<Component, T>::value, "T must be derived from Component");
    auto component = std::make_shared<T>();
    component->SetActor(shared_from_this());
    components.push_back(component);
    return component;
}

template <class T>
std::shared_ptr<T> Actor::GetComponent() const
{
    for (const auto& component : components)
    {
        if (auto casted = std::dynamic_pointer_cast<T>(component))
        {
            return casted;
        }
    }
    return nullptr;
}

template <class T>
void Actor::RemoveComponent()
{
    for (auto it = components.begin(); it != components.end(); ++it)
    {
        if (std::dynamic_pointer_cast<T>(*it))
        {
            components.erase(it);
            return;
        }
    }
}

// ============================================================================
// ActorManager
// ============================================================================
class ActorManager
{
private:
    ActorManager() = default;
    ~ActorManager() = default;

public:
    static ActorManager& Instance()
    {
        static ActorManager instance;
        return instance;
    }

    std::shared_ptr<Actor> Create();

    void AddActor(std::shared_ptr<Actor> actor);

    void Remove(std::shared_ptr<Actor> actor);

    void Clear();

    void Update(float dt);
    void UpdateTransform();

    void Render(const RenderContext& rc, ModelRenderer* renderer);

    const std::vector<std::shared_ptr<Actor>>& GetActors() const { return updateActors; }

    const std::vector<std::shared_ptr<Actor>>& GetStartActors() const { return startActors; }

    void SetSelectedActor(std::shared_ptr<Actor> actor) { selectedActor = actor; }

    std::shared_ptr<Actor> GetSelectedActor() const { return selectedActor.lock(); }

private:
    std::vector<std::shared_ptr<Actor>> startActors;
    std::vector<std::shared_ptr<Actor>> updateActors;
    std::vector<std::shared_ptr<Actor>> removeQueue;

    std::weak_ptr<Actor> selectedActor;
};
