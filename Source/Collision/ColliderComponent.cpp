#include "ColliderComponent.h"
#include "Actor/Actor.h"
#include "Graphics.h"
#include "Gizmos.h"
#include "Timeline/TimelineSequencerComponent.h"
#include "Component/NodeAttachComponent.h"
#include "CollisionManager.h" 
#include "Model/Model.h"       

#include <SimpleMath.h>
#include <cmath>
#include <algorithm> // ★追加: std::find_if 等を使用するため

// ヘルパー
static float Max3(float a, float b, float c)
{
    float maxVal = a;
    if (b > maxVal) maxVal = b;
    if (c > maxVal) maxVal = c;
    return maxVal;
}

ColliderComponent::~ColliderComponent()
{
    for (auto& e : elements)
    {
        UnregisterFromManager(e);
    }
    elements.clear();
}

void ColliderComponent::Start()
{
    for (auto& e : elements) { RegisterToManager(e); }
}

void ColliderComponent::Update(float)
{
    if (!settings.enabled) return;

    for (auto& e : elements)
    {
        if (e.registeredId == 0 && e.enabled) { RegisterToManager(e); }
        if (e.registeredId != 0) { UpdateToManager(e); }
    }
}

// ────────────────────────────────────────────
// マネージャー連携処理
// ────────────────────────────────────────────
void ColliderComponent::RegisterToManager(Element& e)
{
    if (e.registeredId != 0) return;

    auto owner = GetActor();
    if (!owner) return;

    void* userPtr = owner.get();
    DirectX::XMFLOAT3 center = ComputeWorldCenter(e);
    //float scale = Max3(owner->GetScale().x, owner->GetScale().y, owner->GetScale().z);
    float scale = 1.0f;

    if (e.type == ShapeType::Sphere)
    {
        SphereDesc desc;
        desc.center = center;
        desc.radius = e.radius * scale;
        e.registeredId = CollisionManager::Instance().AddSphere(desc, userPtr, e.attribute);
    }
    else if (e.type == ShapeType::Capsule)
    {
        CapsuleDesc desc;
        desc.base = center;
        desc.radius = e.radius * scale;
        desc.height = e.height * scale;
        e.registeredId = CollisionManager::Instance().AddCapsule(desc, userPtr, e.attribute);
    }
    else if (e.type == ShapeType::Box)
    {
        BoxDesc desc;
        desc.center = center;
        desc.size.x = e.size.x * owner->GetScale().x;
        desc.size.y = e.size.y * owner->GetScale().y;
        desc.size.z = e.size.z * owner->GetScale().z;
        e.registeredId = CollisionManager::Instance().AddBox(desc, userPtr, e.attribute);
    }
}

void ColliderComponent::UpdateToManager(Element& e)
{
    if (e.registeredId == 0) return;

    bool isActive = (e.enabled && settings.enabled);
    if (!isActive)
    {
        CollisionManager::Instance().SetEnabled(e.registeredId, false);
        return;
    }
    CollisionManager::Instance().SetEnabled(e.registeredId, true);

    auto owner = GetActor();
    //float scale = owner ? Max3(owner->GetScale().x, owner->GetScale().y, owner->GetScale().z) : 1.0f;
    float scale = 1.0f;
    DirectX::XMFLOAT3 center = ComputeWorldCenter(e);

    if (e.type == ShapeType::Sphere)
    {
        SphereDesc desc;
        desc.center = center;
        desc.radius = e.radius * scale;
        CollisionManager::Instance().UpdateSphere(e.registeredId, desc);
    }
    else if (e.type == ShapeType::Capsule)
    {
        CapsuleDesc desc;
        desc.base = center;
        desc.radius = e.radius * scale;
        desc.height = e.height * scale;
        CollisionManager::Instance().UpdateCapsule(e.registeredId, desc);
    }
    else if (e.type == ShapeType::Box)
    {
        BoxDesc desc;
        desc.center = center;
        desc.size.x = e.size.x * owner->GetScale().x;
        desc.size.y = e.size.y * owner->GetScale().y;
        desc.size.z = e.size.z * owner->GetScale().z;
        CollisionManager::Instance().UpdateBox(e.registeredId, desc);
    }
}

