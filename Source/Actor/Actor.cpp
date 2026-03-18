#include "Actor.h"
#include "Component/Component.h"
#include "Model/Model.h"
#include <Model\ModelRenderer.h>
#include "Actor.h"
#include "Component/Component.h"
#include "Model/Model.h"
#include <Model/ModelRenderer.h> // �p�X�͊��ɍ��킹�Ē������Ă�������
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

// ���C��: �e�q�֌W���l�������s��X�V
// DirectX:: �𖾎����āu�����܂��v�G���[�����
void Actor::UpdateTransform()
{
    // 1. ���[�J���s��̍쐬 (S * R * T)
    DirectX::XMMATRIX S = DirectX::XMMatrixScaling(localScale.x, localScale.y, localScale.z);
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&localRotation));
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(localPosition.x, localPosition.y, localPosition.z);

    DirectX::XMMATRIX localMatrix = S * R * T;
    DirectX::XMMATRIX worldMatrix = localMatrix;

    // 2. �e������ꍇ�A�e�̍s����|����
    std::shared_ptr<Actor> parentPtr = parent.lock();
    if (parentPtr)
    {
        DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&parentPtr->GetTransform());
        worldMatrix = localMatrix * parentWorld;
    }

    // 3. �v�Z���ʂ����[���h���W�n�ϐ��iposition/rotation/scale/transform�j�ɏ����߂�
    DirectX::XMStoreFloat4x4(&transform, worldMatrix);

    // �s�񂩂烏�[���h���W�����𕪉����ăL���b�V��
    DirectX::XMVECTOR outScale, outRot, outTrans;
    DirectX::XMMatrixDecompose(&outScale, &outRot, &outTrans, worldMatrix);

    DirectX::XMStoreFloat3(&position, outTrans);
    DirectX::XMStoreFloat4(&rotation, outRot);
    DirectX::XMStoreFloat3(&scale, outScale);

    // ���f���̍X�V
    //if (model) model->UpdateTransform(transform);

    if (model)
    {
        DirectX::XMFLOAT4X4 identity;
        DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
        model->UpdateTransform(identity);
    }

    // 4. �q�A�N�^�[���X�V�i�ċA�j
    for (auto& childWeak : children)
    {
        if (auto child = childWeak.lock())
        {
            child->UpdateTransform();
        }
    }
}

// --- �e�q�֌W���� ---

