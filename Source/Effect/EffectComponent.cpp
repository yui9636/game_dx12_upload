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
    // 配列として保存するための準備
    std::vector<json> effectsJsonArray;

    for (const auto& ae : activeEffects)
    {
        // パスが空のものは保存しない
        if (ae.effectPath.empty()) continue;

        json effectJson;
        effectJson["EffectPath"] = ae.effectPath;
        effectJson["Loop"] = ae.loop;
        effectJson["BoneName"] = ae.targetBoneName;

        // トランスフォーム (配列として保存)
        effectJson["Offset"] = { ae.localOffsetPosition.x, ae.localOffsetPosition.y, ae.localOffsetPosition.z };
        effectJson["Rotation"] = { ae.localOffsetRotation.x, ae.localOffsetRotation.y, ae.localOffsetRotation.z };
        effectJson["Scale"] = { ae.localOffsetScale.x, ae.localOffsetScale.y, ae.localOffsetScale.z };

        effectsJsonArray.push_back(effectJson);
    }

    // "Effects" というキーで配列を保存
    if (!effectsJsonArray.empty())
    {
        outJson["Effects"] = effectsJsonArray;
    }
}

// -----------------------------------------------------------------------------
// Deserialize: JSONからエフェクト設定を読み込み
// ※ここではデータ復元のみ行い、実際の再生は Start() で行うのが一般的ですが、
//   エディタでの即時反映のため Play も行います。
// -----------------------------------------------------------------------------
void EffectComponent::Deserialize(const json& inJson)
{
    // 既存のエフェクトをクリア
    StopAll();
    activeEffects.clear();

    if (inJson.contains("Effects") && inJson["Effects"].is_array())
    {
        for (const auto& effectJson : inJson["Effects"])
        {
            ActiveEffect ae;

            // パス読み込み
            if (effectJson.contains("EffectPath")) ae.effectPath = effectJson["EffectPath"];
            if (effectJson.contains("Loop")) ae.loop = effectJson["Loop"];
            if (effectJson.contains("BoneName")) ae.targetBoneName = effectJson["BoneName"];

            // トランスフォーム読み込み (ヘルパー関数あるいは手動展開)
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

            // リストに追加 (Start() で再生されるように保持)
            activeEffects.push_back(ae);
        }
    }
}




static XMMATRIX CalcCombinedMatrix(
    const XMFLOAT4X4& actorWorld,
    const XMFLOAT4X4& boneWorld,
    const XMFLOAT3& offsetPos,
    const XMFLOAT3& offsetRotDeg,
    const XMFLOAT3& offsetScale) // ★追加
{
    XMMATRIX W_Actor = XMLoadFloat4x4(&actorWorld);
    XMMATRIX W_Bone = XMLoadFloat4x4(&boneWorld);

    // オフセット行列 (SRT: Scale -> Rotate -> Translate)
    XMMATRIX M_Scale = XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z); // ★追加
    XMMATRIX M_Rot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(offsetRotDeg.x),
        XMConvertToRadians(offsetRotDeg.y),
        XMConvertToRadians(offsetRotDeg.z)
    );
    XMMATRIX M_Trans = XMMatrixTranslation(offsetPos.x, offsetPos.y, offsetPos.z);

    // 合成: Scale * Rot * Trans
    XMMATRIX W_Offset = M_Scale * M_Rot * M_Trans;

    // 最終合成: Offset * Bone * Actor
    return W_Offset * W_Bone * W_Actor;
}