void ColliderComponent::UnregisterFromManager(Element& e)
{
    if (e.registeredId != 0)
    {
        CollisionManager::Instance().Remove(e.registeredId);
        e.registeredId = 0;
    }
}

// ────────────────────────────────────────────
// ヘルパー関数 (属性対応)
// ────────────────────────────────────────────
void ColliderComponent::AddSphere(const DirectX::SimpleMath::Vector3& offset, float radius, ColliderAttribute attr)
{
    Element e;
    e.type = ShapeType::Sphere;
    e.offsetLocal = offset;
    e.radius = radius;
    e.attribute = attr;
    elements.push_back(e);
}

void ColliderComponent::AddCapsule(const DirectX::SimpleMath::Vector3& offset, float radius, float height, ColliderAttribute attr)
{
    Element e;
    e.type = ShapeType::Capsule;
    e.offsetLocal = offset;
    e.radius = radius;
    e.height = height;
    e.attribute = attr;
    elements.push_back(e);
}

void ColliderComponent::AddBox(const DirectX::SimpleMath::Vector3& offset, const DirectX::SimpleMath::Vector3& size, ColliderAttribute attr)
{
    Element e;
    e.type = ShapeType::Box;
    e.offsetLocal = offset;
    e.size = size;
    e.attribute = attr;
    elements.push_back(e);
}

// ────────────────────────────────────────────
// 既存の関数群
// ────────────────────────────────────────────

DirectX::XMFLOAT3 ColliderComponent::ComputeWorldCenter(const Element& e)
{
    auto owner = GetActor();
    if (!owner) return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

    // アクターのワールド行列（位置・回転・スケール）を取得
    DirectX::XMFLOAT4X4 actorWorld = owner->GetTransform();
    DirectX::XMMATRIX matActor = DirectX::XMLoadFloat4x4(&actorWorld);

    // ボーン追従がある場合
    if (e.nodeIndex >= 0)
    {
        const ::Model* model = owner->GetModelRaw();
        if (model)
        {
            // 1. ボーンのモデル空間での位置を取得
            // NodeAttachComponentのヘルパーは「モデル原点からの位置」を返す想定
            DirectX::XMFLOAT3 posModelSpace = NodeAttachComponent::GetWorldPosition_NodeLocal(
                model, e.nodeIndex, DirectX::XMFLOAT3(e.offsetLocal.x, e.offsetLocal.y, e.offsetLocal.z)
            );

            // 2. ★重要修正: それをアクターのワールド行列で変換する！
            // これを忘れていたため、敵が動くと判定が置いていかれていました
            DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&posModelSpace);
            vPos = DirectX::XMVector3TransformCoord(vPos, matActor);

            DirectX::XMFLOAT3 result;
            DirectX::XMStoreFloat3(&result, vPos);
            return result;
        }
    }

    // ボーンなし (Actor原点からのオフセット)
    //DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(e.offsetLocal.x, e.offsetLocal.y, e.offsetLocal.z);
    //DirectX::XMMATRIX M = T * matActor;

    //DirectX::XMFLOAT4X4 out{};
    //DirectX::XMStoreFloat4x4(&out, M);
    //return DirectX::XMFLOAT3(out._41, out._42, out._43);

    DirectX::SimpleMath::Vector3 pos = owner->GetPosition();
    DirectX::SimpleMath::Quaternion rot = owner->GetRotation();

    // S=1.0, R=rot, T=pos の行列を作成
    DirectX::XMMATRIX matRot = DirectX::XMMatrixRotationQuaternion(rot);
    DirectX::XMMATRIX matTrans = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
    DirectX::XMMATRIX matActorUnscaled = matRot * matTrans;

    // オフセット行列を作成
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(e.offsetLocal.x, e.offsetLocal.y, e.offsetLocal.z);

    // ★修正: スケールなしの行列と掛け合わせる
    // これで offsetLocal (0, 11, 0) は、0.1倍されずにそのまま +11.0 されます
    DirectX::XMMATRIX M = T * matActorUnscaled;

    DirectX::XMFLOAT4X4 out{};
    DirectX::XMStoreFloat4x4(&out, M);
    return DirectX::XMFLOAT3(out._41, out._42, out._43);

}

