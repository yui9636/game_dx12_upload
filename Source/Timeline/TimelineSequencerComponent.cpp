#include "TimelineSequencerComponent.h"
#include "Runner/RunnerComponent.h"
#include "Actor/Actor.h"
#include "Character/PlayerEditorComponent.h" 
#include "Collision/ColliderComponent.h"
#include "Component/NodeAttachComponent.h"
#include "Model.h" 

// ★必須: 内部ヘッダー
#include <imgui.h>
#include <imgui_internal.h> 

#include "ImSequencer.h"
#include "ImCurveEdit.h"
#include <DirectXMath.h>            
#include <cstdio>                     
#include <fstream>
#include "Effect/EffectManager.h" 
#include "Effect/EffectNode.h" 
#include "Effect/EffectLoader.h"
#include "ImGuizmo.h"

#include "Storage/GameplayAsset.h"
#include <System/Dialog.h>
#include "Camera/CameraController.h"
#include "Audio/Audio.h"
#include "Audio/AudioSource.h"

#define PI 3.14159265358979323846f
#define DEG_TO_RAD (PI / 180.0f)

// ----------------------------------------------------------------------------
// 時間変換・クリップ長取得
// ----------------------------------------------------------------------------

int TimelineSequencerComponent::SecondsToFrames(float seconds) const
{
    float f = seconds * fps + 0.5f;
    if (f < 0.0f) f = 0.0f;
    return (int)f;
}

float TimelineSequencerComponent::FramesToSeconds(int frames) const
{
    if (fps <= 0.0f) return 0.0f;
    return (float)frames / fps;
}

float TimelineSequencerComponent::GetClipLengthSeconds() const
{
    if (runner) {
        float lengthSeconds = runner->GetClipLength();
        if (lengthSeconds <= 0.0f) lengthSeconds = 1.0f;
        return lengthSeconds;
    }
    float fallback = localClipLengthSec;
    if (fallback <= 0.0f) fallback = 1.0f;
    return fallback;
}

// ----------------------------------------------------------------------------
// 座標計算ロジック
// ----------------------------------------------------------------------------


DirectX::XMMATRIX TimelineSequencerComponent::CalcWorldMatrixForItem(const GESequencerItem& item)
{
    using DirectX::XMVECTOR;
    using DirectX::XMMATRIX;
    using DirectX::XMLoadFloat4x4;
    using DirectX::XMQuaternionRotationRollPitchYaw;
    using DirectX::XMMatrixRotationQuaternion;
    using DirectX::XMMatrixTranslation;
    using DirectX::XMMatrixScaling;
    using DirectX::XMMatrixDecompose;
    using DirectX::XMMatrixAffineTransformation;

    const float DEG_TO_RAD_LOCAL = PI / 180.0f;

    XMMATRIX W_base = DirectX::XMMatrixIdentity();
    std::shared_ptr<Actor> actor = GetActor();

    if (actor)
    {
        if (item.vfx.nodeIndex >= 0)
        {
            ::Model* model = actor->GetModelRaw();
            if (model) {
                const auto& nodes = model->GetNodes();
                if (item.vfx.nodeIndex < (int)nodes.size()) {
                    XMMATRIX W_Bone = XMLoadFloat4x4(&nodes[item.vfx.nodeIndex].worldTransform);
                    XMMATRIX W_Actor = XMLoadFloat4x4(&actor->GetTransform());
                    W_base = W_Bone * W_Actor;
                }
            }
        }
        else
        {
            W_base = XMLoadFloat4x4(&actor->GetTransform());
        }
    }

    // ★修正: 親行列(W_base)からスケール成分を強制的に除去(正規化)する
    // これにより、ボーンに極端なスケール(0.01など)が入っていても計算が狂わなくなる
    XMVECTOR s, r, t;
    if (XMMatrixDecompose(&s, &r, &t, W_base))
    {
        // スケールを(1,1,1)に固定して再構築
        W_base = XMMatrixAffineTransformation(
            DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f),
            DirectX::XMVectorZero(),
            r,
            t
        );
    }

    // オフセット行列の構築
    XMMATRIX T_offset = XMMatrixTranslation(item.vfx.offsetLocal.x, item.vfx.offsetLocal.y, item.vfx.offsetLocal.z);

    XMVECTOR r_quat = XMQuaternionRotationRollPitchYaw(
        item.vfx.offsetRotDeg.x * DEG_TO_RAD_LOCAL,
        item.vfx.offsetRotDeg.y * DEG_TO_RAD_LOCAL,
        item.vfx.offsetRotDeg.z * DEG_TO_RAD_LOCAL
    );
    XMMATRIX R_offset = XMMatrixRotationQuaternion(r_quat);
    XMMATRIX S_offset = XMMatrixScaling(item.vfx.offsetScale.x, item.vfx.offsetScale.y, item.vfx.offsetScale.z);

    XMMATRIX W_offset = S_offset * R_offset * T_offset;

    return W_offset * W_base;
}

// ----------------------------------------------------------------------------
// ライフサイクル (Start, Update)
// ----------------------------------------------------------------------------

