#include "AnimatorComponent.h"
#include "Actor/Actor.h"
#include "Model/Model.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <stack>
#include "easing.h" 
#include "IAnimationDriver.h"

using namespace DirectX;

static void FillBindPose(std::vector<Model::NodePose>& poses, const Model* model) {
    if (!model) return;
    const auto& nodes = model->GetNodes();
    poses.resize(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        poses[i].position = nodes[i].position;
        poses[i].rotation = nodes[i].rotation;
        poses[i].scale = nodes[i].scale;
    }
}


std::vector<std::string> AnimatorComponent::GetAnimationNameList() const
{
    std::vector<std::string> names;

    // 1. まず自分の modelRef を確認
    ::Model* targetModel = modelRef;

    // 2. もし null なら、念のため親アクターから直接取ってみる (初期化ズレ対策)
    // (const関数なので modelRef の書き換えはせず、ローカル変数で扱う)
    if (!targetModel)
    {
        // const外し (GetActorが非constの場合に対応するため)
        auto* self = const_cast<AnimatorComponent*>(this);
        if (auto owner = self->GetActor())
        {
            // ここでモデルを取得 (GetModelRaw または GetModel().get() )
            targetModel = owner->GetModelRaw();
        }
    }

    // 3. それでもモデルがないならどうしようもない
    if (!targetModel) return names;

    // 4. アニメーションが空かチェック
    // (モデルはあるがアニメーションが入っていないFBXの場合)
    if (targetModel->GetAnimations().empty()) return names;

    // 5. リスト化
    for (const auto& anim : targetModel->GetAnimations())
    {
        names.push_back(anim.name);
    }

    return names;
}

int AnimatorComponent::GetAnimationIndexByName(const std::string& name) const
{
    if (!modelRef) return -1;

    // キャッシュにあればそれを返す（高速化）
    auto it = animNameCache.find(name);
    if (it != animNameCache.end())
    {
        return it->second;
    }

    // なければ全探索して探す
    const auto& anims = modelRef->GetAnimations();
    for (int i = 0; i < (int)anims.size(); ++i)
    {
        if (anims[i].name == name)
        {
            // 見つかったらキャッシュに保存して返す
            animNameCache[name] = i;
            return i;
        }
    }

    return -1; // 見つからない
}

void AnimatorComponent::Start()
{
    if (auto owner = GetActor()) {
        if (auto model = owner->GetModelRaw()) {
            modelRef = model;

            // ★In-Place化のため、移動成分を持つ0番を指定
            rootNodeIndex = 1;

            pelvisNodeIndex = model->GetNodeIndex("pelvis");
            spineNodeIndex = model->GetNodeIndex("spine_01");

            BuildBoneMask();

            size_t count = model->GetNodes().size();
            basePoses.resize(count);
            actionPoses.resize(count);
            tempPoses.resize(count);
            finalPoses.resize(count);

            // ★重要: 差分バッファの確保
            blendOffsets.resize(count);

            FillBindPose(finalPoses, modelRef);
        }
    }
}

void AnimatorComponent::BuildBoneMask()
{
    if (!modelRef) return;
    const auto& nodes = modelRef->GetNodes();
    isUpperBody.assign(nodes.size(), false);

    if (spineNodeIndex < 0) return;

    std::stack<int> stack;
    stack.push(spineNodeIndex);

    while (!stack.empty()) {
        int idx = stack.top();
        stack.pop();
        isUpperBody[idx] = true;
        const auto& node = nodes[idx];
        for (auto* child : node.children) {
            int childIdx = (int)(child - &nodes[0]);
            if (childIdx >= 0 && childIdx < (int)nodes.size()) {
                stack.push(childIdx);
            }
        }
    }
}

void AnimatorComponent::ForceRootResetXZ(std::vector<Model::NodePose>& poses)
{
    if (rootNodeIndex >= 0 && rootNodeIndex < (int)poses.size()) {
        poses[rootNodeIndex].position.x = 0.0f;
        // ★重要: ルートの高さも0に固定し、地面への吸い付きを保証する
        poses[rootNodeIndex].position.y = 0.0f;
        poses[rootNodeIndex].position.z = 0.0f;


    }
}