void ColliderComponent::Render()
{
    if (!settings.enabled || !settings.drawGizmo) return;

    auto owner = GetActor(); if (!owner) return;
    auto gizmo = Graphics::Instance().GetGizmos(); if (!gizmo) return;

    const DirectX::SimpleMath::Vector3 scale = { 1.0f, 1.0f, 1.0f };

    // const float maxScale = Max3(scale.x, scale.y, scale.z);
    const float maxScale = 1.0f;

    for (const auto& e : elements)
    {
        if (!e.enabled) continue;

        DirectX::XMFLOAT3 centerW = ComputeWorldCenter(e);
        DirectX::XMFLOAT4 col{ e.color.x, e.color.y, e.color.z, e.color.w };
        const float r = ClampMin(e.radius * maxScale, 0.001f);

        if (e.attribute == ColliderAttribute::Attack) {
            col = { 1.0f, 0.0f, 0.0f, 0.8f };
        }

        if (e.type == ShapeType::Sphere)
        {
            gizmo->DrawSphere(centerW, r, col);
        }
        else if (e.type == ShapeType::Capsule)
        {
            const float h = ClampMin(e.height * maxScale, 0.0f);
            const DirectX::XMFLOAT3 ang{ 0,0,0 };
            gizmo->DrawCapsule(centerW, ang, r, h, col);
        }
        else if (e.type == ShapeType::Box)
        {
            DirectX::XMFLOAT3 sizeW = {
                e.size.x * scale.x,
                e.size.y * scale.y,
                e.size.z * scale.z
            };
            const DirectX::XMFLOAT3 ang{ 0, 0, 0 };
            gizmo->DrawBox(centerW, ang, sizeW, col);
        }
    }
}

std::shared_ptr<Component> ColliderComponent::Clone()
{
    auto clone = std::make_shared<ColliderComponent>();
    clone->settings = this->settings;
    clone->elements = this->elements;
    for (auto& e : clone->elements)
    {
        e.registeredId = 0;
        e.runtimeTag = 0;
    }
    return clone;
}

// ────────────────────────────────────────────
// ★修正: シーケンサー由来(runtimeTag != 0)の要素を全削除
// ────────────────────────────────────────────
void ColliderComponent::ClearSequencerRuntime()
{
    auto it = elements.begin();
    while (it != elements.end())
    {
        // 0以外はすべてシーケンサー管理とみなして削除
        if (it->runtimeTag != 0)
        {
            UnregisterFromManager(*it);
            it = elements.erase(it);
        }
        else
        {
            ++it;
        }
    }
}