void TimelineSequencerComponent::Start()
{
    typeNames.clear();
    typeNames.push_back("Hitbox");//0
    typeNames.push_back("Event");//1
    typeNames.push_back("Presets");//2
    typeNames.push_back("Audio"); //3
    typeNames.push_back("Shake");//4

    frameMin = 0;
    frameMax = SecondsToFrames(GetClipLengthSeconds());
    if (frameMax < frameMin) frameMax = frameMin;

    if (runner)
    {
        int f = SecondsToFrames(runner->GetCurrentSeconds());
        if (f < frameMin) f = frameMin;
        if (f > frameMax) f = frameMax;
        currentFrame = f;
        LoadSpeedCurvePointsFromRunner();
        uiSpeedCurveUseRangeSpace = false;
    }
    else
    {
        currentFrame = 0;
        uiSpeedCurveEnabled = false;
        uiSpeedCurveUseRangeSpace = false;
        uiSpeedCurvePoints.clear();
        uiSpeedCurvePoints.push_back(ImVec2(0.0f, 1.0f));
        uiSpeedCurvePoints.push_back(ImVec2(1.0f, 1.0f));
    }
}

void TimelineSequencerComponent::Update(float dt)
{
    std::shared_ptr<Actor> owner = GetActor();
    if (!owner) return;

    // 現在の時間を取得
    int frameNow = currentFrame;
    float timeNowSec = 0.0f;

    if (runner) {
        timeNowSec = runner->GetTimeSeconds();
        int f = SecondsToFrames(timeNowSec);
        if (f < frameMin) f = frameMin;
        frameNow = f;
        currentFrame = f;
    }

    std::shared_ptr<ColliderComponent> collider = owner->GetComponent<ColliderComponent>();
    if (collider) {
        collider->SyncFromSequencer(this, frameNow);
    }

    // ───────── Items (VFX, Audio, Shake) 同期 ─────────
    for (size_t i = 0; i < items.size(); ++i)
    {
        GESequencerItem& it = items[i];

        // -----------------------------------------------------------------
        // Type 2: Presets (VFX)
        // -----------------------------------------------------------------
        if (it.type == 2)
        {
            const bool isInside = (frameNow >= it.start && frameNow <= it.end);

            if (isInside)
            {
                // A. 再生開始
                if (!it.vfxActive || !it.vfxInstance)
                {
                    if (it.vfx.assetId[0] != '\0')
                    {
                        // 座標計算
                        DirectX::XMMATRIX W = CalcWorldMatrixForItem(it);

                        // 初期位置を指定してPlay
                        float t[3], r[3], s[3];
                        DirectX::XMFLOAT4X4 matF;
                        DirectX::XMStoreFloat4x4(&matF, W);
                        ImGuizmo::DecomposeMatrixToComponents(&matF.m[0][0], t, r, s);

                        auto instance = EffectManager::Get().Play(it.vfx.assetId, { t[0], t[1], t[2] });

                        if (instance)
                        {
                            instance->loop = !it.vfx.fireOnEnterOnly;
                            instance->isSequencerControlled = true;

                            // 親行列をセット
                            DirectX::XMStoreFloat4x4(&instance->parentMatrix, W);

                            // ローカル変形初期化
                            instance->overrideLocalTransform.position = it.vfx.offsetLocal;
                            instance->overrideLocalTransform.rotation = it.vfx.offsetRotDeg;
                            instance->overrideLocalTransform.scale = it.vfx.offsetScale;

                            float barDuration = FramesToSeconds(it.end - it.start);
                            if (barDuration < 0.1f) barDuration = 0.1f;
                            instance->lifeTime = barDuration;

                            it.vfxInstance = instance;
                            it.vfxActive = true;
                        }
                    }
                }

                // B. 毎フレーム同期
                if (it.vfxInstance)
                {
                    // 1. 寿命
                    float barDuration = FramesToSeconds(it.end - it.start);
                    if (barDuration < 0.1f) barDuration = 0.1f;
                    it.vfxInstance->lifeTime = barDuration;

                    // 2. ボーン行列(Parent)
                    using namespace DirectX;
                    XMMATRIX W_Socket = XMMatrixIdentity();
                    if (owner) {
                        if (it.vfx.nodeIndex >= 0) {
                            ::Model* model = owner->GetModelRaw();
                            if (model && it.vfx.nodeIndex < (int)model->GetNodes().size()) {
                                XMMATRIX W_Bone = XMLoadFloat4x4(&model->GetNodes()[it.vfx.nodeIndex].worldTransform);
                                XMMATRIX W_Actor = XMLoadFloat4x4(&owner->GetTransform());
                                W_Socket = W_Bone * W_Actor;
                            }
                        }
                        else {
                            W_Socket = XMLoadFloat4x4(&owner->GetTransform());
                        }
                    }
                    // 正規化
                    {
                        XMFLOAT4X4 m; XMStoreFloat4x4(&m, W_Socket);
                        XMVECTOR ax = XMVector3Normalize(XMVectorSet(m._11, m._12, m._13, 0));
                        XMVECTOR ay = XMVector3Normalize(XMVectorSet(m._21, m._22, m._23, 0));
                        XMVECTOR az = XMVector3Normalize(XMVectorSet(m._31, m._32, m._33, 0));
                        XMVECTOR p = XMVectorSet(m._41, m._42, m._43, 1);
                        W_Socket = XMMatrixIdentity();
                        W_Socket.r[0] = ax; W_Socket.r[1] = ay; W_Socket.r[2] = az; W_Socket.r[3] = p;
                    }
                    DirectX::XMStoreFloat4x4(&it.vfxInstance->parentMatrix, W_Socket);

                    // 3. ローカル変形
                    it.vfxInstance->overrideLocalTransform.position = it.vfx.offsetLocal;
                    it.vfxInstance->overrideLocalTransform.rotation = it.vfx.offsetRotDeg;
                    it.vfxInstance->overrideLocalTransform.scale = it.vfx.offsetScale;

                    // 4. 時間同期
                    float startTimeSec = FramesToSeconds(it.start);
                    float effectAge = timeNowSec - startTimeSec;
                    EffectManager::Get().SyncInstanceToTime(it.vfxInstance, effectAge);
                }
            }
            else
            {
                // C. 範囲外なら停止
                if (it.vfxActive)
                {
                    if (it.vfxInstance) it.vfxInstance->Stop(true);
                    it.vfxInstance.reset();
                    it.vfxActive = false;
                }
            }
        }
        // -----------------------------------------------------------------
        // Type 3: Audio
        // -----------------------------------------------------------------
        else if (it.type == 3)
        {
            const bool isInside = (frameNow >= it.start && frameNow <= it.end);

            // A. 再生開始
            if (isInside)
            {
                if (!it.audioActive)
                {
                    it.audioActive = true;
                    if (it.audio.assetId[0] != '\0')
                    {
                        if (it.audio.is3D)
                        {
                            // 3D座標の計算
                            DirectX::XMFLOAT3 pos = owner->GetPosition();
                            if (it.audio.nodeIndex >= 0) {
                                if (auto model = owner->GetModelRaw()) {
                                    auto& nodes = model->GetNodes();
                                    if (it.audio.nodeIndex < (int)nodes.size()) {
                                        DirectX::XMMATRIX W_Bone = DirectX::XMLoadFloat4x4(&nodes[it.audio.nodeIndex].worldTransform);
                                        DirectX::XMMATRIX W_Actor = DirectX::XMLoadFloat4x4(&owner->GetTransform());
                                        DirectX::XMMATRIX W_Final = W_Bone * W_Actor;

                                        DirectX::XMFLOAT4X4 mat;
                                        DirectX::XMStoreFloat4x4(&mat, W_Final);
                                        pos = { mat._41, mat._42, mat._43 };
                                    }
                                }
                            }
                            // 3D再生
                            it.audioSource = Audio::Instance()->Play3D(
                                it.audio.assetId, pos,
                                it.audio.volume, it.audio.pitch, it.audio.loop
                            );
                        }
                        else
                        {
                            // 2D再生
                            Audio::Instance()->Play2D(
                                it.audio.assetId, it.audio.volume, it.audio.pitch, it.audio.loop
                            );
                            it.audioSource.reset();
                        }
                    }
                }

                // B. 3D位置の更新
                if (it.audioActive && it.audioSource && it.audio.is3D)
                {
                    DirectX::XMFLOAT3 pos = owner->GetPosition();
                    if (it.audio.nodeIndex >= 0) {
                        if (auto model = owner->GetModelRaw()) {
                            auto& nodes = model->GetNodes();
                            if (it.audio.nodeIndex < (int)nodes.size()) {
                                DirectX::XMMATRIX W_Bone = DirectX::XMLoadFloat4x4(&nodes[it.audio.nodeIndex].worldTransform);
                                DirectX::XMMATRIX W_Actor = DirectX::XMLoadFloat4x4(&owner->GetTransform());
                                DirectX::XMMATRIX W_Final = W_Bone * W_Actor;
                                DirectX::XMFLOAT4X4 mat;
                                DirectX::XMStoreFloat4x4(&mat, W_Final);
                                pos = { mat._41, mat._42, mat._43 };
                            }
                        }
                    }
                    it.audioSource->SetPosition(pos);
                }
            }
            // C. 停止
            else
            {
                if (it.audioActive)
                {
                    it.audioActive = false;
                    if (it.audioSource)
                    {
                        it.audioSource->Stop();
                        it.audioSource.reset();
                    }
                }
            }
        }
        // -----------------------------------------------------------------
        // ★追加 Type 4: Shake & HitStop (Motion Shake)
        // -----------------------------------------------------------------
        else if (it.type == 4)
        {
            // 範囲内に入ったか判定
            bool isInside = (frameNow >= it.start && frameNow <= it.end);

            // 巻き戻し対応: 範囲より前に戻ったらフラグをリセット
            if (frameNow < it.start) it.fired = false;

            // --------------------------------------------------------
            // 1. ヒットストップ (イベント方式: 突入時に1回だけ発動)
            // --------------------------------------------------------
            // ※これはRunnerの状態を変えるものなので、イベント方式が正解
            if (isInside && !it.fired)
            {
                it.fired = true;

                // ヒットストップ時間が設定されていれば実行
                if (targetCamera && runner && it.shake.hitStopDuration > 0.0f)
                {
                    runner->RequestHitStop(it.shake.hitStopDuration, it.shake.timeScale);
                }

            }

            // --------------------------------------------------------
            // 2. カメラシェイク (評価方式: 毎フレーム計算して上書き)
            // --------------------------------------------------------
            // ※「範囲内にいる間」はずっと計算し続けることで、エディタでシークしても揺れる
            if (isInside)
            {
                // (1) 経過時間の計算
                float startTime = FramesToSeconds(it.start);
                float time = timeNowSec - startTime;
                if (time < 0.0f) time = 0.0f;

                float progress = 0.0f;
                if (it.shake.duration > 0.0f) {
                    progress = time / it.shake.duration;
                }

                float decayFactor = 1.0f - progress;
                if (decayFactor < 0.0f) decayFactor = 0.0f;


                // 現在の強さ
                float currentAmp = it.shake.amplitude * decayFactor;

                static float shakeRealTime = 0.0f;
                shakeRealTime += dt;

                // まだ揺れが残っているなら計算
                if (currentAmp > 0.001f)
                {
                    float t = shakeRealTime * it.shake.frequency;

                    float nX = sinf(t) + sinf(t * 0.5f + 1.5f) * 0.5f;
                    float nY = cosf(t * 1.1f) + sinf(t * 0.8f + 2.0f) * 0.5f;
                    float nZ = sinf(t * 0.3f);

                    DirectX::XMFLOAT3 offset;
                    offset.x = nX * currentAmp;
                    offset.y = nY * currentAmp;
                    offset.z = nZ * currentAmp;
             
              
                    if (targetCamera)
                    {
                        targetCamera->SetTimelineShakeOffset(offset);
                    }
                }
            }
         }
    }
}

