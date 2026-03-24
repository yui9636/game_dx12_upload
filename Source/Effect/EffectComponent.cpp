#include "EffectComponent.h"
#include "Actor/Actor.h"
#include "Model/Model.h"
#include "Effect/EffectManager.h"
#include "Effect/EffectNode.h" 
#include <imgui.h>
#include "System/Dialog.h" 
#include <filesystem>

using namespace DirectX;

void EffectComponent::Serialize(json& outJson) const
{
    std::vector<json> effectsJsonArray;

    for (const auto& ae : activeEffects)
    {
        if (ae.effectPath.empty()) continue;

        json effectJson;
        effectJson["EffectPath"] = ae.effectPath;
        effectJson["Loop"] = ae.loop;
        effectJson["BoneName"] = ae.targetBoneName;

        effectJson["Offset"] = { ae.localOffsetPosition.x, ae.localOffsetPosition.y, ae.localOffsetPosition.z };
        effectJson["Rotation"] = { ae.localOffsetRotation.x, ae.localOffsetRotation.y, ae.localOffsetRotation.z };
        effectJson["Scale"] = { ae.localOffsetScale.x, ae.localOffsetScale.y, ae.localOffsetScale.z };

        effectsJsonArray.push_back(effectJson);
    }

    if (!effectsJsonArray.empty())
    {
        outJson["Effects"] = effectsJsonArray;
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void EffectComponent::Deserialize(const json& inJson)
{
    StopAll();
    activeEffects.clear();

    if (inJson.contains("Effects") && inJson["Effects"].is_array())
    {
        for (const auto& effectJson : inJson["Effects"])
        {
            ActiveEffect ae;

            if (effectJson.contains("EffectPath")) ae.effectPath = effectJson["EffectPath"];
            if (effectJson.contains("Loop")) ae.loop = effectJson["Loop"];
            if (effectJson.contains("BoneName")) ae.targetBoneName = effectJson["BoneName"];

            if (effectJson.contains("Offset")) {
                auto& v = effectJson["Offset"];
                if (v.size() >= 3) ae.localOffsetPosition = { v[0], v[1], v[2] };
            }
            if (effectJson.contains("Rotation")) {
                auto& v = effectJson["Rotation"];
                if (v.size() >= 3) ae.localOffsetRotation = { v[0], v[1], v[2] };
            }
            if (effectJson.contains("Scale")) {
                auto& v = effectJson["Scale"];
                if (v.size() >= 3) ae.localOffsetScale = { v[0], v[1], v[2] };
            }

            activeEffects.push_back(ae);
        }
    }
}




static XMMATRIX CalcCombinedMatrix(
    const XMFLOAT4X4& actorWorld,
    const XMFLOAT4X4& boneWorld,
    const XMFLOAT3& offsetPos,
    const XMFLOAT3& offsetRotDeg,
    const XMFLOAT3& offsetScale)
{
    XMMATRIX W_Actor = XMLoadFloat4x4(&actorWorld);
    XMMATRIX W_Bone = XMLoadFloat4x4(&boneWorld);

    XMMATRIX M_Scale = XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z);
    XMMATRIX M_Rot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(offsetRotDeg.x),
        XMConvertToRadians(offsetRotDeg.y),
        XMConvertToRadians(offsetRotDeg.z)
    );
    XMMATRIX M_Trans = XMMatrixTranslation(offsetPos.x, offsetPos.y, offsetPos.z);

    XMMATRIX W_Offset = M_Scale * M_Rot * M_Trans;

    return W_Offset * W_Bone * W_Actor;
}

