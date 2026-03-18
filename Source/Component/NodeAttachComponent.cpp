#include "Component/NodeAttachComponent.h"
#include "Actor/Actor.h"
#include "Model.h" 
#include "Graphics.h" // デバッグ描画が必要ならインクルード
#include <imgui.h>
#include <algorithm>

using namespace DirectX;

// =========================================================
// ライフサイクル
// =========================================================

void NodeAttachComponent::Start()
{
    lastModelPtr = nullptr;
}

void NodeAttachComponent::Update(float /*dt*/)
{
    // 自分自身のアタッチ処理（isAttached == true の時のみ動く）
    if (!isAttached) return;

    std::shared_ptr<Actor> owner = GetActor();
    if (!owner) return;

    // ターゲットがいなければ自分（Owner）をターゲットと見なす（自己参照ボーン追従）
    std::shared_ptr<Actor> parent = targetActor ? targetActor : owner;
    const ::Model* model = parent->GetModelRaw();
    if (!model) return;

    // モデルが変わっていたらキャッシュリセット
    if (model != lastModelPtr) {
        lastModelPtr = model;
        for (auto& pair : sockets) pair.second.cachedBoneIndex = -1;
        UpdateBoneListCache(model);
    }

    DirectX::XMFLOAT4X4 finalWorld;
    bool calcSuccess = false;

    if (useSocketForSelf)
    {
        // ソケットを使って追従
        calcSuccess = GetSocketWorldTransform(currentAttachName, finalWorld);
    }
    else
    {
        // ボーン名直接指定
        int boneIdx = -1;
        int dummyCache = -1;
        boneIdx = ResolveBoneIndex(model, currentAttachName, dummyCache);

        if (boneIdx != -1) {
            finalWorld = CalcWorldMatrix(boneIdx, model, parent->GetTransform(),
                defaultOffsetPos, defaultOffsetRot, defaultOffsetScale);
            calcSuccess = true;
        }
    }

    // 適用
    if (calcSuccess)
    {
        XMMATRIX W = XMLoadFloat4x4(&finalWorld);
        XMVECTOR S, R, T;
        if (XMMatrixDecompose(&S, &R, &T, W))
        {
            XMFLOAT3 pos, scale;
            XMFLOAT4 rot;
            XMStoreFloat3(&pos, T);
            XMStoreFloat4(&rot, R);
            XMStoreFloat3(&scale, S);

            owner->SetPosition(pos);
            owner->SetRotation(rot);
            owner->SetScale(scale);
            owner->UpdateTransform();
        }
    }
}

// =========================================================
// ソケット管理
// =========================================================

void NodeAttachComponent::RegisterSocket(const std::string& socketName, const std::string& boneName,
    const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rotDeg,
    const DirectX::XMFLOAT3& scale)
{
    NodeSocket s;
    s.name = socketName;
    s.parentBoneName = boneName;
    s.offsetPos = pos;
    s.offsetRotDeg = rotDeg;
    s.offsetScale = scale;
    s.cachedBoneIndex = -1;

    sockets[socketName] = s;
}

void NodeAttachComponent::UnregisterSocket(const std::string& socketName)
{
    sockets.erase(socketName);
}

bool NodeAttachComponent::GetSocketWorldTransform(const std::string& socketName, DirectX::XMFLOAT4X4& outWorld)
{
    std::shared_ptr<Actor> parent = targetActor ? targetActor : GetActor();
    if (!parent) return false;

    const ::Model* model = parent->GetModelRaw();
    if (!model) return false;

    auto it = sockets.find(socketName);
    if (it == sockets.end()) return false;
    NodeSocket& s = it->second;

    int boneIdx = ResolveBoneIndex(model, s.parentBoneName, s.cachedBoneIndex);
    if (boneIdx == -1) return false;

    outWorld = CalcWorldMatrix(boneIdx, model, parent->GetTransform(),
        s.offsetPos, s.offsetRotDeg, s.offsetScale);
    return true;
}

