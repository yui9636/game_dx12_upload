#include "CinematicSequencerComponent.h"
#include "Cinematic/CinematicTrack.h"
#include "Actor/Actor.h"
#include "Render/Graphics.h"
#include "Camera/Camera.h"
#include "System/Dialog.h"
#include"Model\/Model.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "ImGuizmo.h"

#include <algorithm>
#include <cmath>

using namespace Cinematic;
using namespace DirectX;
// CinematicSequencerComponent の実装。
// タイムライン再生、ImGui 編集 UI、選択キーの可視化を担当する。
// 初期状態のシーケンスを作成する。
CinematicSequencerComponent::CinematicSequencerComponent()
{
    sequence = std::make_shared<Sequence>();
    sequence->name = "New Sequence";
    sequence->duration = 10.0f;
}

// コンポーネント破棄時にドライバー接続と編集用ゴーストを片付ける。
CinematicSequencerComponent::~CinematicSequencerComponent()
{
    driver.Disconnect();

    if (editorGhost)
    {
        // 有効なカメラキー選択が無ければゴーストを消す。
        ActorManager::Instance().Remove(editorGhost);
        editorGhost = nullptr;
    }
}

// 再生中の時間更新、トラック評価、アニメーション上書き情報の更新を行う。
void CinematicSequencerComponent::Update(float dt)
{
    // 選択中キーに合わせて編集用カメラゴーストを更新する。
    UpdateGhostCamera();

    if (isPlaying && sequence)
    {
        if (!isPaused)
        {
            // 一時停止でなければ再生時間を進める。
            currentTime += dt;
        }

        if (currentTime >= sequence->duration)
        {
            currentTime = sequence->duration;
            // 終端に到達したら停止する。
            Stop();
        }

        // 現在時間で各トラックを評価する。
        sequence->Evaluate(currentTime);
// AnimationTrack の現在有効なキーを調べ、外部へ渡す上書きアニメーションを決める。
int overrideAnim = -1;
        float animLocalTime = 0.0f;

        for (auto& track : sequence->tracks)
        {
            if (track->GetType() == TrackType::Animation && !track->isMuted)
            {
                auto animTrack = static_cast<AnimationTrack*>(track.get());

                for (const auto& key : animTrack->keys)
                {
                    float startTime = key.time;
                    float endTime = key.time + key.duration;

                    if (currentTime >= startTime && currentTime < endTime)
                    {
                        overrideAnim = key.animIndex;

                        animLocalTime = currentTime - startTime;

                        break;
                    }
                }
            }
        }

        // アニメーション再生側へ渡す値をドライバーに保存する。
        driver.SetOverrideAnimation(overrideAnim);
        driver.SetLoop(true);

        if (overrideAnim != -1) {
            driver.SetTime(animLocalTime);
        }
        else {
            driver.SetTime(0.0f);
        }
// 旧カメラ再生処理。現行カメラシステム移行により無効化されている。
}
}

