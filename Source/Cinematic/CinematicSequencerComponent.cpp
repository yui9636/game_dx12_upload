#include "CinematicSequencerComponent.h"
#include "Cinematic/CinematicTrack.h"
#include "Actor/Actor.h"
#include "Graphics.h"
#include "Camera/Camera.h"
#include "Camera/CameraController.h"
#include "System/Dialog.h"
#include"Model\/Model.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "ImGuizmo.h"

#include <algorithm>
#include <cmath>

using namespace Cinematic;
using namespace DirectX;

// ==================================================================================
// �R���X�g���N�^ / �f�X�g���N�^
// ==================================================================================

CinematicSequencerComponent::CinematicSequencerComponent()
{
    sequence = std::make_shared<Sequence>();
    sequence->name = "New Sequence";
    sequence->duration = 10.0f;
}

CinematicSequencerComponent::~CinematicSequencerComponent()
{
    driver.Disconnect();

    if (editorGhost)
    {
        ActorManager::Instance().Remove(editorGhost);
        editorGhost = nullptr;
    }
}

void CinematicSequencerComponent::Update(float dt)
{
    UpdateGhostCamera();

    if (isPlaying && sequence)
    {
        if (!isPaused)
        {
            currentTime += dt;
        }

        // �V�[�P���X�I�����̏���
        if (currentTime >= sequence->duration)
        {
            currentTime = sequence->duration;
            Stop();
        }

        // �J�[�u�]���i�J�������͂����OK�j
        sequence->Evaluate(currentTime);

        // =========================================================================
        // ���������C���ӏ��F�A�j���[�V�����̔��胍�W�b�N�������ɂ���
        // =========================================================================
        int overrideAnim = -1;      // �f�t�H���g�́u�Ȃ�(-1)�v
        float animLocalTime = 0.0f; // �A�j���[�V�����̍Đ��ʒu

        for (auto& track : sequence->tracks)
        {
            if (track->GetType() == TrackType::Animation && !track->isMuted)
            {
                // AnimationTrack�̃L�[���𒼐ڌ���
                auto animTrack = static_cast<AnimationTrack*>(track.get());

                // ���ׂẴL�[�i�o�[�j�𑖍����āA���ݎ��Ԃ��u�o�[�̒��v�ɂ��邩�m�F
                for (const auto& key : animTrack->keys)
                {
                    float startTime = key.time;
                    float endTime = key.time + key.duration; // �o�[�̏I��莞��

                    // ���d�v����: �J�[�\�����o�[�͈͓̔��ɂ��邩�H
                    if (currentTime >= startTime && currentTime < endTime)
                    {
                        overrideAnim = key.animIndex; // ���̃A�j���[�V�������Đ�

                        // �����łɏC��: 
                        // �A�j���[�V�����̊J�n���Ԃ� 0�b�n�_ �ɍ��킹��i���Ύ��ԁj
                        // ��������Ȃ��ƁA5�b�ڂɒu�����A�j����5�b�ڂ���Đ��J�n����Ă��܂��܂�
                        animLocalTime = currentTime - startTime;

                        break; // �d�Ȃ�o�[������������I���i�㏟���ɂ���Ȃ�t���T���j
                    }
                }
            }
        }

        // �h���C�o�[�ɓK�p
        driver.SetOverrideAnimation(overrideAnim); // �͈͊O�Ȃ� -1 ������̂Ŏ~�܂�
        driver.SetLoop(true); // �o�[�͈͓̔��ł̓��[�v�����Ă����i�o�[�̒����Ő؂��̂�OK�j

        // �����͈͊O(-1)�Ȃ�A���Ԃ͉��ł��������A�͈͓��Ȃ�u���Ύ��ԁv��n���̂�����
        if (overrideAnim != -1) {
            driver.SetTime(animLocalTime);
        }
        else {
            driver.SetTime(0.0f);
        }

        // =========================================================================
        // �J�����X�V�����i�����͕ύX�Ȃ��j
        // =========================================================================
     /*   if (Camera* renderCam = Graphics::Instance().GetCamera())
        {
            for (auto& track : sequence->tracks)
            {
                if (track->GetType() == TrackType::Camera)
                {
                    CameraTrack* camTrack = static_cast<CameraTrack*>(track.get());
                    if (!camTrack->isMuted)
                    {
                        XMFLOAT3 eye = camTrack->eyeCurve.Evaluate(currentTime);
                        XMFLOAT3 focus = camTrack->focusCurve.Evaluate(currentTime);

                        XMVECTOR vEye = XMLoadFloat3(&eye);
                        XMVECTOR vFocus = XMLoadFloat3(&focus);
                        XMVECTOR vDiff = XMVectorSubtract(vFocus, vEye);
                        float lenSq = XMVectorGetX(XMVector3LengthSq(vDiff));

                        if (lenSq < 0.0001f)
                        {
                            vFocus = XMVectorAdd(vEye, XMVectorSet(0, 0, 1, 0));
                            XMStoreFloat3(&focus, vFocus);
                        }

                        renderCam->SetLookAt(eye, focus, { 0, 1, 0 });
                    }
                }
            }
        }*/
    }
}

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
                // �S�[�X�g����
                if (!editorGhost)
                {
                    editorGhost = ActorManager::Instance().Create();
                    editorGhost->SetName("GhostCamera");
                    editorGhost->LoadModel("Data/Model/Camera/Camera.fbx", 0.005f);
                    editorGhost->isDebugModel = true;
                }

                // �ʒu�E��]����
                const auto& keyEye = camTrack->eyeCurve.keys[selection.keyIndex];
                editorGhost->SetPosition(keyEye.value);

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

    // ��\��
    if (editorGhost)
    {
        ActorManager::Instance().Remove(editorGhost);
        editorGhost = nullptr;
    }
}