void EffectComponent::Update(float dt)
{
    auto actor = GetActor();
    if (!actor) return;

    // アクターの行列取得
    XMMATRIX W_Actor = XMLoadFloat4x4(&actor->GetTransform());

    Model* model = actor->GetModelRaw();
    const auto& nodes = model ? model->GetNodes() : std::vector<Model::Node>();

    //// 1. 寿命切れ削除
    //auto it = std::remove_if(activeEffects.begin(), activeEffects.end(),
    //    [](const ActiveEffect& ae) { return ae.instance.expired(); });
    //activeEffects.erase(it, activeEffects.end());

    // 2. 更新処理
    for (auto& ae : activeEffects)
    {
        auto instance = ae.instance.lock();
        if (!instance) continue;

        instance->loop = ae.loop;

        // --- A. 親行列 (ボーン * アクター) の計算 ---
        XMMATRIX W_Socket = XMMatrixIdentity();

        if (!ae.targetBoneName.empty() && model)
        {
            // ボーン検索
            if (ae.targetBoneIndex == -1) {
                for (size_t i = 0; i < nodes.size(); ++i) {
                    if (nodes[i].name == ae.targetBoneName) {
                        ae.targetBoneIndex = (int)i;
                        break;
                    }
                }
            }

            // ボーン行列があれば適用
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
            // ボーンなし＝アクター直下
            W_Socket = W_Actor;
        }

        // --- B. 親行列の正規化 (TimelineSequencerと同じ処理) ---
        // 親のスケールが入っているとエフェクトが歪むため、基底ベクトルを正規化してスケールを (1,1,1) に戻す
        {
            XMFLOAT4X4 m; XMStoreFloat4x4(&m, W_Socket);

            XMVECTOR ax = XMVector3Normalize(XMVectorSet(m._11, m._12, m._13, 0));
            XMVECTOR ay = XMVector3Normalize(XMVectorSet(m._21, m._22, m._23, 0));
            XMVECTOR az = XMVector3Normalize(XMVectorSet(m._31, m._32, m._33, 0));
            XMVECTOR p = XMVectorSet(m._41, m._42, m._43, 1);

            // 正規化済み行列
            W_Socket = XMMatrixIdentity();
            W_Socket.r[0] = ax;
            W_Socket.r[1] = ay;
            W_Socket.r[2] = az;
            W_Socket.r[3] = p;
        }

        // --- C. 親行列の適用 ---
        // ここにはスケールを含めない！
        XMStoreFloat4x4(&instance->parentMatrix, W_Socket);

        // --- D. ローカル変形 (Offset & Scale) の適用 ---
        // ここで初めて「エフェクト独自のスケール」を適用する
        // これにより、親のスケールは無視され、シーケンサーで設定したスケールだけが効く
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

// Play関数の実装修正
std::weak_ptr<EffectInstance> EffectComponent::Play(
    const std::string& effectName,
    const std::string& boneName,
    const DirectX::XMFLOAT3& offset,
    const DirectX::XMFLOAT3& rotation,
    const DirectX::XMFLOAT3& scale, // ★追加
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
        ae.effectPath = effectName; // ★パスを保存
        ae.loop = loop;             // ★ループ設定を保存
        ae.targetBoneName = boneName;
        ae.localOffsetPosition = offset;
        ae.localOffsetRotation = rotation;
        ae.localOffsetScale = scale; // ★保存
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
    // ハンドルが有効かチェック
    auto targetPtr = handle.lock();
    if (!targetPtr) return;

    // 管理リストから該当するエフェクトを探して、オフセット値を更新
    for (auto& ae : activeEffects)
    {
        // lockしてポインタ比較
        auto aePtr = ae.instance.lock();
        if (aePtr == targetPtr)
        {
            ae.localOffsetPosition = offset;
            ae.localOffsetRotation = rotation;
            ae.localOffsetScale = scale; // ★ここでスケールも更新される
            return;
        }
    }
}

void EffectComponent::Stop(const std::weak_ptr<EffectInstance>& handle) { if (auto ptr = handle.lock()) ptr->Stop(); }
void EffectComponent::StopAll() { for (auto& ae : activeEffects) if (auto ptr = ae.instance.lock()) ptr->Stop(); activeEffects.clear(); }

void EffectComponent::OnGUI()
{
    // ヘッダー表示
    if (!ImGui::CollapsingHeader("Effect Component", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // リストが空なら、デフォルトで1つ空のスロットを追加しておく
    if (activeEffects.empty()) {
        ActiveEffect ae;
        ae.loop = true;
        // Scaleの初期値は {1,1,1} (構造体定義で初期化されている前提)
        ae.localOffsetScale = { 1.0f, 1.0f, 1.0f };
        activeEffects.push_back(ae);
    }

    // リスト描画
    for (int i = 0; i < (int)activeEffects.size(); ++i)
    {
        auto& ae = activeEffects[i];
        auto instance = ae.instance.lock();

        ImGui::PushID(i);

        // ファイル名をヘッダー名として使用
        std::string displayName = ae.effectPath;
        if (!displayName.empty()) {
            std::filesystem::path p(displayName);
            displayName = p.filename().string();
        }
        else {
            displayName = "No File";
        }

        bool open = ImGui::TreeNode(displayName.c_str());

        // 右クリックメニュー (削除や新規追加)
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
            // --- 1. ファイルパス入力欄と参照ボタン ---
            char pathBuf[MAX_PATH];
            strcpy_s(pathBuf, ae.effectPath.c_str());
            if (ImGui::InputText("File", pathBuf, sizeof(pathBuf))) {
                ae.effectPath = pathBuf;
            }
            ImGui::SameLine();

            // ★修正: ファイル選択時、即座にエフェクトを再生成して反映させる
            if (ImGui::Button("...")) {
                char path[MAX_PATH] = "";
                if (Dialog::OpenFileName(path, MAX_PATH, "Effect JSON\0*.json\0All Files\0*.*\0") == DialogResult::OK) {
                    // パスの相対化処理
                    std::string fullPath = path;
                    size_t p = fullPath.find("Data\\");
                    if (p != std::string::npos) ae.effectPath = fullPath.substr(p);
                    else ae.effectPath = fullPath;
                    std::replace(ae.effectPath.begin(), ae.effectPath.end(), '\\', '/');

                    // --- 自動再生処理 ---
                    // 1. 古いものを停止
                    if (auto old = ae.instance.lock()) old->Stop(true);

                    // 2. 新しいパスで即再生
                    auto actor = GetActor();
                    if (actor) {
                        auto newInst = EffectManager::Get().Play(ae.effectPath, actor->GetPosition());
                        if (newInst) {
                            newInst->loop = ae.loop;
                            // トランスフォーム初期適用（1フレームのズレ防止）
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

            // --- 2. トランスフォーム設定 ---
            // ※GUI操作はUpdate関数経由で次フレームに反映されますが、
            //   即座に見たい場合はここで instance->overrideLocalTransform を更新してもOKです
            ImGui::DragFloat3("Offset", &ae.localOffsetPosition.x, 0.01f);
            ImGui::DragFloat3("Rotation", &ae.localOffsetRotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &ae.localOffsetScale.x, 0.01f, 0.001f, 100.0f);

            // Loop設定 (変更時にインスタンスへ即反映)
            if (ImGui::Checkbox("Loop", &ae.loop)) {
                if (instance) instance->loop = ae.loop;
            }

            // --- 3. プレビュー制御 ---
            ImGui::Separator();
            float width = ImGui::GetContentRegionAvail().x;

            // Play / Respawn ボタン
            if (ImGui::Button("Play", ImVec2(width * 0.5f, 0)))
            {
                // 既存があれば停止
                if (auto old = ae.instance.lock()) old->Stop(true);

                // 再生成
                auto actor = GetActor();
                if (actor && !ae.effectPath.empty()) {
                    auto newInst = EffectManager::Get().Play(ae.effectPath, actor->GetPosition());
                    if (newInst) {
                        newInst->loop = ae.loop;

                        // 再生時もトランスフォームを即適用
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

            // Stop ボタン
            if (ImGui::Button("Stop", ImVec2(width * 0.5f, 0)))
            {
                if (auto inst = ae.instance.lock()) inst->Stop(true);
            }

            // ステータス表示
            if (instance) ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Playing");
            else ImGui::TextDisabled("Status: Stopped");

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