// 選択中の CameraTrack キー位置に編集用ゴースト Actor を配置する。
void CinematicSequencerComponent::UpdateGhostCamera()
{
    if (selection.IsValid() && selection.trackIndex < (int)sequence->tracks.size())
    {
        auto track = sequence->tracks[selection.trackIndex];
        if (track->GetType() == TrackType::Camera)
        {
            CameraTrack* camTrack = static_cast<CameraTrack*>(track.get());
            if (!camTrack->eyeCurve.keys.empty())
            {
                if (!editorGhost)
                {
                    // 初回のみゴースト Actor を作成してカメラモデルを読み込む。
                    editorGhost = ActorManager::Instance().Create();
                    editorGhost->SetName("GhostCamera");
                    editorGhost->LoadModel("Data/Model/Camera/Camera.fbx", 0.005f);
                    editorGhost->isDebugModel = true;
                }

                // 選択キーの位置をゴーストの位置に反映する。
                const auto& keyEye = camTrack->eyeCurve.keys[selection.keyIndex];
                editorGhost->SetPosition(keyEye.value);

                // 注視点から向きを計算し、ゴーストの回転へ変換する。
                XMFLOAT3 focusPos = camTrack->focusCurve.Evaluate(keyEye.time);
                XMVECTOR Eye = XMLoadFloat3(&keyEye.value);
                XMVECTOR Focus = XMLoadFloat3(&focusPos);
                XMVECTOR Up = XMVectorSet(0, 1, 0, 0);

                if (XMVector3Equal(Eye, Focus)) Focus = XMVectorAdd(Eye, XMVectorSet(0, 0, 1, 0));

                XMMATRIX View = XMMatrixLookAtLH(Eye, Focus, Up);
                XMMATRIX World = XMMatrixInverse(nullptr, View);

                XMVECTOR S, R, T;
                XMMatrixDecompose(&S, &R, &T, World);

                XMFLOAT4 rot; XMStoreFloat4(&rot, R);
                editorGhost->SetRotation(rot);
                return;
            }
        }
    }

    if (editorGhost)
    {
        // 有効なカメラキー選択が無ければゴーストを消す。
        ActorManager::Instance().Remove(editorGhost);
        editorGhost = nullptr;
    }
}

// シーケンスを先頭から再生開始する。
void CinematicSequencerComponent::Play()
{
    if (sequence)
    {
        isPlaying = true;
        isPaused = false;
        currentTime = 0.0f;
    }
}

// 再生を停止し、時間を 0 に戻す。
void CinematicSequencerComponent::Stop()
{
    if (isPlaying)
    {
        isPlaying = false;
        isPaused = false;
        currentTime = 0.0f;
    }
    UpdateGhostCamera();
}

// 再生中なら一時停止状態を切り替える。
void CinematicSequencerComponent::Pause()
{
    if (isPlaying) isPaused = !isPaused;
}

// タイムラインの現在時間を指定し、その時間の状態に更新する。
void CinematicSequencerComponent::SetTime(float time)
{
    float maxT = sequence->duration;
    currentTime = (time < 0.0f) ? 0.0f : (time > maxT ? maxT : time);
    if (sequence) sequence->Evaluate(currentTime);

    driver.SetTime(currentTime);

    UpdateGhostCamera();
}

// トラックの反映対象 Actor を設定し、各トラックへ Bind する。
void CinematicSequencerComponent::SetTargetActor(std::shared_ptr<Actor> actor)
{
    targetActor = actor;
    if (auto act = targetActor.lock())
    {
        // 旧 AnimatorComponent 連携は削除済み。
        // シーケンサー改修後に AnimationTrack 再生を再導入する。
        driver.Disconnect();

        if (sequence)
        {
            for (auto& track : sequence->tracks)
            {
                track->Bind(act.get());
            }
        }

    }
}

// 再生前状態保存のための予約関数。現在は未実装。
void CinematicSequencerComponent::CaptureInitialState() {}
// 保存した状態へ戻すための予約関数。現在は未実装。
void CinematicSequencerComponent::RestoreInitialState() {}

