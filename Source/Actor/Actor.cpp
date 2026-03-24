#include "Actor.h"
#include "Component/Component.h"
#include "Model/Model.h"
#include <Model\ModelRenderer.h>
#include "Actor.h"
#include "Component/Component.h"
#include "Model/Model.h"
#include <Model/ModelRenderer.h>
#include"System\/ResourceManager.h"

// ============================================================================
// Actor Implementation
// ============================================================================

void Actor::Start()
{
    for (auto& c : components) c->Start();
}

void Actor::Update(float dt)
{
    for (auto& c : components) c->Update(dt);
}

void Actor::Render(ModelRenderer* renderer)
{
    if (model)
    {
        //renderer->Draw(ShaderId::PBR, model, transform);
    }
    for (auto& c : components) c->Render();
}

void Actor::OnGUI()
{
    for (auto& c : components) c->OnGUI();
}

void Actor::UpdateTransform()
{
    DirectX::XMMATRIX S = DirectX::XMMatrixScaling(localScale.x, localScale.y, localScale.z);
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&localRotation));
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(localPosition.x, localPosition.y, localPosition.z);

    DirectX::XMMATRIX localMatrix = S * R * T;
    DirectX::XMMATRIX worldMatrix = localMatrix;

    std::shared_ptr<Actor> parentPtr = parent.lock();
    if (parentPtr)
    {
        DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&parentPtr->GetTransform());
        worldMatrix = localMatrix * parentWorld;
    }

    DirectX::XMStoreFloat4x4(&transform, worldMatrix);

    DirectX::XMVECTOR outScale, outRot, outTrans;
    DirectX::XMMatrixDecompose(&outScale, &outRot, &outTrans, worldMatrix);

    DirectX::XMStoreFloat3(&position, outTrans);
    DirectX::XMStoreFloat4(&rotation, outRot);
    DirectX::XMStoreFloat3(&scale, outScale);

    //if (model) model->UpdateTransform(transform);

    if (model)
    {
        DirectX::XMFLOAT4X4 identity;
        DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
        model->UpdateTransform(identity);
    }

    for (auto& childWeak : children)
    {
        if (auto child = childWeak.lock())
        {
            child->UpdateTransform();
        }
    }
}


void Actor::SetParent(std::shared_ptr<Actor> newParent, bool keepWorldTransform)
{
    std::shared_ptr<Actor> oldParent = parent.lock();
    if (oldParent == newParent) return;

    if (oldParent)
    {
        oldParent->RemoveChild(shared_from_this());
    }

    parent = newParent;

    if (newParent)
    {
        newParent->AddChild(shared_from_this());

        if (keepWorldTransform)
        {
            DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(&transform);
            DirectX::XMMATRIX parentMat = DirectX::XMLoadFloat4x4(&newParent->GetTransform());
            DirectX::XMMATRIX invParent = DirectX::XMMatrixInverse(nullptr, parentMat);
            DirectX::XMMATRIX newLocal = worldMat * invParent;

            DirectX::XMVECTOR s, r, t;
            DirectX::XMMatrixDecompose(&s, &r, &t, newLocal);

            DirectX::XMStoreFloat3(&localPosition, t);
            DirectX::XMStoreFloat4(&localRotation, r);
            DirectX::XMStoreFloat3(&localScale, s);
        }
        else
        {
            localPosition = position;
            localRotation = rotation;
            localScale = scale;
        }
    }
    else
    {
        localPosition = position;
        localRotation = rotation;
        localScale = scale;
    }
}

std::shared_ptr<Actor> Actor::GetParent() const
{
    return parent.lock();
}

void Actor::AddChild(std::shared_ptr<Actor> child)
{
    if (child) children.push_back(child);
}

void Actor::RemoveChild(std::shared_ptr<Actor> child)
{
    auto it = std::remove_if(children.begin(), children.end(),
        [&](const std::weak_ptr<Actor>& ptr) {
            return ptr.lock() == child;
        });
    children.erase(it, children.end());
}


void Actor::SetPosition(const DirectX::XMFLOAT3& pos)
{
    position = pos;

    if (auto p = parent.lock())
    {
        DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&p->GetTransform());
        DirectX::XMMATRIX invParent = DirectX::XMMatrixInverse(nullptr, parentWorld);

        DirectX::XMVECTOR targetWorldPos = DirectX::XMLoadFloat3(&pos);
        DirectX::XMVECTOR targetLocalPos = DirectX::XMVector3TransformCoord(targetWorldPos, invParent);

        DirectX::XMStoreFloat3(&localPosition, targetLocalPos);
    }
    else
    {
        localPosition = pos;
    }
}