void AnimatorComponent::CaptureTransitionOffsets(const std::vector<Model::NodePose>& currentPose, int nextAnimIndex)
{
    if (!modelRef) return;

    FillBindPose(tempPoses, modelRef);
    modelRef->ComputeAnimation(nextAnimIndex, 0.0f, tempPoses);
    ForceRootResetXZ(tempPoses);

    size_t count = currentPose.size();
    for (size_t i = 0; i < count; ++i) {
        // 位置差分（Y無視はそのまま）
        XMVECTOR P_Curr = XMLoadFloat3(&currentPose[i].position);
        XMVECTOR P_Next = XMLoadFloat3(&tempPoses[i].position);
        XMVECTOR P_Diff = P_Curr - P_Next;
        P_Diff = XMVectorSetY(P_Diff, 0.0f);
        XMStoreFloat3(&blendOffsets[i].position, P_Diff);

        // 回転差分
        XMVECTOR Q_Curr = XMLoadFloat4(&currentPose[i].rotation);
        XMVECTOR Q_Next = XMLoadFloat4(&tempPoses[i].rotation);
        XMVECTOR Q_InvNext = XMQuaternionInverse(Q_Next);
        XMVECTOR Q_Diff = XMQuaternionMultiply(Q_InvNext, Q_Curr);

        // ★★★ 追加修正: ルートボーンの回転差分を「無し」にする ★★★
        // これで「揺れ戻し」の計算自体が行われなくなりますが、
        // モデルの基本姿勢（Bind Pose）は維持されるため、メッシュは壊れません。
        if (i == rootNodeIndex) {
            Q_Diff = XMQuaternionIdentity();
        }

        XMStoreFloat4(&blendOffsets[i].rotation, Q_Diff);

        // スケール
        XMVECTOR S_Curr = XMLoadFloat3(&currentPose[i].scale);
        XMVECTOR S_Next = XMLoadFloat3(&tempPoses[i].scale);
        XMStoreFloat3(&blendOffsets[i].scale, S_Curr - S_Next);
    }
}

void AnimatorComponent::UpdateOffsetBlending(float dt)
{
    if (!useOffsetBlending) return;

    offsetBlendTimer += dt;
    if (offsetBlendTimer >= offsetBlendDuration) {
        useOffsetBlending = false;
        offsetBlendTimer = 0.0f;
    }
}

void AnimatorComponent::ApplyOffsetBlending(std::vector<Model::NodePose>& poses)
{
    if (!useOffsetBlending) return;

    // 残り時間率 (0.0 -> 1.0)
    float t = offsetBlendTimer / offsetBlendDuration;
    if (t > 1.0f) t = 1.0f;

    // 減衰カーブ: 1.0(開始) -> 0.0(終了)
    // easeOutCubic などで「最初は粘り、最後はスッと消える」動きにする
    float decay = 1.0f - Easing::easeOutCubic(t);

    size_t count = poses.size();
    for (size_t i = 0; i < count; ++i) {
        // 位置: P_final = P_anim + P_offset * decay
        XMVECTOR P_Base = XMLoadFloat3(&poses[i].position);
        XMVECTOR P_Off = XMLoadFloat3(&blendOffsets[i].position);
        P_Base += P_Off * decay;
        XMStoreFloat3(&poses[i].position, P_Base);

        // 回転: Q_final = Q_anim * Slerp(Identity, Q_offset, decay)
        XMVECTOR Q_Base = XMLoadFloat4(&poses[i].rotation);
        XMVECTOR Q_Off = XMLoadFloat4(&blendOffsets[i].rotation);
        XMVECTOR Q_Identity = XMQuaternionIdentity();

        XMVECTOR Q_DecayedOff = XMQuaternionSlerp(Q_Identity, Q_Off, decay);
        XMVECTOR Q_Final = XMQuaternionMultiply(Q_Base, Q_DecayedOff);
        XMStoreFloat4(&poses[i].rotation, Q_Final);

        // スケール
        XMVECTOR S_Base = XMLoadFloat3(&poses[i].scale);
        XMVECTOR S_Off = XMLoadFloat3(&blendOffsets[i].scale);
        S_Base += S_Off * decay;
        XMStoreFloat3(&poses[i].scale, S_Base);
    }
}