void EffectComponent::Update(float dt)
{
    auto actor = GetActor();
    if (!actor) return;

    XMMATRIX W_Actor = XMLoadFloat4x4(&actor->GetTransform());

    Model* model = actor->GetModelRaw();
    const auto& nodes = model ? model->GetNodes() : std::vector<Model::Node>();

    //auto it = std::remove_if(activeEffects.begin(), activeEffects.end(),
    //    [](const ActiveEffect& ae) { return ae.instance.expired(); });
    //activeEffects.erase(it, activeEffects.end());

    for (auto& ae : activeEffects)
    {
        auto instance = ae.instance.lock();
        if (!instance) continue;

        instance->loop = ae.loop;

        XMMATRIX W_Socket = XMMatrixIdentity();

        if (!ae.targetBoneName.empty() && model)
        {
            if (ae.targetBoneIndex == -1) {
                for (size_t i = 0; i < nodes.size(); ++i) {
                    if (nodes[i].name == ae.targetBoneName) {
                        ae.targetBoneIndex = (int)i;
                        break;
                    }
                }
            }

            if (ae.targetBoneIndex >= 0 && ae.targetBoneIndex < (int)nodes.size()) {
                XMMATRIX W_Bone = XMLoadFloat4x4(&nodes[ae.targetBoneIndex].worldTransform);
                W_Socket = W_Bone * W_Actor;
            }
            else {
                W_Socket = W_Actor;
            }
        }
        else
        {
            W_Socket = W_Actor;
        }

        {
            XMFLOAT4X4 m; XMStoreFloat4x4(&m, W_Socket);

            XMVECTOR ax = XMVector3Normalize(XMVectorSet(m._11, m._12, m._13, 0));
            XMVECTOR ay = XMVector3Normalize(XMVectorSet(m._21, m._22, m._23, 0));
            XMVECTOR az = XMVector3Normalize(XMVectorSet(m._31, m._32, m._33, 0));
            XMVECTOR p = XMVectorSet(m._41, m._42, m._43, 1);

            W_Socket = XMMatrixIdentity();
            W_Socket.r[0] = ax;
            W_Socket.r[1] = ay;
            W_Socket.r[2] = az;
            W_Socket.r[3] = p;
        }

        XMStoreFloat4x4(&instance->parentMatrix, W_Socket);

        if (instance->rootNode)
        {
            instance->rootNode->localTransform.position = ae.localOffsetPosition;
            instance->rootNode->localTransform.rotation = ae.localOffsetRotation;
            instance->rootNode->localTransform.scale = ae.localOffsetScale;

            instance->overrideLocalTransform.position = ae.localOffsetPosition;
            instance->overrideLocalTransform.rotation = ae.localOffsetRotation;
            instance->overrideLocalTransform.scale = ae.localOffsetScale;
        }
    }
}

std::weak_ptr<EffectInstance> EffectComponent::Play(
    const std::string& effectName,
    const std::string& boneName,
    const DirectX::XMFLOAT3& offset,
    const DirectX::XMFLOAT3& rotation,
    const DirectX::XMFLOAT3& scale,
    bool loop)
{
    auto actor = GetActor();
    if (!actor) return {};

    XMFLOAT3 spawnPos = actor->GetPosition();
    auto instance = EffectManager::Get().Play(effectName, spawnPos);

    if (instance)
    {
        instance->loop = loop;

        ActiveEffect ae;
        ae.instance = instance;
        ae.effectPath = effectName;
        ae.loop = loop;
        ae.targetBoneName = boneName;
        ae.localOffsetPosition = offset;
        ae.localOffsetRotation = rotation;
        ae.localOffsetScale = scale;
        ae.targetBoneIndex = -1;

        activeEffects.push_back(ae);
    }
    return instance;
}


void EffectComponent::SetEffectTransform(
    const std::weak_ptr<EffectInstance>& handle,
    const DirectX::XMFLOAT3& offset,
    const DirectX::XMFLOAT3& rotation,
    const DirectX::XMFLOAT3& scale)
{
    auto targetPtr = handle.lock();
    if (!targetPtr) return;

    for (auto& ae : activeEffects)
    {
        auto aePtr = ae.instance.lock();
        if (aePtr == targetPtr)
        {
            ae.localOffsetPosition = offset;
            ae.localOffsetRotation = rotation;
            ae.localOffsetScale = scale;
            return;
        }
    }
}

void EffectComponent::Stop(const std::weak_ptr<EffectInstance>& handle) { if (auto ptr = handle.lock()) ptr->Stop(); }
void EffectComponent::StopAll() { for (auto& ae : activeEffects) if (auto ptr = ae.instance.lock()) ptr->Stop(); activeEffects.clear(); }