void Actor::SetParent(std::shared_ptr<Actor> newParent, bool keepWorldTransform)
{
    std::shared_ptr<Actor> oldParent = parent.lock();
    if (oldParent == newParent) return;

    // 1. ���e���痣�E
    if (oldParent)
    {
        oldParent->RemoveChild(shared_from_this());
    }

    // 2. �V�e�ݒ�
    parent = newParent;

    if (newParent)
    {
        newParent->AddChild(shared_from_this());

        if (keepWorldTransform)
        {
            // ���[���h���W���ێ����邽�߂Ƀ��[�J�����W���Čv�Z
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
            // �ʒu�ێ��Ȃ��Ȃ�A���݈ʒu�����̂܂܃��[�J���Ƃ���
            localPosition = position;
            localRotation = rotation;
            localScale = scale;
        }
    }
    else
    {
        // �e�����Ȃ��Ȃ����ꍇ�A���[�J�������[���h
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

// --- ���[���h���WSetter�i�����݊��j ---

void Actor::SetPosition(const DirectX::XMFLOAT3& pos)
{
    position = pos;

    // �e������ꍇ�A���[���h���W���烍�[�J�����W���t�Z���ĕۑ�
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
        // �e�����Ȃ���� ���[�J�� = ���[���h
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
        // ���C��: �O�񂱂��� localPosition �ɓ���Ă��܂��~�X������܂����BlocalScale�������ł��B
        if (ps.x != 0 && ps.y != 0 && ps.z != 0)
        {
            localScale = { s.x / ps.x, s.y / ps.y, s.z / ps.z };
        }
        else
        {
            localScale = s; // �e�̃X�P�[����0�̏ꍇ�͂��̂܂ܐݒ�i��O�����j
        }
    }
    else
    {
        localScale = s;
    }
}

// --- ���[�J�����WAccessors ---

void Actor::SetLocalPosition(const DirectX::XMFLOAT3& pos)
{
    localPosition = pos;
    // �e�����Ȃ��ꍇ�̓��[���h���W���X�V���Ă���
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

// ... LoadModel, GetModelRaw �͕ύX�Ȃ� ...
void Actor::LoadModel(const char* filename, float scaling)
{
 /*   model = std::make_shared<Model>(device, filename, scaling);*/

    //modelFilePath = filename;

    // �Â����ڃ��[�h�������̂āA�}�l�[�W���[�ɈϏ�����
    this->model = ResourceManager::Instance().GetModel(filename, scaling);

    // �p�X��ێ����Ă����i�V���A���C�Y�p�j
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
    // 1. �V�����A�N�^�[�̐����i�܂�Manager�ɂ͓o�^���Ȃ��j
    // ActorManager::Create() ���g���Ǝ����o�^����邪�A�R�s�[���͎蓮���䂵�����ꍇ������B
    // �����ł� Manager �o�R�ō쐬���ēo�^�܂ōς܂���̂����S�B
    auto newActor = ActorManager::Instance().Create();

    std::string baseName = this->name;
    std::string suffix = " (Clone)";

    // ���ɖ����� " (Clone)" �ŏI����Ă��邩�`�F�b�N
    if (baseName.size() >= suffix.size() &&
        baseName.compare(baseName.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        // ���� (Clone) �����Ă���ꍇ�́A���̂܂܂̖��O���g���i�܂��͘A�ԏ����ɂ���j
        // �����ł́u���B�����Ȃ��v���j�ŁA���̂܂܂̖��O�ɂ��܂��B
        // ���� "Object (Clone) 2" �݂����ɂ������ꍇ�́A�����ŕ������͂��K�v�ł��B
        newActor->SetName(baseName);
    }
    else
    {
        // �t���Ă��Ȃ���Εt����
        newActor->SetName(baseName + suffix);
    }


    // Transform: �e�q�֌W���l�����ă��[�J���l���R�s�[
    // (Clone����͐e�����Ȃ��̂ŁA���[�J���l���Z�b�g�����OK)
    newActor->SetLocalPosition(this->localPosition);
    newActor->SetLocalRotation(this->localRotation);
    newActor->SetLocalScale(this->localScale);

    // ���[���h���W�n���O�̂��ߍX�V
    newActor->UpdateTransform();

    // 3. ���f���̎Q�ƃR�s�[
    // ���f���f�[�^���̂͏d���̂ŁA�|�C���^�������L���� (���L���\�[�X�̂���)
    newActor->model = this->model;
    newActor->modelFilePath = this->modelFilePath;

    // 4. �R���|�[�l���g�̃f�B�[�v�R�s�[
    for (const auto& component : this->components)
    {
        // �R���|�[�l���g���Ƃ� Clone �������Ăяo��
        auto newComp = component->Clone();
        if (newComp)
        {
            // �V�����A�N�^�[�ɕR�t����
            newComp->SetActor(newActor);
            newActor->components.push_back(newComp);
            // Start�Ȃǂ͌��Manager���ꊇ�ŌĂԂ��A�����ŌĂԂ��݌v����
            // Manager::Create�o�R�Ȃ�Start�҂����X�g�ɓ���̂�OK
        }
    }

    // 5. �q�A�N�^�[�̍ċA�R�s�[
    for (auto& childWeak : this->children)
    {
        if (auto child = childWeak.lock())
        {
            // �q���N���[�����A�V�����A�N�^�[(newActor)��e�ɂ���
            auto newChild = child->Clone(newActor);
        }
    }

    // 6. �w�肳�ꂽ�e������ΐe�q�t��
    if (parentToAttach)
    {
        newActor->SetParent(parentToAttach, false); // ���[�J�����W�ێ��Őe�t��
    }
    // �w�肪�Ȃ���΁A���̐e�Ɠ����e�ɂ���i�Z��R�s�[�j
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
    // Start����
    if (!startActors.empty())
    {
        for (auto& actor : startActors)
        {
            actor->Start();
            updateActors.push_back(actor);
        }
        startActors.clear();
    }

    // Update����
    for (auto& actor : updateActors)
    {
        actor->Update(dt);
    }

    // �폜����
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

// ���ǉ�: �ꊇ�`��o�^
void ActorManager::Render(const RenderContext& rc, ModelRenderer* renderer)
{
    // �e�A�N�^�[��Render���Ă�
    // (�����͎g��Ȃ��݌v�ɂ��Ă��܂����A�����̂��߂ɓn���Ă�OK)
    for (auto& actor : updateActors)
    {
        actor->Render(renderer);
    }
}