namespace {
    struct SeqModelAlwaysToolbar : public ImSequencer::SequenceInterface
    {
        int frameMin = 0; int frameMax = 600;
        std::vector<std::string>* typeNames = nullptr;
        std::vector<GESequencerItem>* items = nullptr;
        int dummyStart = 0; int dummyEnd = 0; int dummyType = 0; unsigned int dummyColor = 0xFF404040u;

        int GetFrameMin() const override { return frameMin; }
        int GetFrameMax() const override { return frameMax; }
        int GetItemCount() const override { return items ? (int)items->size() : 0; }
        int GetItemTypeCount() const override { return typeNames ? (int)typeNames->size() : 0; }
        const char* GetItemTypeName(int typeIndex) const override { return (typeNames && typeIndex >= 0) ? (*typeNames)[typeIndex].c_str() : ""; }
        const char* GetItemLabel(int index) const override { return (items && index >= 0) ? (*items)[index].label.c_str() : ""; }
        void Get(int index, int** start, int** end, int* type, unsigned int* color) override {
            if (items && index >= 0) {
                GESequencerItem& it = (*items)[index];
                if (start) *start = &it.start; if (end) *end = &it.end; if (type) *type = it.type; if (color) *color = it.color;
            }
        }
        void Add(int type) override {
            if (!items) return;
            GESequencerItem it; it.type = type; it.start = frameMin; it.end = frameMin + 10;
            // デフォルト設定
            if (type == 0) { it.color = 0xFF3CB371u; it.label = "Hitbox"; it.hb.radius = 30.0f; }
            else if (type == 1) { it.color = 0xFFFFA500u; it.label = "Event"; }
            else if (type == 2) { it.color = 0xFF66CCFFu; it.label = "Presets"; it.vfx.nodeIndex = -1; }
            else if (type == 3) { it.color = 0xFF00FF7Fu; it.label = "Audio"; it.audio.volume = 1.0f; } // ★Audio初期化
            else if (type == 4) {
                it.color = 0xFFFF4040u;
                it.label = "Shake";
                it.shake.amplitude = 0.5f;
                it.shake.duration = 0.2f;
                it.shake.frequency = 20.0f;
                it.shake.decay = 0.9f;
            }
            items->push_back(it);
        }
        void Del(int index) override {
            if (items && index >= 0) {
                // VFX停止
                if ((*items)[index].vfxActive && (*items)[index].vfxInstance) (*items)[index].vfxInstance->Stop();
                // Audio停止
                if ((*items)[index].audioActive && (*items)[index].audioSource) (*items)[index].audioSource->Stop();

                items->erase(items->begin() + index);
            }
        }
        void Duplicate(int index) override {
            if (items && index >= 0) {
                GESequencerItem cp = (*items)[index];
                int w = cp.end - cp.start; cp.start += w + 1; cp.end += w + 1;
                cp.vfxActive = false; cp.vfxInstance.reset();
                cp.audioActive = false; cp.audioSource.reset(); // Audioリセット
                items->push_back(cp);
            }
        }
        size_t GetCustomHeight(int) override { return 0; }
    };