bool NodeAttachComponent::GetBoneWorldTransform(const std::string& boneName, DirectX::XMFLOAT4X4& outWorld)
{
    std::shared_ptr<Actor> parent = targetActor ? targetActor : GetActor();
    if (!parent) return false;
    const ::Model* model = parent->GetModelRaw();
    if (!model) return false;

    int dummyCache = -1;
    int boneIdx = ResolveBoneIndex(model, boneName, dummyCache);

    if (boneIdx == -1) return false;

    outWorld = CalcWorldMatrix(boneIdx, model, parent->GetTransform(),
        { 0,0,0 }, { 0,0,0 }, { 1,1,1 });
    return true;
}

// =========================================================
// アタッチ制御
// =========================================================

void NodeAttachComponent::BindTargetActor(std::shared_ptr<Actor> actor)
{
    targetActor = actor;
    lastModelPtr = nullptr;
}

void NodeAttachComponent::AttachSelfToSocket(const std::string& socketName)
{
    isAttached = true;
    useSocketForSelf = true;
    currentAttachName = socketName;
}

void NodeAttachComponent::AttachSelfToBone(const std::string& boneName)
{
    isAttached = true;
    useSocketForSelf = false;
    currentAttachName = boneName;
}

void NodeAttachComponent::Detach()
{
    isAttached = false;
}

// =========================================================
// 内部ロジック
// =========================================================

int NodeAttachComponent::ResolveBoneIndex(const ::Model* model, const std::string& name, int& cacheIndex)
{
    if (!model) return -1;

    // キャッシュが有効なら即リターン
    // (モデルポインタが変わった時は呼び出し元でリセットされている前提)
    if (cacheIndex >= 0 && cacheIndex < (int)model->GetNodes().size()) {
        return cacheIndex;
    }

    const auto& nodes = model->GetNodes();
    for (int i = 0; i < (int)nodes.size(); ++i) {
        if (nodes[i].name == name) {
            cacheIndex = i;
            return i;
        }
    }
    return -1;
}

DirectX::XMFLOAT4X4 NodeAttachComponent::CalcWorldMatrix(int boneIndex, const ::Model* model,
    const DirectX::XMFLOAT4X4& actorWorld,
    const DirectX::XMFLOAT3& t,
    const DirectX::XMFLOAT3& r,
    const DirectX::XMFLOAT3& s) const
{
    XMMATRIX W_Actor = XMLoadFloat4x4(&actorWorld);
    XMMATRIX W_Bone = XMMatrixIdentity();

    if (model && boneIndex >= 0 && boneIndex < (int)model->GetNodes().size()) {
        W_Bone = XMLoadFloat4x4(&model->GetNodes()[boneIndex].worldTransform);
    }

    // オフセットSRT
    XMMATRIX M_Scale = XMMatrixScaling(s.x, s.y, s.z);
    XMMATRIX M_Rot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(r.x), XMConvertToRadians(r.y), XMConvertToRadians(r.z));
    XMMATRIX M_Trans = XMMatrixTranslation(t.x, t.y, t.z);
    XMMATRIX W_Offset = M_Scale * M_Rot * M_Trans;

    XMMATRIX Result;
    if (offsetSpace == OffsetSpace::NodeLocal) {
        Result = W_Offset * W_Bone * W_Actor;
    }
    else {
        // ModelLocal: ボーン位置には移動するが、ボーン回転は無視
        XMVECTOR boneScale, boneRot, bonePos;
        XMMatrixDecompose(&boneScale, &boneRot, &bonePos, W_Bone);
        XMMATRIX W_BonePosOnly = XMMatrixTranslationFromVector(bonePos);
        Result = W_Offset * W_BonePosOnly * W_Actor;
    }

    XMFLOAT4X4 out;
    XMStoreFloat4x4(&out, Result);
    return out;
}

void NodeAttachComponent::UpdateBoneListCache(const ::Model* model)
{
    guiBoneNameCache.clear();
    if (!model) return;
    const auto& nodes = model->GetNodes();
    for (const auto& node : nodes) {
        guiBoneNameCache.push_back(node.name);
    }
}