void CinematicSequencerComponent::Play()
{
    if (sequence)
    {
        isPlaying = true;
        isPaused = false;
        currentTime = 0.0f;
    }
}

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

void CinematicSequencerComponent::Pause()
{
    if (isPlaying) isPaused = !isPaused;
}

void CinematicSequencerComponent::SetTime(float time)
{
    float maxT = sequence->duration;
    currentTime = (time < 0.0f) ? 0.0f : (time > maxT ? maxT : time);
    if (sequence) sequence->Evaluate(currentTime);

    driver.SetTime(currentTime);

    UpdateGhostCamera();
}

void CinematicSequencerComponent::SetTargetActor(std::shared_ptr<Actor> actor)
{
    targetActor = actor;
    if (auto act = targetActor.lock())
    {
        // �A�N�^�[��Animator�Ƀh���C�o�[��ڑ�(�߈�)
        if (auto animator = act->GetComponent<AnimatorComponent>())
        {
            driver.Connect(animator.get());
        }

        if (sequence)
        {
            for (auto& track : sequence->tracks)
            {
                track->Bind(act.get());
            }
        }

    }
}

void CinematicSequencerComponent::CaptureInitialState() {}
void CinematicSequencerComponent::RestoreInitialState() {}

void CinematicSequencerComponent::OnGUI()
{
    DrawGizmo();

    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cinematic Sequencer"))
    {
        // 1. �^�[�Q�b�g�I��
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

        // 2. �c�[���o�[
        if (ImGui::Button("Save")) {
            char path[MAX_PATH] = "";
            if (Dialog::SaveFileName(path, MAX_PATH, "JSON Files\0*.json\0All Files\0*.*\0") == DialogResult::OK)
                sequence->SaveToFile(path);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            char path[MAX_PATH] = "";
            if (Dialog::OpenFileName(path, MAX_PATH, "JSON Files\0*.json\0All Files\0*.*\0") == DialogResult::OK) {
                sequence->LoadFromFile(path);
                currentTime = 0.0f; Stop(); selection.Clear();
            }
        }
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        if (ImGui::Button("Play")) Play();
        ImGui::SameLine(); if (ImGui::Button("Stop")) Stop();
        ImGui::SameLine(); if (ImGui::Button(isPaused ? "Resume" : "Pause")) Pause();
        ImGui::SameLine(); ImGui::Text("Time: %.2f / %.2f", currentTime, sequence->duration);
        ImGui::SameLine(); ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##TimeSlider", &currentTime, 0.0f, sequence->duration)) {
            if (sequence) sequence->Evaluate(currentTime);
            driver.SetTime(currentTime);
            UpdateGhostCamera();
        }
        ImGui::PopItemWidth();
        ImGui::Separator();

        // 3. �C���X�y�N�^�[
        if (selection.trackIndex >= 0 && selection.trackIndex < (int)sequence->tracks.size())
        {
            auto track = sequence->tracks[selection.trackIndex];
            ImGui::Text("Selected Track: %s", track->name.c_str());
            ImGui::SameLine();

            // --- A. �J���� ---
            if (track->GetType() == TrackType::Camera) {
                if (ImGui::Button("Add Camera Key")) {
                    //Camera* cam = Graphics::Instance().GetCamera();
                   /* if (cam) {
                        CameraTrack* camTrack = static_cast<CameraTrack*>(track.get());
                        camTrack->eyeCurve.AddKey(currentTime, cam->GetEye());
                        camTrack->focusCurve.AddKey(currentTime, cam->GetFocus());
                        selection.keyIndex = (int)camTrack->eyeCurve.keys.size() - 1;
                        UpdateGhostCamera();
                    }*/
                }
            }
            // --- B. �A�j���[�V���� ---
            else if (track->GetType() == TrackType::Animation) {
                if (ImGui::Button("Add Animation Key")) ImGui::OpenPopup("SelectAnimPopup");
                if (ImGui::BeginPopup("SelectAnimPopup")) {
                    std::vector<std::string> animNames;
                    if (auto actor = targetActor.lock()) {
                        if (auto animator = actor->GetComponent<AnimatorComponent>()) animNames = animator->GetAnimationNameList();
                    }
                    if (animNames.empty()) { ImGui::TextDisabled("No Animator found"); }
                    else {
                        if (ImGui::Selectable("Stop / None")) {
                            AnimationTrack* animTrack = static_cast<AnimationTrack*>(track.get());
                            animTrack->AddKey(currentTime, -1, "None", 1.0f);
                            driver.SetOverrideAnimation(-1);
                        }
                        for (const auto& name : animNames) {
                            if (ImGui::Selectable(name.c_str())) {
                                if (auto actor = targetActor.lock()) {
                                    float duration = 2.0f;
                                    if (auto model = actor->GetModelRaw()) {
                                        int animModelIdx = model->GetAnimationIndex(name.c_str());
                                        if (animModelIdx >= 0) duration = model->GetAnimations()[animModelIdx].secondsLength;
                                    }
                                    auto animator = actor->GetComponent<AnimatorComponent>();
                                    int idx = animator->GetAnimationIndexByName(name);
                                    AnimationTrack* animTrack = static_cast<AnimationTrack*>(track.get());
                                    animTrack->AddKey(currentTime, idx, name, duration);
                                    driver.SetOverrideAnimation(idx);
                                }
                            }
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            // --- C. �G�t�F�N�g (�ڍאݒ����) ---
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

                    ImGui::Text("Key Property:");
                    ImGui::DragFloat("Start Time", &key.time, 0.01f);
                    ImGui::DragFloat("Duration", &key.duration, 0.01f, 0.1f, 100.0f);

                    // �p�X����
                    char buf[128]; strcpy_s(buf, key.effectName.c_str());
                    if (ImGui::InputText("Effect Path", buf, sizeof(buf))) key.effectName = buf;
                    ImGui::SameLine();
                    if (ImGui::Button("...##Eff")) {
                        char path[MAX_PATH] = "";
                        if (Dialog::OpenFileName(path, MAX_PATH, "JSON\0*.json\0", "Select Effect", nullptr) == DialogResult::OK) {
                            std::string fullPath = path;
                            size_t p = fullPath.find("Data\\");
                            if (p != std::string::npos) key.effectName = "Data/" + fullPath.substr(p + 5);
                            else key.effectName = fullPath;
                        }
                    }

                    // ���d�v: �{�[�����I�� (�R���{�{�b�N�X)
                    if (auto actor = targetActor.lock())
                    {
                        if (auto model = actor->GetModelRaw())
                        {
                            std::string currentBone = key.boneName.empty() ? "(Root)" : key.boneName;
                            if (ImGui::BeginCombo("Bone Name", currentBone.c_str()))
                            {
                                // Root�I����
                                if (ImGui::Selectable("(Root) / None", key.boneName.empty())) {
                                    key.boneName = "";
                                }

                                // ���f���̃m�[�h�ꗗ��\��
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

                    // �I�t�Z�b�g
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

void CinematicSequencerComponent::DrawGizmo()
{
    if (editorGhost && selection.IsValid())
    {
        //Camera* camera = Graphics::Instance().GetCamera();
        //if (!camera) return;

        auto track = sequence->tracks[selection.trackIndex];
        if (track->GetType() != TrackType::Camera) return;
        CameraTrack* camTrack = static_cast<CameraTrack*>(track.get());

        ImGuizmo::Enable(true);
        ImGuizmo::SetRect(0, 0, (float)Graphics::Instance().GetScreenWidth(), (float)Graphics::Instance().GetScreenHeight());

        //const XMFLOAT4X4& view = camera->GetView();
        //const XMFLOAT4X4& proj = camera->GetProjection();
        XMFLOAT4X4 worldMatrix;

        XMMATRIX T = XMMatrixTranslationFromVector(XMLoadFloat3(&editorGhost->GetPosition()));
        XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&editorGhost->GetRotation()));
        XMStoreFloat4x4(&worldMatrix, R * T);

  /*      if (ImGuizmo::Manipulate((float*)&view, (float*)&proj, ImGuizmo::TRANSLATE | ImGuizmo::ROTATE, ImGuizmo::WORLD, (float*)&worldMatrix))
        {
            XMVECTOR scale, rot, trans;
            XMMatrixDecompose(&scale, &rot, &trans, XMLoadFloat4x4(&worldMatrix));

            XMFLOAT3 newPos; XMStoreFloat3(&newPos, trans);
            editorGhost->SetPosition(newPos);
            editorGhost->SetRotation(*(XMFLOAT4*)&rot);

            camTrack->eyeCurve.keys[selection.keyIndex].value = newPos;

            XMVECTOR Forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot);
            XMVECTOR NewFocus = XMVectorAdd(trans, XMVectorScale(Forward, 10.0f));
            XMFLOAT3 newFocusPos; XMStoreFloat3(&newFocusPos, NewFocus);

            if (selection.keyIndex < (int)camTrack->focusCurve.keys.size()) {
                camTrack->focusCurve.keys[selection.keyIndex].value = newFocusPos;
            }
        }*/
    }
}

void CinematicSequencerComponent::DrawTrackList()
{
    ImGui::BeginChild("TrackList", ImVec2(0, 0), false);
    if (ImGui::Button("+ Add Track")) ImGui::OpenPopup("AddTrackPopup");
    if (ImGui::BeginPopup("AddTrackPopup"))
    {
        // �w���p�[�����_: �g���b�N�ǉ����ɑ��o�C���h����
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
            AddAndBind(t); // ��������Bind�����
        }

        ImGui::EndPopup();
    }
    ImGui::Separator();

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

void CinematicSequencerComponent::DrawTimelineWindow()
{
    ImGui::BeginChild("TimelineView", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float scale = 100.0f;
    float rowHeight = ImGui::GetTextLineHeightWithSpacing();

    // �w�i�O���b�h�Ǝ��ԃo�[�`��i�����̂܂܁j
    for (float t = 0; t <= sequence->duration + 1; t += 1.0f) {
        float x = p.x + t * scale;
        drawList->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + 1000), 0x40FFFFFF);
        char buf[8]; sprintf_s(buf, "%.0fs", t);
        drawList->AddText(ImVec2(x + 2, p.y), 0xFFAAAAAA, buf);
    }
    float cx = p.x + currentTime * scale;
    drawList->AddLine(ImVec2(cx, p.y), ImVec2(cx, p.y + 1000), 0xFFFF0000, 2.0f);

    // �]���N���b�N�ŃV�[�N
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
        float t = (ImGui::GetMousePos().x - p.x) / scale;
        SetTime(t);
        selection.Clear();
        UpdateGhostCamera();
    }

    // �g���b�N�`�惋�[�v
    for (int i = 0; i < (int)sequence->tracks.size(); ++i) {
        float y = p.y + i * rowHeight + 25.0f;
        auto track = sequence->tracks[i];

        // -----------------------------------------------------------
        // A. �J�����g���b�N (�H�`�L�[)
        // -----------------------------------------------------------
        if (track->GetType() == TrackType::Camera) {
            CameraTrack* camTrack = static_cast<CameraTrack*>(track.get());
            auto& keys = camTrack->eyeCurve.keys;
            for (int k = 0; k < (int)keys.size(); ++k) {
                float x = p.x + keys[k].time * scale;
                ImU32 col = selection.IsSelected(i, k) ? 0xFFFF0000 : 0xFF00FF00;

                ImGui::SetCursorScreenPos(ImVec2(x - 6, y - 6));
                ImGui::PushID(i * 1000 + k);

                // �L�[����
                if (ImGui::InvisibleButton("##Key", ImVec2(12, 12))) {
                    selection.trackIndex = i;
                    selection.keyIndex = k;
                    UpdateGhostCamera();
                }
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    keys[k].time += ImGui::GetIO().MouseDelta.x / scale;
                    if (keys[k].time < 0.0f) keys[k].time = 0.0f;
                    // Focus�J�[�u������
                    if (k < (int)camTrack->focusCurve.keys.size()) camTrack->focusCurve.keys[k].time = keys[k].time;
                    SetTime(keys[k].time);
                }
                ImGui::PopID();

                // �`�� (�H�`)
                drawList->AddQuadFilled(ImVec2(x, y - 5), ImVec2(x + 5, y), ImVec2(x, y + 5), ImVec2(x - 5, y), col);
                drawList->AddQuad(ImVec2(x, y - 5), ImVec2(x + 5, y), ImVec2(x, y + 5), ImVec2(x - 5, y), 0xFF000000);
            }
        }
        // -----------------------------------------------------------
        // B. �A�j���[�V�����g���b�N (�o�[�\��)
        // -----------------------------------------------------------
        else if (track->GetType() == TrackType::Animation)
        {
            AnimationTrack* animTrack = static_cast<AnimationTrack*>(track.get());
            auto& keys = animTrack->keys;

            for (int k = 0; k < (int)keys.size(); ++k) {
                float startX = p.x + keys[k].time * scale;
                float endX = p.x + (keys[k].time + keys[k].duration) * scale;
                float width = endX - startX;
                if (width < 5.0f) width = 5.0f; // �ŏ����ۏ�

                float barY = y - 9.0f;
                float barH = 18.0f;

                ImVec2 rectMin(startX, barY);
                ImVec2 rectMax(startX + width, barY + barH);

                ImGui::SetCursorScreenPos(rectMin);
                ImGui::PushID(i * 1000 + k);

                // �o�[����
                if (ImGui::InvisibleButton("##Bar", ImVec2(width, barH))) {
                    selection.trackIndex = i;
                    selection.keyIndex = k;
                }
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    keys[k].time += ImGui::GetIO().MouseDelta.x / scale;
                    if (keys[k].time < 0.0f) keys[k].time = 0.0f;
                    SetTime(keys[k].time);
                }

                // �c�[���`�b�v
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s (%.2fs)", keys[k].animName.c_str(), keys[k].duration);
                }

                ImGui::PopID();

                // �`�� (��`)
                ImU32 col = selection.IsSelected(i, k) ? 0xFF55AAFF : 0xFF3366AA;
                drawList->AddRectFilled(rectMin, rectMax, col, 4.0f);
                drawList->AddRect(rectMin, rectMax, 0xFF000000, 4.0f);

                // �e�L�X�g�N���b�s���O�\��
                drawList->PushClipRect(rectMin, rectMax, true);
                drawList->AddText(ImVec2(rectMin.x + 4, rectMin.y + 2), 0xFFFFFFFF, keys[k].animName.c_str());
                drawList->PopClipRect();
            }
        }
        // -----------------------------------------------------------
        // ���ǉ� C. �G�t�F�N�g�g���b�N (�o�[�\���E���F)
        // -----------------------------------------------------------
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

                // �o�[����
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

                // �`�� (��`: ���F�n)
                ImU32 col = selection.IsSelected(i, k) ? 0xFFFFAAFF : 0xFFAA55AA;
                drawList->AddRectFilled(rectMin, rectMax, col, 4.0f);
                drawList->AddRect(rectMin, rectMax, 0xFF000000, 4.0f);

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