void Actor::SetRotation(const DirectX::XMFLOAT4& rot)
{
    rotation = rot;

    if (auto p = parent.lock())
    {
        DirectX::XMVECTOR worldQ = DirectX::XMLoadFloat4(&rot);
        DirectX::XMVECTOR parentQ = DirectX::XMLoadFloat4(&p->GetRotation());
        DirectX::XMVECTOR invParentQ = DirectX::XMQuaternionInverse(parentQ);
        DirectX::XMVECTOR localQ = DirectX::XMQuaternionMultiply(worldQ, invParentQ);

        DirectX::XMStoreFloat4(&localRotation, localQ);
    }
    else
    {
        localRotation = rot;
    }
}

void Actor::SetScale(const DirectX::XMFLOAT3& s)
{
    scale = s;

    if (auto p = parent.lock())
    {
        DirectX::XMFLOAT3 ps = p->GetScale();
        if (ps.x != 0 && ps.y != 0 && ps.z != 0)
        {
            localScale = { s.x / ps.x, s.y / ps.y, s.z / ps.z };
        }
        else
        {
            localScale = s;
        }
    }
    else
    {
        localScale = s;
    }
}


void Actor::SetLocalPosition(const DirectX::XMFLOAT3& pos)
{
    localPosition = pos;
    if (parent.expired()) position = pos;
}

void Actor::SetLocalRotation(const DirectX::XMFLOAT4& rot)
{
    localRotation = rot;
    if (parent.expired()) rotation = rot;
}

void Actor::SetLocalScale(const DirectX::XMFLOAT3& scale)
{
    localScale = scale;
    if (parent.expired()) this->scale = scale;
}

void Actor::LoadModel(const char* filename, float scaling)
{
 /*   model = std::make_shared<Model>(device, filename, scaling);*/

    //modelFilePath = filename;

    this->model = ResourceManager::Instance().GetModel(filename, scaling);

    this->modelFilePath = filename;

}

Model* Actor::GetModelRaw() const
{
    return model.get();
}


void Actor::RemoveComponent(std::shared_ptr<Component> component)
{
    component->OnDestroy();

    auto it = std::remove(components.begin(), components.end(), component);
    if (it != components.end())
    {
        components.erase(it, components.end());
    }
}


std::shared_ptr<Actor> Actor::Clone(std::shared_ptr<Actor> parentToAttach)
{
    auto newActor = ActorManager::Instance().Create();

    std::string baseName = this->name;
    std::string suffix = " (Clone)";

    if (baseName.size() >= suffix.size() &&
        baseName.compare(baseName.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        newActor->SetName(baseName);
    }
    else
    {
        newActor->SetName(baseName + suffix);
    }


    newActor->SetLocalPosition(this->localPosition);
    newActor->SetLocalRotation(this->localRotation);
    newActor->SetLocalScale(this->localScale);

    newActor->UpdateTransform();

    newActor->model = this->model;
    newActor->modelFilePath = this->modelFilePath;

    for (const auto& component : this->components)
    {
        auto newComp = component->Clone();
        if (newComp)
        {
            newComp->SetActor(newActor);
            newActor->components.push_back(newComp);
        }
    }

    for (auto& childWeak : this->children)
    {
        if (auto child = childWeak.lock())
        {
            auto newChild = child->Clone(newActor);
        }
    }

    if (parentToAttach)
    {
        newActor->SetParent(parentToAttach, false);
    }
    else
    {
        auto myParent = this->GetParent();
        if (myParent)
        {
            newActor->SetParent(myParent, false);
        }
    }

    return newActor;
}

std::shared_ptr<Actor> ActorManager::Create()
{
    auto actor = std::make_shared<Actor>();
    startActors.push_back(actor);
    return actor;
}

void ActorManager::AddActor(std::shared_ptr<Actor> actor)
{
  
    if (actor)
    {
        startActors.push_back(actor);
    }
}

void ActorManager::Remove(std::shared_ptr<Actor> actor)
{
    if (actor)
    {
        removeQueue.push_back(actor);
    }
}

void ActorManager::Clear()
{
    startActors.clear();
    updateActors.clear();
    removeQueue.clear();
}

void ActorManager::Update(float dt)
{
    if (!startActors.empty())
    {
        for (auto& actor : startActors)
        {
            actor->Start();
            updateActors.push_back(actor);
        }
        startActors.clear();
    }

    for (auto& actor : updateActors)
    {
        actor->Update(dt);
    }

    if (!removeQueue.empty())
    {
        for (auto& target : removeQueue)
        {
            auto it = std::find(updateActors.begin(), updateActors.end(), target);
            if (it != updateActors.end())
            {
                updateActors.erase(it);
            }
        }
        removeQueue.clear();
    }
}

void ActorManager::UpdateTransform()
{
    for (auto& actor : updateActors)
    {
        actor->UpdateTransform();
    }
}

void ActorManager::Render(const RenderContext& rc, ModelRenderer* renderer)
{
    for (auto& actor : updateActors)
    {
        actor->Render(renderer);
    }
}