void EffectComponent::OnGUI()
{
    if (!ImGui::CollapsingHeader("Effect Component", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (activeEffects.empty()) {
        ActiveEffect ae;
        ae.loop = true;
        ae.localOffsetScale = { 1.0f, 1.0f, 1.0f };
        activeEffects.push_back(ae);
    }

    for (int i = 0; i < (int)activeEffects.size(); ++i)
    {
        auto& ae = activeEffects[i];
        auto instance = ae.instance.lock();

        ImGui::PushID(i);

        std::string displayName = ae.effectPath;
        if (!displayName.empty()) {
            std::filesystem::path p(displayName);
            displayName = p.filename().string();
        }
        else {
            displayName = "No File";
        }

        bool open = ImGui::TreeNode(displayName.c_str());

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Add Slot")) {
                ActiveEffect newAe;
                newAe.loop = true;
                newAe.localOffsetScale = { 1.0f, 1.0f, 1.0f };
                activeEffects.push_back(newAe);
            }
            if (ImGui::MenuItem("Remove")) {
                if (instance) instance->Stop(true);
                activeEffects.erase(activeEffects.begin() + i);
                ImGui::EndPopup();
                ImGui::TreePop();
                ImGui::PopID();
                i--;
                continue;
            }
            ImGui::EndPopup();
        }

        if (open)
        {
            char pathBuf[MAX_PATH];
            strcpy_s(pathBuf, ae.effectPath.c_str());
            if (ImGui::InputText("File", pathBuf, sizeof(pathBuf))) {
                ae.effectPath = pathBuf;
            }
            ImGui::SameLine();

            if (ImGui::Button("...")) {
                char path[MAX_PATH] = "";
                if (Dialog::OpenFileName(path, MAX_PATH, "Effect JSON\0*.json\0All Files\0*.*\0") == DialogResult::OK) {
                    std::string fullPath = path;
                    size_t p = fullPath.find("Data\\");
                    if (p != std::string::npos) ae.effectPath = fullPath.substr(p);
                    else ae.effectPath = fullPath;
                    std::replace(ae.effectPath.begin(), ae.effectPath.end(), '\\', '/');

                    if (auto old = ae.instance.lock()) old->Stop(true);

                    auto actor = GetActor();
                    if (actor) {
                        auto newInst = EffectManager::Get().Play(ae.effectPath, actor->GetPosition());
                        if (newInst) {
                            newInst->loop = ae.loop;
                            EffectTransform tf;
                            tf.position = ae.localOffsetPosition;
                            tf.rotation = ae.localOffsetRotation;
                            tf.scale = ae.localOffsetScale;
                            newInst->overrideLocalTransform = tf;

                            ae.instance = newInst;
                        }
                    }
                }
            }

            ImGui::DragFloat3("Offset", &ae.localOffsetPosition.x, 0.01f);
            ImGui::DragFloat3("Rotation", &ae.localOffsetRotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &ae.localOffsetScale.x, 0.01f, 0.001f, 100.0f);

            if (ImGui::Checkbox("Loop", &ae.loop)) {
                if (instance) instance->loop = ae.loop;
            }

            ImGui::Separator();
            float width = ImGui::GetContentRegionAvail().x;

            if (ImGui::Button("Play", ImVec2(width * 0.5f, 0)))
            {
                if (auto old = ae.instance.lock()) old->Stop(true);

                auto actor = GetActor();
                if (actor && !ae.effectPath.empty()) {
                    auto newInst = EffectManager::Get().Play(ae.effectPath, actor->GetPosition());
                    if (newInst) {
                        newInst->loop = ae.loop;

                        EffectTransform tf;
                        tf.position = ae.localOffsetPosition;
                        tf.rotation = ae.localOffsetRotation;
                        tf.scale = ae.localOffsetScale;
                        newInst->overrideLocalTransform = tf;

                        ae.instance = newInst;
                    }
                }
            }
            ImGui::SameLine();

            if (ImGui::Button("Stop", ImVec2(width * 0.5f, 0)))
            {
                if (auto inst = ae.instance.lock()) inst->Stop(true);
            }

            if (instance) ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Playing");
            else ImGui::TextDisabled("Status: Stopped");

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