void AnimatorComponent::PlayBase(int animIndex, bool loop, float blendTime, float speed)
{
    if (!modelRef) return;
    if (animIndex < 0) return;

    auto& L = baseLayer;
    if (L.currentAnimIndex == animIndex) {
        L.isLoop = loop;
        L.currentSpeed = speed;
        return;
    }

    // ★ここで差分ブレンドをセットアップ
    if (L.currentAnimIndex >= 0 && blendTime > 0.0f) {
        // 1. 現在表示されている最終ポーズ(finalPoses)と、次のアニメの開始差分を撮る
        CaptureTransitionOffsets(finalPoses, animIndex);

        // 2. ブレンド開始
        useOffsetBlending = true;
        offsetBlendDuration = blendTime;
        offsetBlendTimer = 0.0f;
    }
    else {
        useOffsetBlending = false;
    }

    // 3. アニメーション自体は「即座に」切り替える
    L.currentAnimIndex = animIndex;
    L.isLoop = loop;
    L.currentSpeed = speed;
    L.currentTime = 0.0f;
    L.weight = 1.0f;
}

void AnimatorComponent::PlayAction(int animIndex, bool loop, float blendTime, bool isFullBody)
{
    if (!modelRef) return;
    const auto& anims = modelRef->GetAnimations();
    if (animIndex < 0 || animIndex >= (int)anims.size()) return;

    auto& L = actionLayer;

    // 既に再生中ならクロスフェード準備
    bool isAlreadyPlaying = (L.currentAnimIndex >= 0 && L.weight > 0.01f);

    if (isAlreadyPlaying && blendTime > 0.0f) {
        L.prevAnimIndex = L.currentAnimIndex;
        L.prevAnimTime = L.currentTime;
        L.isBlending = true;
        L.blendDuration = blendTime;
        L.blendTimer = 0.0f;
    }
    else {
        L.isBlending = false;
    }

    L.currentAnimIndex = animIndex;
    L.isLoop = loop;
    L.currentSpeed = 1.0f;
    L.currentTime = 0.0f;
    L.weight = 1.0f;
    L.isFullBody = isFullBody;

    prevActionTime = 0.0f;
}

void AnimatorComponent::StopAction(float blendTime)
{
    // 即時停止
    actionLayer.weight = 0.0f;
    actionLayer.currentAnimIndex = -1;
}