    struct SpeedCurveDelegate : public ImCurveEdit::Delegate {
        ImVec2 min, max; unsigned int bgColor = 0xFF131313u;
        std::vector<ImVec2>* points = nullptr;
        ImVec2& GetMin() { return min; }
        ImVec2& GetMax() { return max; }
        unsigned int GetBackgroundColor() { return bgColor; }
        size_t GetCurveCount() { return points ? 1u : 0u; }
        bool IsVisible(size_t) { return points != nullptr; }
        ImCurveEdit::CurveType GetCurveType(size_t) { return ImCurveEdit::CurveSmooth; }
        unsigned int GetCurveColor(size_t) { return 0xFF4FC3F7u; }
        size_t GetPointCount(size_t) { return points ? points->size() : 0u; }
        ImVec2* GetPoints(size_t) { return points ? points->data() : nullptr; }
        int EditPoint(size_t, int idx, ImVec2 v) {
            if (!points) return idx;
            if (v.x < min.x) v.x = min.x; if (v.x > max.x) v.x = max.x;
            if (v.y < min.y) v.y = min.y; if (v.y > max.y) v.y = max.y;
            (*points)[idx] = v; return idx;
        }
        void AddPoint(size_t, ImVec2 v) { if (points) points->push_back(v); }
        void BeginEdit(int) {}
        void EndEdit() {}
    };
}