void ColliderComponent::SyncFromSequencer(TimelineSequencerComponent* seq, int currentFrame)
{
    if (!seq) return;

    // 前回の「全削除(ClearSequencerRuntime)」は呼びません！
    // 既存のIDを維持して更新するためです。

    const auto& items = seq->GetItems();

    // 1. 今回のフレームで有効であるべきアイテムのリストを作る
    std::vector<int> activeIndices;
    for (int i = 0; i < (int)items.size(); ++i)
    {
        const auto& it = items[i];
        if (it.type != 0) continue; // Hitboxのみ

        if (currentFrame >= it.start && currentFrame <= it.end)
        {
            activeIndices.push_back(i);
        }
    }

    // 2. 有効なアイテムについて「更新」または「新規作成」
    for (int index : activeIndices)
    {
        // アイテムを一意に識別するために (index + 1) をタグとして利用
        // (0は静的コライダー用なので+1する)
        int targetTag = index + 1;
        const auto& it = items[index];

        // 既にこのタグを持つElementがあるか探す
        auto found = std::find_if(elements.begin(), elements.end(),
            [targetTag](const Element& e) { return e.runtimeTag == targetTag; });

        if (found != elements.end())
        {
            // [A] 既に存在する -> パラメータ更新 (ID維持！)
            Element& e = *found;
            e.nodeIndex = it.hb.nodeIndex;
            e.offsetLocal = DirectX::SimpleMath::Vector3(it.hb.offsetLocal.x, it.hb.offsetLocal.y, it.hb.offsetLocal.z);
            e.radius = it.hb.radius;
            // 位置とサイズを物理マネージャーへ即時反映
            UpdateToManager(e);
        }
        else
        {
            // [B] 存在しない -> 新規作成して登録
            Element e{};
            e.enabled = true;
            e.type = ShapeType::Sphere;
            e.nodeIndex = it.hb.nodeIndex;
            e.offsetLocal = DirectX::SimpleMath::Vector3(it.hb.offsetLocal.x, it.hb.offsetLocal.y, it.hb.offsetLocal.z);
            e.radius = it.hb.radius;
            e.color = DirectX::SimpleMath::Vector4(1, 0, 0, 0.35f);
            e.runtimeTag = targetTag; // ★タグを設定して管理下に置く
            e.label = "HB(runtime)";
            e.attribute = ColliderAttribute::Attack;

            RegisterToManager(e); // ID発行
            elements.push_back(e);
        }
    }

    // 3. 有効期限が切れたElementを削除
    // (runtimeTag != 0 かつ activeIndices に含まれないものを消す)
    auto it = elements.begin();
    while (it != elements.end())
    {
        // シーケンサー由来のものだけチェック
        if (it->runtimeTag != 0)
        {
            int originalIndex = it->runtimeTag - 1;
            bool isActive = false;
            for (int idx : activeIndices) {
                if (idx == originalIndex) { isActive = true; break; }
            }

            if (!isActive)
            {
                // もう有効範囲外なので削除
                UnregisterFromManager(*it);
                it = elements.erase(it);
                continue;
            }
        }
        ++it;
    }
}