void AnimatorComponent::SetActionTime(float time)
{
    if (!modelRef || actionLayer.currentAnimIndex < 0) return;
    prevActionTime = actionLayer.currentTime;
    actionLayer.currentTime = time;
    actionLayer.isBlending = false;
}
//void AnimatorComponent::Update(float dt)
//{
//    // 1. モデル取得 & 遅延初期化 (既存コード維持)
//    if (!modelRef) {
//        if (auto owner = GetActor()) {
//            if (auto m = owner->GetModelRaw()) {
//                modelRef = m;
//                Start();
//            }
//        }
//        if (!modelRef) return;
//    }
//
//    // 毎フレームリセット (既存コード維持)
//    rootMotionDelta = { 0,0,0 };
//
// 
//    float dt = dt;
//
//    if (currentDriver)
//    {
//        if (!currentDriver->AllowInternalUpdate())
//        {
//            dt = 0.0f;
//        }
//
//        int overrideIdx = currentDriver->GetOverrideAnimationIndex();
//        int animCount = (int)modelRef->GetAnimations().size();
//
//        if (overrideIdx >= 0 && overrideIdx < animCount)
//        {
//            if (actionLayer.currentAnimIndex != overrideIdx)
//            {
//                PlayAction(overrideIdx, true, 0.0f);
//            }
//        }
//
// 
//        float driverTime = currentDriver->GetTime();
//        SetActionTime(driverTime);
//    }
//
// 
//
//    float localPrevBaseTime = baseLayer.currentTime;
//
//    // Baseレイヤー更新
//    UpdateLayer(baseLayer, dt, true);
//    UpdateOffsetBlending(dt);
//
//    // Actionレイヤー更新
//    if (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f) {
//        UpdateLayer(actionLayer, dt, true);
//    }
//
//    bool hasAction = (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f);
//
//    // ルートモーション計算
//    if (hasAction && actionLayer.isFullBody) {
//        // ★重要: dt=0 (シーケンサー停止中) なら計算させない
//        // これで「停止中に地面を滑る」バグが消滅する
//        if (dt > 0.0001f) {
//            ComputeRootMotion(actionLayer, this->prevActionTime, actionLayer.currentTime);
//        }
//        // 時間更新は行う（次回フレームのため）
//        this->prevActionTime = actionLayer.currentTime;
//    }
//    else {
//        ComputeRootMotion(baseLayer, localPrevBaseTime, baseLayer.currentTime);
//    }
//
//    // --- ポーズ計算 (以降は既存コードと全く同じ) ---
//    if (baseLayer.currentAnimIndex >= 0) {
//        ComputeLayerPose(baseLayer, basePoses);
//    }
//    else {
//        FillBindPose(basePoses, modelRef);
//    }
//
//    if (hasAction) {
//        FillBindPose(actionPoses, modelRef);
//        // 範囲チェック
//        if (actionLayer.currentAnimIndex >= 0 && actionLayer.currentAnimIndex < (int)modelRef->GetAnimations().size()) {
//            modelRef->ComputeAnimation(actionLayer.currentAnimIndex, actionLayer.currentTime, actionPoses);
//        }
//        ForceRootResetXZ(actionPoses);
//
//        if (actionLayer.isBlending && actionLayer.prevAnimIndex >= 0) {
//            if (actionLayer.prevAnimIndex < (int)modelRef->GetAnimations().size()) {
//                FillBindPose(tempPoses, modelRef);
//                modelRef->ComputeAnimation(actionLayer.prevAnimIndex, actionLayer.prevAnimTime, tempPoses);
//                ForceRootResetXZ(tempPoses);
//
//                float t = (actionLayer.blendDuration > 0.0f) ? (actionLayer.blendTimer / actionLayer.blendDuration) : 1.0f;
//                if (t > 1.0f) t = 1.0f;
//                BlendPoses(tempPoses, actionPoses, t, actionPoses);
//            }
//        }
//    }
//
//    // --- 合成 ---
//    size_t count = finalPoses.size();
//    bool isSizeValid = (basePoses.size() == count);
//    if (hasAction && actionPoses.size() != count) isSizeValid = false;
//
//    if (isSizeValid)
//    {
//        for (size_t i = 0; i < count; ++i) {
//            finalPoses[i] = basePoses[i];
//            if (hasAction) {
//                if (actionLayer.isFullBody || isUpperBody[i]) {
//                    finalPoses[i] = actionPoses[i];
//                }
//            }
//        }
//    }
//
//    ApplyOffsetBlending(finalPoses);
//    ForceRootResetXZ(finalPoses);
//
//    modelRef->SetNodePoses(finalPoses);
//}