void TimelineSequencerComponent::OnGUI()
{
    // タイプ名を拡張
    if (typeNames.size() < 5) {
        typeNames = { "Hitbox", "Event", "Presets", "Audio", "Shake" };
    }

    if (ImGui::Begin("Timeline"))
    {
        bool isPlaying = runner ? runner->IsPlaying() : false;
        float currentTime = runner ? runner->GetTimeSeconds() : 0.0f;
        float totalTime = GetClipLengthSeconds();

        // [再生コントロール]
        ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? ImVec4(0.5f, 0.1f, 0.1f, 1) : ImVec4(0.1f, 0.5f, 0.1f, 1));
        if (ImGui::Button(isPlaying ? " || " : " > ", ImVec2(40, 24))) {
            if (runner) { if (isPlaying) runner->Pause(); else runner->Play(); }
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button(" |< ", ImVec2(40, 24))) {
            if (runner) { runner->SetTimeSeconds(0.0f); runner->Pause(); currentFrame = 0; }
        }

        // [シークバー]
        ImGui::SameLine();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 180);
        float seekTime = currentTime;
        if (ImGui::SliderFloat("##Seek", &seekTime, 0.0f, totalTime, "%.2fs"))
        {
            if (runner)
            {
                runner->Pause();
                runner->SetTimeSeconds(seekTime);
            }
        }
        ImGui::PopItemWidth();

        // [速度スライダー]
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        float spd = runner ? runner->GetPlaySpeed() : 1.0f;
        if (ImGui::SliderFloat("##Speed", &spd, 0.05f, 2.0f, "x%.2f")) {
            if (runner) runner->SetPlaySpeed(spd);
        }

        // [追加ボタン]
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
        ImGui::TextDisabled("Add Track:");
        ImGui::SameLine();

        if (ImGui::Button("+ Hitbox")) {
            GESequencerItem it;
            it.type = 0; it.start = currentFrame; it.end = currentFrame + 30;
            it.color = 0xFF3CB371u; it.label = "Hitbox"; it.hb.radius = 0.5f;
            items.push_back(it);
            selectedEntry = (int)items.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Event")) {
            GESequencerItem it;
            it.type = 1; it.start = currentFrame; it.end = currentFrame + 5;
            it.color = 0xFFFFA500u; it.label = "Event";
            items.push_back(it);
            selectedEntry = (int)items.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ VFX")) {
            GESequencerItem it;
            it.type = 2; it.start = currentFrame; it.end = currentFrame + 60;
            it.color = 0xFF66CCFFu; it.label = "Effect";
            it.vfx.nodeIndex = -1;
            items.push_back(it);
            selectedEntry = (int)items.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Audio")) {
            GESequencerItem it;
            it.type = 3; it.start = currentFrame; it.end = currentFrame + 60;
            it.color = 0xFF00FF7Fu; it.label = "Audio";
            it.audio.nodeIndex = -1; it.audio.volume = 1.0f;
            items.push_back(it);
            selectedEntry = (int)items.size() - 1;
        }
        ImGui::SameLine();
        // ★追加: Shakeボタン
        if (ImGui::Button("+ Shake")) {
            GESequencerItem it;
            it.type = 4; // Shake type
            it.start = currentFrame;
            it.end = currentFrame + 5; // ワンショットなので短くてOK
            it.color = 0xFFFF4040u;    // 赤色
            it.label = "Shake";

            // デフォルト値 (打撃感のある設定)
            it.shake.amplitude = 0.5f;
            it.shake.duration = 0.2f;
            it.shake.frequency = 20.0f;
            it.shake.decay = 0.9f;

            items.push_back(it);
            selectedEntry = (int)items.size() - 1;
        }

        ImGui::Separator();

        // [Sequencer 本体]
        {
            frameMin = 0;
            frameMax = SecondsToFrames(totalTime);
            if (frameMax < 60) frameMax = 60;

            if (runner) currentFrame = SecondsToFrames(currentTime);

            SeqModelAlwaysToolbar model;
            model.frameMin = frameMin;
            model.frameMax = frameMax;
            model.typeNames = &typeNames;
            model.items = &items;

            bool expanded = true;
            int firstFrame = firstVisible;
            unsigned flags = ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_CHANGE_FRAME | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_ADD;

            if (ImSequencer::Sequencer(&model, &currentFrame, &expanded, &selectedEntry, &firstFrame, flags)) {
                if (runner) {
                    runner->Pause();
                    runner->SetCurrentSeconds(FramesToSeconds(currentFrame));
                }
            }
            firstVisible = firstFrame;
            SanitizeItems();
        }

        // [Speed Curve]
        if (ImGui::CollapsingHeader("Playback Speed Curve"))
        {
            bool desiredEnabled = uiSpeedCurveEnabled;
            bool desiredUseRange = uiSpeedCurveUseRangeSpace;
            if (ImGui::Checkbox("Enable", &desiredEnabled)) uiSpeedCurveEnabled = desiredEnabled;
            ImGui::SameLine();
            if (ImGui::Checkbox("Normalized Time (0-1)", &desiredUseRange)) uiSpeedCurveUseRangeSpace = desiredUseRange;
            SpeedCurveDelegate del;
            del.min = ImVec2(0.0f, 0.0f);
            del.max = ImVec2(1.0f, 2.0f);
            del.points = &uiSpeedCurvePoints;
            float curveW = ImGui::GetContentRegionAvail().x;
            ImCurveEdit::Edit(del, ImVec2(curveW, 120.0f), ImGui::GetID("SpeedCurveEditor"), nullptr, nullptr);
            if (runner) StoreSpeedCurvePointsToRunner();
        }
    }
    ImGui::End();

    // Inspector
    if (ImGui::Begin("Inspector"))
    {
        if (selectedEntry >= 0 && selectedEntry < (int)items.size()) {
            GESequencerItem& it = items[(size_t)selectedEntry];

            // タイプ名の表示
            const char* typeName = "Unknown";
            if (it.type == 0) typeName = "Hitbox";
            else if (it.type == 1) typeName = "Event";
            else if (it.type == 2) typeName = "Presets (VFX)";
            else if (it.type == 3) typeName = "Audio";
            else if (it.type == 4) typeName = "Shake";

            ImGui::Text("Type: %s  Range: %d - %d", typeName, it.start, it.end);
            ImGui::Separator();

            // -----------------------------------------------------------------
            // Type 2: Presets (VFX)
            // -----------------------------------------------------------------
            if (it.type == 2)
            {
                ImGui::SeparatorText("VFX Settings");
                GEVfxPayload& vfx = it.vfx;
                float w = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(w - 30);
                ImGui::InputText("##AssetId", vfx.assetId, sizeof(vfx.assetId));
                ImGui::SameLine();
                if (ImGui::Button("...")) {
                    char path[MAX_PATH] = {};
                    if (Dialog::OpenFileName(path, MAX_PATH, "Effect JSON (*.json)\0*.json\0", "Select Effect", nullptr) == DialogResult::OK) {
                        // パス処理（省略せず記述）
                        std::string fullPath = path;
                        std::string key = "Data\\";
                        size_t pos = fullPath.find(key);
                        if (pos != std::string::npos) {
                            std::string relPath = "Data/" + fullPath.substr(pos + key.length());
                            for (auto& c : relPath) { if (c == '\\') c = '/'; }
                            strcpy_s(vfx.assetId, relPath.c_str());
                        }
                        else {
                            strcpy_s(vfx.assetId, path);
                        }

                        // 寿命自動読み込み
                        float life = 2.0f;
                        float inF = 0, outF = 0; bool lp = false;
                        if (EffectLoader::LoadEffect(vfx.assetId, &life, &inF, &outF, &lp)) {
                            int frames = SecondsToFrames(life);
                            if (frames < 1) frames = 1;
                            it.end = it.start + frames;
                        }
                    }
                }

                int hierarchyIndex = -1;
                if (auto owner = GetActor()) if (auto ed = owner->GetComponent<PlayerEditorComponent>()) hierarchyIndex = ed->GetSelectedNodeIndex();
                ImGui::InputInt("Node Index", &vfx.nodeIndex);
                ImGui::SameLine();
                if (ImGui::Button("Get Selection##V") && hierarchyIndex >= 0) vfx.nodeIndex = hierarchyIndex;

                ImGui::Separator();
                ImGui::Text("Offset Transform");
                ImGui::DragFloat3("Pos", &vfx.offsetLocal.x, 0.01f);
                ImGui::DragFloat3("Rot", &vfx.offsetRotDeg.x, 1.0f);
                ImGui::DragFloat3("Scale", &vfx.offsetScale.x, 0.01f);
                ImGui::Checkbox("Fire On Enter Only", &vfx.fireOnEnterOnly);

                if (ImGui::Button("Force Stop")) {
                    if (it.vfxActive && it.vfxInstance) {
                        it.vfxInstance->Stop();
                        it.vfxInstance.reset();
                        it.vfxActive = false;
                    }
                }
            }
            // -----------------------------------------------------------------
            // Type 3: Audio
            // -----------------------------------------------------------------
            else if (it.type == 3)
            {
                ImGui::SeparatorText("Audio Settings");
                GEAudioPayload& audio = it.audio;

                float w = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(w - 30);
                ImGui::InputText("##AudioAsset", audio.assetId, sizeof(audio.assetId));
                ImGui::SameLine();
                if (ImGui::Button("...")) {
                    char path[MAX_PATH] = {};
                    if (Dialog::OpenFileName(path, MAX_PATH, "Audio (*.wav)\0*.wav\0", "Select Audio", nullptr) == DialogResult::OK) {
                        std::string fullPath = path;
                        std::string key = "Data\\";
                        size_t pos = fullPath.find(key);
                        if (pos != std::string::npos) {
                            std::string relPath = "Data/" + fullPath.substr(pos + key.length());
                            for (auto& c : relPath) { if (c == '\\') c = '/'; }
                            strcpy_s(audio.assetId, relPath.c_str());
                        }
                        else {
                            strcpy_s(audio.assetId, path);
                        }
                    }
                }

                ImGui::DragFloat("Volume", &audio.volume, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Pitch", &audio.pitch, 0.01f, 0.1f, 3.0f);
                ImGui::Checkbox("Loop", &audio.loop);

                ImGui::Separator();
                ImGui::Checkbox("3D Sound", &audio.is3D);
                if (audio.is3D)
                {
                    int hierarchyIndex = -1;
                    if (auto owner = GetActor()) if (auto ed = owner->GetComponent<PlayerEditorComponent>()) hierarchyIndex = ed->GetSelectedNodeIndex();

                    ImGui::InputInt("Bone Index", &audio.nodeIndex);
                    ImGui::SameLine();
                    if (ImGui::Button("Get Selection##A") && hierarchyIndex >= 0) audio.nodeIndex = hierarchyIndex;
                }
            }
            // -----------------------------------------------------------------
            // Type 0: Hitbox
            // -----------------------------------------------------------------
            else if (it.type == 0) {
                ImGui::SeparatorText("Hitbox Settings");
                int hierarchyIndex = -1;
                if (auto owner = GetActor()) if (auto ed = owner->GetComponent<PlayerEditorComponent>()) hierarchyIndex = ed->GetSelectedNodeIndex();

                ImGui::InputInt("Node Index", &it.hb.nodeIndex);
                ImGui::SameLine();
                if (ImGui::Button("Get Selection") && hierarchyIndex >= 0) it.hb.nodeIndex = hierarchyIndex;

                ImGui::DragFloat3("Offset", (float*)&it.hb.offsetLocal, 0.01f);
                ImGui::DragFloat("Radius", &it.hb.radius, 0.1f);
            }
            // -----------------------------------------------------------------
            // ★追加 Type 4: Shake & HitStop
            // -----------------------------------------------------------------
            else if (it.type == 4)
            {
                ImGui::SeparatorText("Camera Shake");
                // ニーア的なパラメータ群
                ImGui::DragFloat("Amplitude (m)", &it.shake.amplitude, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Duration (s)", &it.shake.duration, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Frequency (Hz)", &it.shake.frequency, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Decay (0-1)", &it.shake.decay, 0.01f, 0.0f, 1.0f);

                ImGui::SeparatorText("Hit Stop");
                ImGui::DragFloat("Stop Duration (s)", &it.shake.hitStopDuration, 0.01f, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set > 0 to enable HitStop");

                ImGui::DragFloat("Time Scale", &it.shake.timeScale, 0.01f, 0.0f, 1.0f, "%.2f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("0.0 = Freeze, 0.1 = Slow Motion");
            }
        }
    }
    ImGui::End();
}


void TimelineSequencerComponent::OnGizmo()
{
    for (auto& item : items)
    {
        if (item.type == 2 && (item.vfxActive || &item == &items[selectedEntry]))
        {
            DrawGizmoForItem(item);
        }
    }
}
void TimelineSequencerComponent::DrawGizmoForItem(GESequencerItem& item)
{
    using namespace DirectX;

    // 1. 現在のワールド行列を計算（正規化済み親行列を使用）
    XMMATRIX W_current = CalcWorldMatrixForItem(item);

    XMFLOAT4X4 matF;
    XMStoreFloat4x4(&matF, W_current);

    static float dummyView[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    static float dummyProj[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

    ImGuizmo::Manipulate(
        dummyView, dummyProj,
        ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE,
        ImGuizmo::LOCAL,
        reinterpret_cast<float*>(&matF)
    );

    XMMATRIX W_new = XMLoadFloat4x4(&matF);

    // 2. 親行列の再取得＆正規化
    XMMATRIX W_socket = XMMatrixIdentity();
    std::shared_ptr<Actor> actor = GetActor();
    if (actor)
    {
        if (item.vfx.nodeIndex >= 0) {
            ::Model* model = actor->GetModelRaw();
            if (model && item.vfx.nodeIndex < (int)model->GetNodes().size()) {
                XMMATRIX W_Bone = XMLoadFloat4x4(&model->GetNodes()[item.vfx.nodeIndex].worldTransform);
                XMMATRIX W_Actor = XMLoadFloat4x4(&actor->GetTransform());
                W_socket = W_Bone * W_Actor;
            }
        }
        else {
            W_socket = XMLoadFloat4x4(&actor->GetTransform());
        }
    }
    {
        XMFLOAT4X4 m; XMStoreFloat4x4(&m, W_socket);
        XMVECTOR ax = XMVector3Normalize(XMVectorSet(m._11, m._12, m._13, 0));
        XMVECTOR ay = XMVector3Normalize(XMVectorSet(m._21, m._22, m._23, 0));
        XMVECTOR az = XMVector3Normalize(XMVectorSet(m._31, m._32, m._33, 0));
        XMVECTOR p = XMVectorSet(m._41, m._42, m._43, 1);
        W_socket = XMMatrixIdentity();
        W_socket.r[0] = ax; W_socket.r[1] = ay; W_socket.r[2] = az; W_socket.r[3] = p;
    }

    // 3. 逆算して保存
    XMMATRIX W_invSocket = XMMatrixInverse(nullptr, W_socket);
    XMMATRIX W_newLocal = W_new * W_invSocket;

    XMVECTOR s, r, t;
    XMMatrixDecompose(&s, &r, &t, W_newLocal);

    XMStoreFloat3(&item.vfx.offsetLocal, t);
    XMStoreFloat3(&item.vfx.offsetScale, s);

    // Quaternion -> Euler
    XMFLOAT4 q; XMStoreFloat4(&q, r);

    float t_arr[3], r_arr[3], s_arr[3];
    ImGuizmo::DecomposeMatrixToComponents(&matF.m[0][0], t_arr, r_arr, s_arr);

    XMFLOAT4X4 localF; XMStoreFloat4x4(&localF, W_newLocal);
    ImGuizmo::DecomposeMatrixToComponents(&localF.m[0][0], t_arr, r_arr, s_arr);

    item.vfx.offsetLocal = { t_arr[0], t_arr[1], t_arr[2] };
    item.vfx.offsetRotDeg = { r_arr[0], r_arr[1], r_arr[2] };
    item.vfx.offsetScale = { s_arr[0], s_arr[1], s_arr[2] };

    // 4. プレビュー反映
    if (item.vfxInstance && item.vfxInstance->rootNode)
    {
        XMStoreFloat4x4(&item.vfxInstance->parentMatrix, W_socket);

        item.vfxInstance->overrideLocalTransform.position = item.vfx.offsetLocal;
        item.vfxInstance->overrideLocalTransform.rotation = item.vfx.offsetRotDeg;
        item.vfxInstance->overrideLocalTransform.scale = item.vfx.offsetScale;

        // 即時適用
        item.vfxInstance->rootNode->localTransform = item.vfxInstance->overrideLocalTransform;
        item.vfxInstance->rootNode->UpdateTransform(W_socket);
    }
}

void TimelineSequencerComponent::SanitizeItems()
{
    std::vector<GESequencerItem>& arr = GetItemsMutable();
    const int minF = frameMin;
    const int maxF = frameMax;

    for (size_t i = 0; i < arr.size(); ++i)
    {
        GESequencerItem& it = arr[i];

        if (it.start < minF) it.start = minF;
        if (it.end < minF) it.end = minF;

        if (it.end < it.start) {
            int tmp = it.start;
            it.start = it.end;
            it.end = tmp;
        }

        if (it.color == 0u) {
            if (it.type == 0) { it.color = 0xFF3CB371u; }
            else if (it.type == 2) { it.color = 0xFF66CCFFu; }
            else if (it.type == 3) { it.color = 0xFFFFD700u; }
            else if (it.type == 4) { it.color = 0xFFFF4040u; }
        }
    }
}

void TimelineSequencerComponent::BindAnimation(int animIndex, float clipLengthSec)
{
    animationIndex = animIndex;
    if (clipLengthSec <= 0.0f) clipLengthSec = 1.0f;
    localClipLengthSec = clipLengthSec;
}

const std::vector<GESequencerItem>& TimelineSequencerComponent::GetItems() const { return items; }
std::vector<GESequencerItem>& TimelineSequencerComponent::GetItemsMutable() { return items; }

GECurveSettings TimelineSequencerComponent::GetCurveSettings() const
{
    GECurveSettings s;
    s.enabled = uiSpeedCurveEnabled;
    s.useRange = uiSpeedCurveUseRangeSpace;
    for (const auto& p : uiSpeedCurvePoints) {
        s.points.push_back({ p.x, p.y });
    }
    return s;
}

void TimelineSequencerComponent::SetCurveSettings(const GECurveSettings& settings)
{
    uiSpeedCurveEnabled = settings.enabled;
    uiSpeedCurveUseRangeSpace = settings.useRange;
    uiSpeedCurvePoints.clear();
    for (const auto& p : settings.points) {
        uiSpeedCurvePoints.push_back(ImVec2(p.x, p.y));
    }
    if (uiSpeedCurvePoints.empty()) {
        uiSpeedCurvePoints.push_back(ImVec2(0.0f, 1.0f));
        uiSpeedCurvePoints.push_back(ImVec2(1.0f, 1.0f));
    }
    if (runner) {
        runner->SetSpeedCurveEnabled(uiSpeedCurveEnabled);
        runner->SetSpeedCurveUseRangeSpace(uiSpeedCurveUseRangeSpace);
        StoreSpeedCurvePointsToRunner();
    }
}

void TimelineSequencerComponent::LoadSpeedCurvePointsFromRunner()
{
    uiSpeedCurvePoints.clear();
    if (!runner) { uiSpeedCurvePoints.push_back(ImVec2(0.0f, 1.0f)); uiSpeedCurvePoints.push_back(ImVec2(1.0f, 1.0f)); return; }
    uiSpeedCurveEnabled = runner->IsSpeedCurveEnabled();
    uiSpeedCurveUseRangeSpace = runner->IsSpeedCurveUseRangeSpace();
    const std::vector<RunnerComponent::CurvePoint>& pts = runner->GetSpeedCurvePoints();
    if (pts.empty()) { uiSpeedCurvePoints.push_back(ImVec2(0.0f, 1.0f)); uiSpeedCurvePoints.push_back(ImVec2(1.0f, 1.0f)); return; }
    for (size_t i = 0; i < pts.size(); ++i) {
        uiSpeedCurvePoints.push_back(ImVec2(pts[i].t01, pts[i].value));
    }
}

void TimelineSequencerComponent::StoreSpeedCurvePointsToRunner()
{
    if (!runner) return;
    std::vector<RunnerComponent::CurvePoint> pts;
    pts.reserve(uiSpeedCurvePoints.size());
    for (size_t i = 0; i < uiSpeedCurvePoints.size(); ++i) {
        RunnerComponent::CurvePoint cp; cp.t01 = uiSpeedCurvePoints[i].x; cp.value = uiSpeedCurvePoints[i].y;
        pts.push_back(cp);
    }
    runner->SetSpeedCurvePoints(pts);
    runner->SetSpeedCurveEnabled(uiSpeedCurveEnabled);
    runner->SetSpeedCurveUseRangeSpace(uiSpeedCurveUseRangeSpace);
}

const GESequencerItem* TimelineSequencerComponent::GetActiveShakeItem() const
{
    // 全アイテムの中から、現在再生中のフレームに含まれる Type 4 (Shake) を探す
    for (const auto& it : items)
    {
        if (it.type == 4 && currentFrame >= it.start && currentFrame <= it.end)
        {
            return &it;
        }
    }
    return nullptr;
}