void ColliderComponent::OnGUI()
{
   
        ImGui::Checkbox("Enabled", &settings.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Show Gizmo", &settings.drawGizmo);
        ImGui::Separator();

        for (int i = 0; i < (int)elements.size(); ++i)
        {
            auto& e = elements[i];
            ImGui::PushID(i);
            std::string label = "Unknown";
            if (e.type == ShapeType::Sphere) label = "Sphere";
            else if (e.type == ShapeType::Capsule) label = "Capsule";
            else if (e.type == ShapeType::Box) label = "Box";
            label += " " + std::to_string(i);

            bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Delete Shape"))
                {
                    UnregisterFromManager(e);
                    elements.erase(elements.begin() + i);
                    ImGui::EndPopup();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    i--;
                    continue;
                }
                ImGui::EndPopup();
            }

            if (open)
            {
                if (ImGui::Checkbox("Active", &e.enabled)) {
                    if (e.enabled) RegisterToManager(e);
                    else UnregisterFromManager(e);
                }

                const char* attrs[] = { "Body", "Attack" };
                int attrIdx = (int)e.attribute;
                if (ImGui::Combo("Attribute", &attrIdx, attrs, IM_ARRAYSIZE(attrs)))
                {
                    UnregisterFromManager(e);
                    e.attribute = (ColliderAttribute)attrIdx;
                    RegisterToManager(e);
                }

                if (ImGui::DragFloat3("Offset", &e.offsetLocal.x, 0.01f)) UpdateToManager(e);

                if (e.type == ShapeType::Sphere)
                {
                    if (ImGui::DragFloat("Radius", &e.radius, 0.01f, 0.001f)) UpdateToManager(e);
                }
                else if (e.type == ShapeType::Capsule)
                {
                    if (ImGui::DragFloat("Radius", &e.radius, 0.01f, 0.001f)) UpdateToManager(e);
                    if (ImGui::DragFloat("Height", &e.height, 0.01f, 0.001f)) UpdateToManager(e);
                }
                else if (e.type == ShapeType::Box)
                {
                    if (ImGui::DragFloat3("Size", &e.size.x, 0.01f, 0.001f)) UpdateToManager(e);
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::Separator();
        if (ImGui::Button("Add Shape..."))
        {
            ImGui::OpenPopup("AddShapePopup");
        }

        if (ImGui::BeginPopup("AddShapePopup"))
        {
            if (ImGui::MenuItem("Add Sphere"))  AddSphere({ 0,0,0 }, 0.5f);
            if (ImGui::MenuItem("Add Capsule")) AddCapsule({ 0,0,0 }, 0.5f, 1.0f);
            if (ImGui::MenuItem("Add Box"))     AddBox({ 0,0,0 }, { 1,1,1 });
            ImGui::EndPopup();
        }
    
}

void ColliderComponent::Serialize(json& outJson) const
{
    outJson["enabled"] = settings.enabled;
    outJson["drawGizmo"] = settings.drawGizmo;

    json shapes = json::array();
    for (const auto& e : elements)
    {
        // ランタイム生成されたもの（攻撃判定）は保存しない
        if (e.runtimeTag != 0) continue;

        json j;
        j["type"] = (int)e.type;
        j["enabled"] = e.enabled;
        j["attribute"] = (int)e.attribute;
        j["offset"] = (DirectX::XMFLOAT3)e.offsetLocal;
        j["size"] = (DirectX::XMFLOAT3)e.size;
        j["radius"] = e.radius;
        j["height"] = e.height;
        shapes.push_back(j);
    }
    outJson["shapes"] = shapes;
}

void ColliderComponent::Deserialize(const json& inJson)
{
    if (inJson.contains("enabled")) settings.enabled = inJson["enabled"];
    if (inJson.contains("drawGizmo")) settings.drawGizmo = inJson["drawGizmo"];

    if (inJson.contains("shapes") && inJson["shapes"].is_array())
    {
        elements.clear();
        for (const auto& j : inJson["shapes"])
        {
            Element e;
            e.type = (ShapeType)j.value("type", 0);
            e.enabled = j.value("enabled", true);
            e.attribute = (ColliderAttribute)j.value("attribute", 0);

            if (j.contains("offset")) {
                DirectX::XMFLOAT3 v = j["offset"].get<DirectX::XMFLOAT3>();
                e.offsetLocal = { v.x, v.y, v.z };
            }
            if (j.contains("size")) {
                DirectX::XMFLOAT3 v = j["size"].get<DirectX::XMFLOAT3>();
                e.size = { v.x, v.y, v.z };
            }

            e.radius = j.value("radius", 0.5f);
            e.height = j.value("height", 1.0f);

            // runtimeTagは0で初期化されるのでそのままでOK

            RegisterToManager(e);
            elements.push_back(e);
        }
    }
}

// ColliderComponent.cpp の最後に追加

float ColliderComponent::GetMaxRadiusXZ() const
{
    using namespace DirectX;
    using namespace DirectX::SimpleMath;

    float maxRadius = 0.0f; // デフォルト (コライダーなしなら0)

    for (const auto& e : elements)
    {
        // 無効なコライダーは無視
        if (!e.enabled) continue;

        // アクター中心(0,0)から、コライダー中心(offset)までの距離
        // Y軸(高さ)は壁判定に関係ないので無視してXZのみで計算
        Vector2 offsetXZ = Vector2(e.offsetLocal.x, e.offsetLocal.z);
        float distToCenter = offsetXZ.Length();

        float shapeRadius = 0.0f;

        // 形状ごとの「厚み」を計算
        switch (e.type)
        {
        case ShapeType::Sphere:
        case ShapeType::Capsule:
            // 球とカプセルは radius そのもの
            shapeRadius = e.radius;
            break;

        case ShapeType::Box:
            // ボックスの場合、回転を考慮しない簡易計算として
            // 「XZ断面の対角線の半分」を半径とみなす (一番出っ張る角までの距離)
            // size は全幅なので * 0.5 する
            shapeRadius = Vector2(e.size.x, e.size.z).Length() * 0.5f;
            break;
        }

        // 「中心までのズレ + その形状の半径」が、アクター中心からの最大到達距離
        float totalDist = distToCenter + shapeRadius;

        // 最大値を更新
        if (totalDist > maxRadius)
        {
            maxRadius = totalDist;
        }
    }

    // もしコライダーが1つもなければ、最低限の半径 (0.5m) を返しても良いが、
    // ここでは忠実に 0.0f を返す実装とする
    return maxRadius;
}