void AnimatorComponent::Update(float dt)
{
    // 1. モデル取得 & 遅延初期化 (既存コード維持)
    if (!modelRef) {
        if (auto owner = GetActor()) {
            if (auto m = owner->GetModelRaw()) {
                modelRef = m;
                Start();
            }
        }
        if (!modelRef) return;
    }

    // 毎フレームリセット (既存コード維持)
    rootMotionDelta = { 0,0,0 };



    // ドライバー（シーケンサー）による制御中
    if (currentDriver)
    {
        if (!currentDriver->AllowInternalUpdate())
        {
            dt = 0.0f; // 内部時間の進行を止める
        }

        int overrideIdx = currentDriver->GetOverrideAnimationIndex();
        int animCount = (int)modelRef->GetAnimations().size();

        // アニメーション切り替え判定
        if (overrideIdx >= 0 && overrideIdx < animCount)
        {
            if (actionLayer.currentAnimIndex != overrideIdx)
            {
                PlayAction(overrideIdx, true, 0.0f);
            }
        }

        // ドライバーから時間を取得
        float driverTime = currentDriver->GetTime();

        // ★追加: ループ制御ロジック
        // ドライバーが「ループしない(IsLoop == false)」と言っている場合、
        // アニメーションの長さを超えないように時間をクランプする
        if (overrideIdx >= 0 && overrideIdx < animCount)
        {
            // 現在再生中のアニメーションの長さを取得
            float duration = modelRef->GetAnimations()[overrideIdx].secondsLength;

            if (!currentDriver->IsLoop())
            {
                if (driverTime > duration)
                {
                    driverTime = duration; // 最後で止める
                }
            }
        }

        // 調整した時間をセット
        SetActionTime(driverTime);
    }


    float localPrevBaseTime = baseLayer.currentTime;

    // Baseレイヤー更新
    UpdateLayer(baseLayer, dt, true);
    UpdateOffsetBlending(dt);

    // Actionレイヤー更新
    if (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f) {
        UpdateLayer(actionLayer, dt, true);
    }

    bool hasAction = (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f);

    // ルートモーション計算
    if (hasAction && actionLayer.isFullBody) {
        // ★重要: dt=0 (シーケンサー停止中) なら計算させない
        // これで「停止中に地面を滑る」バグが消滅する
        if (dt > 0.0001f) {
            ComputeRootMotion(actionLayer, this->prevActionTime, actionLayer.currentTime);
        }
        // 時間更新は行う（次回フレームのため）
        this->prevActionTime = actionLayer.currentTime;
    }
    else {
        ComputeRootMotion(baseLayer, localPrevBaseTime, baseLayer.currentTime);
    }

    // --- ポーズ計算 (以降は既存コードと全く同じ) ---
    if (baseLayer.currentAnimIndex >= 0) {
        ComputeLayerPose(baseLayer, basePoses);
    }
    else {
        FillBindPose(basePoses, modelRef);
    }

    if (hasAction) {
        FillBindPose(actionPoses, modelRef);
        // 範囲チェック
        if (actionLayer.currentAnimIndex >= 0 && actionLayer.currentAnimIndex < (int)modelRef->GetAnimations().size()) {
            modelRef->ComputeAnimation(actionLayer.currentAnimIndex, actionLayer.currentTime, actionPoses);
        }
        ForceRootResetXZ(actionPoses);

        if (actionLayer.isBlending && actionLayer.prevAnimIndex >= 0) {
            if (actionLayer.prevAnimIndex < (int)modelRef->GetAnimations().size()) {
                FillBindPose(tempPoses, modelRef);
                modelRef->ComputeAnimation(actionLayer.prevAnimIndex, actionLayer.prevAnimTime, tempPoses);
                ForceRootResetXZ(tempPoses);

                float t = (actionLayer.blendDuration > 0.0f) ? (actionLayer.blendTimer / actionLayer.blendDuration) : 1.0f;
                if (t > 1.0f) t = 1.0f;
                BlendPoses(tempPoses, actionPoses, t, actionPoses);
            }
        }
    }

    // --- 合成 ---
    size_t count = finalPoses.size();
    bool isSizeValid = (basePoses.size() == count);
    if (hasAction && actionPoses.size() != count) isSizeValid = false;

    if (isSizeValid)
    {
        for (size_t i = 0; i < count; ++i) {
            finalPoses[i] = basePoses[i];
            if (hasAction) {
                if (actionLayer.isFullBody || isUpperBody[i]) {
                    finalPoses[i] = actionPoses[i];
                }
            }
        }
    }

    ApplyOffsetBlending(finalPoses);
    ForceRootResetXZ(finalPoses);

    modelRef->SetNodePoses(finalPoses);
}


void AnimatorComponent::UpdateLayer(AnimLayer& L, float dt, bool autoAdvance)
{
    if (L.currentAnimIndex < 0) return;
    const auto& anim = modelRef->GetAnimations()[L.currentAnimIndex];
    float maxTime = anim.secondsLength;

    if (autoAdvance) {
        L.currentTime += dt * L.currentSpeed;
        if (L.currentTime >= maxTime) {
            if (L.isLoop) L.currentTime = std::fmod(L.currentTime, maxTime);
            else L.currentTime = maxTime;
        }
    }

    if (L.isBlending) {
        L.blendTimer += dt;
        if (L.blendTimer >= L.blendDuration) {
            L.isBlending = false;
            L.blendTimer = 0.0f;
        }
    }
}

void AnimatorComponent::ComputeLayerPose(AnimLayer& L, std::vector<Model::NodePose>& out)
{
    FillBindPose(out, modelRef);
    modelRef->ComputeAnimation(L.currentAnimIndex, L.currentTime, out);
    ForceRootResetXZ(out);
}