// 静的ユーティリティ
DirectX::XMFLOAT3 NodeAttachComponent::GetWorldPosition_NodeLocal(
    const ::Model* model, int nodeIndex, const DirectX::XMFLOAT3& offsetLocal)
{
    if (!model) return { 0,0,0 };
    const auto& nodes = model->GetNodes();
    if (nodeIndex < 0 || nodeIndex >= (int)nodes.size()) return { 0,0,0 };

    XMMATRIX W_Bone = XMLoadFloat4x4(&nodes[nodeIndex].worldTransform);
    XMMATRIX T = XMMatrixTranslation(offsetLocal.x, offsetLocal.y, offsetLocal.z);
    // Actor行列がないのでモデル空間座標になる点に注意
    XMMATRIX Result = T * W_Bone;

    XMFLOAT4X4 out;
    XMStoreFloat4x4(&out, Result);
    return { out._41, out._42, out._43 };
}

// =========================================================
// GUI (ここが最強のUIじゃ！)
// =========================================================

void NodeAttachComponent::OnGUI()
{
    if (!ImGui::CollapsingHeader("NodeAttach System", ImGuiTreeNodeFlags_DefaultOpen)) return;

    std::shared_ptr<Actor> parent = targetActor ? targetActor : GetActor();
    if (!parent) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No Target Actor!");
        return;
    }

    const ::Model* model = parent->GetModelRaw();
    if (!model) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Target has no Model!");
        return;
    }

    // モデルが変わっていたらボーンリスト更新
    if (model != lastModelPtr) {
        lastModelPtr = model;
        UpdateBoneListCache(model);
    }

    ImGui::Text("Target: %s", parent->GetName());
    ImGui::Separator();

    // --- 新規ソケット登録 ---
    ImGui::Text("Create New Socket");

    static char newSocketName[64] = "NewSocket";
    ImGui::InputText("Socket Name", newSocketName, 64);

    // ボーン選択（コンボボックス）
    if (!guiBoneNameCache.empty()) {
        if (guiSelectedBoneIndex >= (int)guiBoneNameCache.size()) guiSelectedBoneIndex = 0;
        if (ImGui::BeginCombo("Parent Bone", guiBoneNameCache[guiSelectedBoneIndex].c_str())) {
            for (int i = 0; i < (int)guiBoneNameCache.size(); i++) {
                bool is_selected = (guiSelectedBoneIndex == i);
                if (ImGui::Selectable(guiBoneNameCache[i].c_str(), is_selected)) {
                    guiSelectedBoneIndex = i;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    else {
        ImGui::TextDisabled("No bones found in model.");
    }

    // オフセット調整
    static float newPos[3] = { 0,0,0 };
    static float newRot[3] = { 0,0,0 };
    ImGui::DragFloat3("Offset Pos", newPos, 0.01f);
    ImGui::DragFloat3("Offset Rot", newRot, 1.0f);

    if (ImGui::Button("Register Socket")) {
        if (!guiBoneNameCache.empty()) {
            RegisterSocket(newSocketName, guiBoneNameCache[guiSelectedBoneIndex],
                { newPos[0], newPos[1], newPos[2] }, { newRot[0], newRot[1], newRot[2] });
        }
    }

    ImGui::Separator();

    // --- 登録済みソケット一覧と編集 ---
    ImGui::Text("Managed Sockets (%d)", (int)sockets.size());

    if (ImGui::BeginTable("SocketTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Bone");
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        // 削除用の一時リスト
        std::string toDelete = "";

        for (auto& kv : sockets) {
            NodeSocket& s = kv.second;
            ImGui::PushID(s.name.c_str());

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", s.name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", s.parentBoneName.c_str());

            ImGui::TableNextColumn();
            // 簡易編集 (位置のみ)
            ImGui::DragFloat3("##pos", &s.offsetPos.x, 0.01f);

            ImGui::TableNextColumn();
            if (ImGui::Button("Test Attach")) {
                AttachSelfToSocket(s.name);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                toDelete = s.name;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (!toDelete.empty()) {
            UnregisterSocket(toDelete);
        }
    }

    // --- 自身のステータス ---
    ImGui::Separator();
    ImGui::Text("Self Status:");
    if (isAttached) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Attached to: %s (%s)",
            currentAttachName.c_str(), useSocketForSelf ? "Socket" : "Bone");
        if (ImGui::Button("Detach")) Detach();
    }
    else {
        ImGui::TextDisabled("Not attached");
    }
}