// シーケンサー編集用の ImGui ウィンドウを描画する。
void CinematicSequencerComponent::OnGUI()
{
    DrawGizmo();

    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cinematic Sequencer"))
    {
        // 現在のターゲット Actor 名を表示する。
        std::string targetName = "None";
        if (auto act = targetActor.lock()) targetName = act->GetName();

        ImGui::Text("Target: %s", targetName.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Link Selected")) {
            auto selected = ActorManager::Instance().GetSelectedActor();
            if (selected) SetTargetActor(selected);
        }
        ImGui::SameLine();
        if (ImGui::BeginCombo("##SelectActor", "Select from List")) {
            for (auto& actor : ActorManager::Instance().GetActors()) {
                bool isSelected = (targetActor.lock() == actor);
                if (ImGui::Selectable(actor->GetName().c_str(), isSelected)) {
                    SetTargetActor(actor);
                    ActorManager::Instance().SetSelectedActor(actor);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();

        // シーケンスを JSON として保存する。
        if (ImGui::Button("Save")) {
            char path[MAX_PATH] = "";
            if (Dialog::SaveFileName(path, MAX_PATH, "JSON Files\0*.json\0All Files\0*.*\0") == DialogResult::OK)
                sequence->SaveToFile(path);
        }
        ImGui::SameLine();
        // JSON からシーケンスを読み込み直す。
        if (ImGui::Button("Load")) {
            char path[MAX_PATH] = "";
            if (Dialog::OpenFileName(path, MAX_PATH, "JSON Files\0*.json\0All Files\0*.*\0") == DialogResult::OK) {
                sequence->LoadFromFile(path);
                currentTime = 0.0f; Stop(); selection.Clear();
            }
        }
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        // 再生制御ボタン。
        if (ImGui::Button("Play")) Play();
        ImGui::SameLine(); if (ImGui::Button("Stop")) Stop();
        ImGui::SameLine(); if (ImGui::Button(isPaused ? "Resume" : "Pause")) Pause();
        ImGui::SameLine(); ImGui::Text("Time: %.2f / %.2f", currentTime, sequence->duration);
        ImGui::SameLine(); ImGui::PushItemWidth(-1);
        // スライダー操作中は現在時間を直接評価する。
        if (ImGui::SliderFloat("##TimeSlider", &currentTime, 0.0f, sequence->duration)) {
            if (sequence) sequence->Evaluate(currentTime);
            driver.SetTime(currentTime);
            UpdateGhostCamera();
        }
        ImGui::PopItemWidth();
        ImGui::Separator();

        // 選択中トラックに応じてキー追加・キー編集 UI を切り替える。
        if (selection.trackIndex >= 0 && selection.trackIndex < (int)sequence->tracks.size())
        {
            auto track = sequence->tracks[selection.trackIndex];
            ImGui::Text("Selected Track: %s", track->name.c_str());
            ImGui::SameLine();

            if (track->GetType() == TrackType::Camera) {
                if (ImGui::Button("Add Camera Key")) {
                    // 現行カメラシステム移行中のため、実カメラからのキー取得は無効化中。
                }
            }
            else if (track->GetType() == TrackType::Animation) {
                ImGui::BeginDisabled();
                ImGui::Button("Add Animation Key");
                ImGui::EndDisabled();
                ImGui::TextDisabled("Animation track editing is temporarily disabled.");
            }
            else if (track->GetType() == TrackType::Effect)
            {
                EffectTrack* effTrack = static_cast<EffectTrack*>(track.get());

                if (ImGui::Button("Add Effect Key")) {
                    effTrack->AddKey(currentTime, "", 2.0f);
                    selection.keyIndex = (int)effTrack->keys.size() - 1;
                }

                ImGui::Separator();

                if (selection.keyIndex >= 0 && selection.keyIndex < (int)effTrack->keys.size())
                {
                    auto& key = effTrack->keys[selection.keyIndex];

                    // 選択中エフェクトキーの詳細を編集する。
                    ImGui::Text("Key Property:");
                    ImGui::DragFloat("Start Time", &key.time, 0.01f);
                    ImGui::DragFloat("Duration", &key.duration, 0.01f, 0.1f, 100.0f);

                    char buf[128]; strcpy_s(buf, key.effectName.c_str());
                    if (ImGui::InputText("Effect Path", buf, sizeof(buf))) key.effectName = buf;
                    ImGui::SameLine();
                    if (ImGui::Button("...##Eff")) {
                        // ファイルダイアログからエフェクト JSON を選ぶ。
                        char path[MAX_PATH] = "";
                        if (Dialog::OpenFileName(path, MAX_PATH, "JSON\0*.json\0", "Select Effect", nullptr) == DialogResult::OK) {
                            std::string fullPath = path;
                            size_t p = fullPath.find("Data\\");
                            if (p != std::string::npos) key.effectName = "Data/" + fullPath.substr(p + 5);
                            else key.effectName = fullPath;
                        }
                    }

                    if (auto actor = targetActor.lock())
                    {
                        if (auto model = actor->GetModelRaw())
                        {
                            // モデルのノード一覧から追従ボーンを選ぶ。
                            std::string currentBone = key.boneName.empty() ? "(Root)" : key.boneName;
                            if (ImGui::BeginCombo("Bone Name", currentBone.c_str()))
                            {
                                if (ImGui::Selectable("(Root) / None", key.boneName.empty())) {
                                    key.boneName = "";
                                }

                                const auto& nodes = model->GetNodes();
                                for (const auto& node : nodes)
                                {
                                    bool isSelected = (key.boneName == node.name);
                                    if (ImGui::Selectable(node.name.c_str(), isSelected)) {
                                        key.boneName = node.name;
                                    }
                                    if (isSelected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("No Model attached to Actor");
                            char boneBuf[128]; strcpy_s(boneBuf, key.boneName.c_str());
                            if (ImGui::InputText("Bone Name (Manual)", boneBuf, sizeof(boneBuf))) key.boneName = boneBuf;
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("No Target Actor");
                    }

                    ImGui::DragFloat3("Pos Offset", &key.offsetPos.x, 0.01f);
                    ImGui::DragFloat3("Rot Offset", &key.offsetRot.x, 1.0f);
                    ImGui::DragFloat3("Scale", &key.offsetScale.x, 0.01f);
                }
            }
        }
        else {
            ImGui::TextDisabled("Select a track to add keys.");
        }

        ImGui::Separator();
        ImGui::Columns(2, "SequencerCols");
        DrawTrackList();
        ImGui::NextColumn();
        DrawTimelineWindow();
        ImGui::Columns(1);
    }
    ImGui::End();
}

// 選択中カメラキーをギズモで編集するための処理。
void CinematicSequencerComponent::DrawGizmo()
{
    if (editorGhost && selection.IsValid())
    {
        // 現行カメラシステム移行中のため、実カメラの view/projection 取得は無効化中。

        auto track = sequence->tracks[selection.trackIndex];
        if (track->GetType() != TrackType::Camera) return;
        CameraTrack* camTrack = static_cast<CameraTrack*>(track.get());

        // ImGuizmo の描画範囲を画面全体に設定する。
        ImGuizmo::Enable(true);
        ImGuizmo::SetRect(0, 0, (float)Graphics::Instance().GetScreenWidth(), (float)Graphics::Instance().GetScreenHeight());
        XMFLOAT4X4 worldMatrix;

        // ゴースト Actor の位置・回転から編集用ワールド行列を作る。
        XMMATRIX T = XMMatrixTranslationFromVector(XMLoadFloat3(&editorGhost->GetPosition()));
        XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&editorGhost->GetRotation()));
        XMStoreFloat4x4(&worldMatrix, R * T);
    }
}

// トラック一覧とトラック追加メニューを描画する。
void CinematicSequencerComponent::DrawTrackList()
{
    ImGui::BeginChild("TrackList", ImVec2(0, 0), false);
    // トラック追加ポップアップを開く。
    if (ImGui::Button("+ Add Track")) ImGui::OpenPopup("AddTrackPopup");
    if (ImGui::BeginPopup("AddTrackPopup"))
    {
        // 追加直後のトラックを現在のターゲット Actor へ接続する。
        auto AddAndBind = [&](auto track) {
            if (auto act = targetActor.lock()) track->Bind(act.get());
            };

        if (ImGui::MenuItem("Camera Track")) {
            auto t = sequence->AddTrack<CameraTrack>("Camera Track");
            AddAndBind(t);
        }
        if (ImGui::MenuItem("Animation Track")) {
            auto t = sequence->AddTrack<AnimationTrack>("Actor Animation");
            AddAndBind(t);
        }
        if (ImGui::MenuItem("Effect Track")) {
            auto t = sequence->AddTrack<EffectTrack>("Effect Track");
            AddAndBind(t);
        }

        ImGui::EndPopup();
    }
    ImGui::Separator();

    // 既存トラックを一覧表示し、クリックで選択する。
    for (int i = 0; i < (int)sequence->tracks.size(); ++i)
    {
        auto& track = sequence->tracks[i];
        ImGui::PushID(i);
        bool isSelected = (selection.trackIndex == i);
        if (ImGui::Selectable(track->name.c_str(), isSelected))
        {
            selection.trackIndex = i;
            selection.keyIndex = -1;
            selection.isDragging = false;
            UpdateGhostCamera();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// タイムライン本体を描画し、キーの選択・ドラッグ操作を処理する。
void CinematicSequencerComponent::DrawTimelineWindow()
{
    ImGui::BeginChild("TimelineView", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float scale = 100.0f;
    float rowHeight = ImGui::GetTextLineHeightWithSpacing();

    // 1秒ごとの縦線と時刻ラベルを描く。
    for (float t = 0; t <= sequence->duration + 1; t += 1.0f) {
        float x = p.x + t * scale;
        drawList->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + 1000), 0x40FFFFFF);
        char buf[8]; sprintf_s(buf, "%.0fs", t);
        drawList->AddText(ImVec2(x + 2, p.y), 0xFFAAAAAA, buf);
    }
    // 現在時間を赤い再生ヘッドとして表示する。
    float cx = p.x + currentTime * scale;
    drawList->AddLine(ImVec2(cx, p.y), ImVec2(cx, p.y + 1000), 0xFFFF0000, 2.0f);

    // 空白部分をクリックしたら、その位置へ再生時間を移動する。
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
        float t = (ImGui::GetMousePos().x - p.x) / scale;
        SetTime(t);
        selection.Clear();
        UpdateGhostCamera();
    }

    // 既存トラックを一覧表示し、クリックで選択する。
    for (int i = 0; i < (int)sequence->tracks.size(); ++i) {
        float y = p.y + i * rowHeight + 25.0f;
        auto track = sequence->tracks[i];
// Camera Track のキーをダイヤ型で表示する。
if (track->GetType() == TrackType::Camera) {
            CameraTrack* camTrack = static_cast<CameraTrack*>(track.get());
            auto& keys = camTrack->eyeCurve.keys;
            for (int k = 0; k < (int)keys.size(); ++k) {
                float x = p.x + keys[k].time * scale;
                ImU32 col = selection.IsSelected(i, k) ? 0xFFFF0000 : 0xFF00FF00;

                ImGui::SetCursorScreenPos(ImVec2(x - 6, y - 6));
                ImGui::PushID(i * 1000 + k);

                // 透明ボタンでキーのクリック判定だけを受ける。
                if (ImGui::InvisibleButton("##Key", ImVec2(12, 12))) {
                    selection.trackIndex = i;
                    selection.keyIndex = k;
                    UpdateGhostCamera();
                }
                // ドラッグ量を時間へ変換してキー位置を移動する。
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    keys[k].time += ImGui::GetIO().MouseDelta.x / scale;
                    if (keys[k].time < 0.0f) keys[k].time = 0.0f;
                    if (k < (int)camTrack->focusCurve.keys.size()) camTrack->focusCurve.keys[k].time = keys[k].time;
                    SetTime(keys[k].time);
                }
                ImGui::PopID();

                drawList->AddQuadFilled(ImVec2(x, y - 5), ImVec2(x + 5, y), ImVec2(x, y + 5), ImVec2(x - 5, y), col);
                drawList->AddQuad(ImVec2(x, y - 5), ImVec2(x + 5, y), ImVec2(x, y + 5), ImVec2(x - 5, y), 0xFF000000);
            }
        }
// AnimationTrack のキーを横長バーとして表示する。
else if (track->GetType() == TrackType::Animation)
        {
            AnimationTrack* animTrack = static_cast<AnimationTrack*>(track.get());
            auto& keys = animTrack->keys;

            for (int k = 0; k < (int)keys.size(); ++k) {
                float startX = p.x + keys[k].time * scale;
                float endX = p.x + (keys[k].time + keys[k].duration) * scale;
                float width = endX - startX;
                if (width < 5.0f) width = 5.0f;

                float barY = y - 9.0f;
                float barH = 18.0f;

                ImVec2 rectMin(startX, barY);
                ImVec2 rectMax(startX + width, barY + barH);

                ImGui::SetCursorScreenPos(rectMin);
                ImGui::PushID(i * 1000 + k);

                // バー全体をクリック・ドラッグ対象にする。
                if (ImGui::InvisibleButton("##Bar", ImVec2(width, barH))) {
                    selection.trackIndex = i;
                    selection.keyIndex = k;
                }
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    keys[k].time += ImGui::GetIO().MouseDelta.x / scale;
                    if (keys[k].time < 0.0f) keys[k].time = 0.0f;
                    SetTime(keys[k].time);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s (%.2fs)", keys[k].animName.c_str(), keys[k].duration);
                }

                ImGui::PopID();

                ImU32 col = selection.IsSelected(i, k) ? 0xFF55AAFF : 0xFF3366AA;
                drawList->AddRectFilled(rectMin, rectMax, col, 4.0f);
                drawList->AddRect(rectMin, rectMax, 0xFF000000, 4.0f);

                drawList->PushClipRect(rectMin, rectMax, true);
                drawList->AddText(ImVec2(rectMin.x + 4, rectMin.y + 2), 0xFFFFFFFF, keys[k].animName.c_str());
                drawList->PopClipRect();
            }
        }
// EffectTrack のキーを横長バーとして表示する。
else if (track->GetType() == TrackType::Effect)
        {
            EffectTrack* effTrack = static_cast<EffectTrack*>(track.get());
            auto& keys = effTrack->keys;

            for (int k = 0; k < (int)keys.size(); ++k) {
                float startX = p.x + keys[k].time * scale;
                float endX = p.x + (keys[k].time + keys[k].duration) * scale;
                float width = endX - startX;
                if (width < 5.0f) width = 5.0f;

                float barY = y - 9.0f;
                float barH = 18.0f;

                ImVec2 rectMin(startX, barY);
                ImVec2 rectMax(startX + width, barY + barH);

                ImGui::SetCursorScreenPos(rectMin);
                ImGui::PushID(i * 1000 + k);

                // エフェクトバー全体をクリック・ドラッグ対象にする。
                if (ImGui::InvisibleButton("##EffBar", ImVec2(width, barH))) {
                    selection.trackIndex = i;
                    selection.keyIndex = k;
                }
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    keys[k].time += ImGui::GetIO().MouseDelta.x / scale;
                    if (keys[k].time < 0.0f) keys[k].time = 0.0f;
                    SetTime(keys[k].time);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s (%.2fs)", keys[k].effectName.c_str(), keys[k].duration);
                }

                ImGui::PopID();

                ImU32 col = selection.IsSelected(i, k) ? 0xFFFFAAFF : 0xFFAA55AA;
                drawList->AddRectFilled(rectMin, rectMax, col, 4.0f);
                drawList->AddRect(rectMin, rectMax, 0xFF000000, 4.0f);

                // 表示名はパスの末尾だけにしてタイムラインを見やすくする。
                std::string label = keys[k].effectName;
                size_t slash = label.find_last_of("/\\");
                if (slash != std::string::npos) label = label.substr(slash + 1);

                drawList->PushClipRect(rectMin, rectMax, true);
                drawList->AddText(ImVec2(rectMin.x + 4, rectMin.y + 2), 0xFFFFFFFF, label.c_str());
                drawList->PopClipRect();
            }
        }
    }
    ImGui::EndChild();
}