void AnimatorComponent::BlendPoses(const std::vector<Model::NodePose>& src, const std::vector<Model::NodePose>& dst, float t, std::vector<Model::NodePose>& out)
{
    size_t count = src.size();
    for (size_t i = 0; i < count; ++i) {
        XMVECTOR P = XMVectorLerp(XMLoadFloat3(&src[i].position), XMLoadFloat3(&dst[i].position), t);
        if (i == (size_t)rootNodeIndex) {
            float dstY = dst[i].position.y;
            P = XMVectorSetY(P, dstY);
        }
        XMVECTOR R = XMQuaternionSlerp(XMLoadFloat4(&src[i].rotation), XMLoadFloat4(&dst[i].rotation), t);
        XMVECTOR S = XMVectorLerp(XMLoadFloat3(&src[i].scale), XMLoadFloat3(&dst[i].scale), t);
        XMStoreFloat3(&out[i].position, P);
        XMStoreFloat4(&out[i].rotation, R);
        XMStoreFloat3(&out[i].scale, S);
    }
}

DirectX::XMFLOAT3 AnimatorComponent::SampleRootPos(int animIndex, float time)
{
    // 安全装置1: インデックスチェック
    if (!modelRef || animIndex < 0 || rootNodeIndex < 0) return { 0,0,0 };

    // 安全装置2: 時間のクランプ (範囲外参照を防ぐ)
    const auto& anim = modelRef->GetAnimations()[animIndex];

    // std::max/min の代わりに if 文を使用
    float safeTime = time;
    if (safeTime < 0.0f)
    {
        safeTime = 0.0f;
    }
    else if (safeTime > anim.secondsLength)
    {
        safeTime = anim.secondsLength;
    }

    // 安全装置3: 初期化 (ゴミデータ対策)
    Model::NodePose pose = {};
    pose.scale = { 1,1,1 };
    pose.rotation = { 0,0,0,1 };

    // バインドポーズ（初期姿勢）を取得してセット
    if (rootNodeIndex < (int)modelRef->GetNodes().size()) {
        pose.position = modelRef->GetNodes()[rootNodeIndex].position;
    }

    // 計算実行
    modelRef->ComputeAnimation(animIndex, rootNodeIndex, safeTime, pose);

    return pose.position;
}

void AnimatorComponent::ComputeRootMotion(const AnimLayer& layer, float prevTime, float currTime)
{
    if (!enableRootMotion || layer.currentAnimIndex < 0) return;

    // 安全なサンプリング関数を使用
    XMVECTOR P_Prev = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, prevTime));
    XMVECTOR P_Curr = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, currTime));
    XMVECTOR V_Delta = P_Curr - P_Prev;

    // ループ対応
    if (currTime < prevTime)
    {
        float duration = modelRef->GetAnimations()[layer.currentAnimIndex].secondsLength;
        XMVECTOR P_End = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, duration));
        XMVECTOR P_Start = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, 0.0f));

        V_Delta = (P_End - P_Prev) + (P_Curr - P_Start);
    }

    // アクターの回転とスケールを適用
    if (auto owner = GetActor())
    {
        // 1. 回転の適用
        XMVECTOR Q_Actor = XMLoadFloat4(&owner->GetRotation());
        V_Delta = XMVector3Rotate(V_Delta, Q_Actor);

        // 2. スケールの適用 (重要: これがないと移動量が小さすぎる/大きすぎる)
        float scale = owner->GetScale().x;
        V_Delta *= scale * RootMotionScale;
    }

    // Y軸移動を無視する場合
    if (!bakeRootMotionY)
    {
        V_Delta = XMVectorSetY(V_Delta, 0.0f);
    }

    // ★安全装置4: 万が一のNaNチェック
    XMFLOAT3 delta;
    XMStoreFloat3(&delta, V_Delta);

    if (std::isnan(delta.x) || std::isnan(delta.z)) {
        rootMotionDelta = { 0,0,0 }; // 異常値なら動かさない
    }
    else {
        rootMotionDelta = delta;
    }
}

void AnimatorComponent::OnGUI()
{
    
        ImGui::Text("Base   Anim: %d", baseLayer.currentAnimIndex);
        ImGui::Text("Offset Blend: %s (%.2f / %.2f)", useOffsetBlending ? "ON" : "OFF", offsetBlendTimer, offsetBlendDuration);

        ImGui::Text("Root Delta: (%.4f, %.4f, %.4f)",
            rootMotionDelta.x, rootMotionDelta.y, rootMotionDelta.z);

    
}