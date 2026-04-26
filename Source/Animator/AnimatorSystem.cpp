#include "AnimatorSystem.h"

#include "AnimatorComponent.h"
#include "AnimatorRuntime.h"
#include "AnimatorService.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include <algorithm>
#include <cmath>
#include "Component/HierarchyComponent.h"
#include "Component/PhysicsComponent.h"
#include "Physics/PhysicsManager.h"
#include <Jolt/Physics/Body/BodyInterface.h>

using namespace DirectX;

namespace
{
    // モデルの bind pose 相当の初期姿勢を poses に書き込む。
    // 各ノードの position / rotation / scale をそのままコピーする。
    static void FillBindPose(std::vector<Model::NodePose>& poses, const Model* model)
    {
        // モデルが無いなら何もしない。
        if (!model) return;

        // モデルが持つノード一覧を取得する。
        const auto& nodes = model->GetNodes();

        // ノード数に合わせて pose 配列を確保する。
        poses.resize(nodes.size());

        // 各ノードの初期 transform を pose 配列へコピーする。
        for (size_t i = 0; i < nodes.size(); ++i) {
            poses[i].position = nodes[i].position;
            poses[i].rotation = nodes[i].rotation;
            poses[i].scale = nodes[i].scale;
        }
    }

    // ルートノードの位置を強制的に原点へ戻す。
    // root motion を別管理する構成で、描画姿勢の root 平行移動を抑えたい時に使う。
    static void ForceRootResetXZ(std::vector<Model::NodePose>& poses, int rootNodeIndex)
    {
        // root index が有効範囲なら、そのノードの位置をゼロにする。
        if (rootNodeIndex >= 0 && rootNodeIndex < static_cast<int>(poses.size())) {
            poses[rootNodeIndex].position.x = 0.0f;
            poses[rootNodeIndex].position.y = 0.0f;
            poses[rootNodeIndex].position.z = 0.0f;
        }
    }

    // transition 補間中のオフセットブレンド時間を進める。
    // 規定時間を超えたらオフセットブレンドを終了する。
    static void UpdateOffsetBlending(AnimatorRuntimeEntry& runtime, float dt)
    {
        // 現在オフセットブレンド中でなければ何もしない。
        if (!runtime.useOffsetBlending) return;

        // 経過時間を進める。
        runtime.offsetBlendTimer += dt;

        // 補間時間を超えたらブレンド終了とする。
        if (runtime.offsetBlendTimer >= runtime.offsetBlendDuration) {
            runtime.useOffsetBlending = false;
            runtime.offsetBlendTimer = 0.0f;
        }
    }

    // transition 開始時にキャプチャしておいた姿勢差分を poses へ適用する。
    // これにより、アニメ切り替え時の急な姿勢ジャンプを和らげる。
    static void ApplyOffsetBlending(AnimatorRuntimeEntry& runtime, std::vector<Model::NodePose>& poses)
    {
        // オフセットブレンド中でない、または時間が無効なら何もしない。
        if (!runtime.useOffsetBlending || runtime.offsetBlendDuration <= 0.0f) return;

        // 進行率を 0～1 に正規化する。
        float t = runtime.offsetBlendTimer / runtime.offsetBlendDuration;
        t = (std::clamp)(t, 0.0f, 1.0f);

        // 減衰カーブを作る。
        // 単純な線形ではなく、終わり側でゆるやかに収束する形にしている。
        const float decay = 1.0f - (1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t));

        // すべてのノードへオフセットを適用する。
        for (size_t i = 0; i < poses.size(); ++i) {
            // 位置オフセットを加算する。
            XMVECTOR p = XMLoadFloat3(&poses[i].position) + XMLoadFloat3(&runtime.blendOffsets[i].position) * decay;
            XMStoreFloat3(&poses[i].position, p);

            // 回転オフセットをクォータニオン補間で乗算する。
            XMVECTOR qBase = XMLoadFloat4(&poses[i].rotation);
            XMVECTOR qOff = XMLoadFloat4(&runtime.blendOffsets[i].rotation);
            XMVECTOR q = XMQuaternionMultiply(qBase, XMQuaternionSlerp(XMQuaternionIdentity(), qOff, decay));
            XMStoreFloat4(&poses[i].rotation, q);

            // スケールオフセットを加算する。
            XMVECTOR s = XMLoadFloat3(&poses[i].scale) + XMLoadFloat3(&runtime.blendOffsets[i].scale) * decay;
            XMStoreFloat3(&poses[i].scale, s);
        }
    }

    // 1つのアニメーション layer の再生時間や blend 状態を更新する。
    static void UpdateLayer(Model* model, AnimatorComponent::LayerState& layer, float dt, bool autoAdvance)
    {
        // モデルが無い、または現在アニメ index が不正なら何もしない。
        if (!model || layer.currentAnimIndex < 0 || layer.currentAnimIndex >= static_cast<int>(model->GetAnimations().size())) {
            return;
        }

        // 現在アニメの長さを取得する。
        const auto& anim = model->GetAnimations()[layer.currentAnimIndex];
        const float maxTime = anim.secondsLength;

        // 自動進行する設定なら再生時間を進める。
        if (autoAdvance) {
            layer.currentTime += dt * layer.currentSpeed;

            // アニメ終端に達したら loop 有無に応じて調整する。
            if (layer.currentTime >= maxTime) {
                layer.currentTime = layer.isLoop ? std::fmod(layer.currentTime, maxTime) : maxTime;
            }
        }

        // layer blend 中なら blend timer を進める。
        if (layer.isBlending) {
            layer.blendTimer += dt;

            // blend 時間を超えたら blending を終了する。
            if (layer.blendTimer >= layer.blendDuration) {
                layer.isBlending = false;
                layer.blendTimer = 0.0f;
            }
        }
    }

    // 指定 layer の現在時刻から pose を計算する。
    static void ComputeLayerPose(Model* model, int rootNodeIndex, const AnimatorComponent::LayerState& layer, std::vector<Model::NodePose>& out)
    {
        // まず bind pose で初期化する。
        FillBindPose(out, model);

        // 有効なアニメが設定されていれば、その時刻の pose を計算する。
        if (layer.currentAnimIndex >= 0 && layer.currentAnimIndex < static_cast<int>(model->GetAnimations().size())) {
            model->ComputeAnimation(layer.currentAnimIndex, layer.currentTime, out);
        }

        // root の位置をゼロに戻す。
        ForceRootResetXZ(out, rootNodeIndex);
    }

    // 2つの pose 配列を t で補間し、out へ書き込む。
    static void BlendPoses(const std::vector<Model::NodePose>& src, const std::vector<Model::NodePose>& dst, float t, int rootNodeIndex, std::vector<Model::NodePose>& out)
    {
        const size_t count = src.size();
        out.resize(count);

        // 各ノードの position / rotation / scale を補間する。
        for (size_t i = 0; i < count; ++i) {
            // 位置は線形補間する。
            XMVECTOR p = XMVectorLerp(XMLoadFloat3(&src[i].position), XMLoadFloat3(&dst[i].position), t);

            // root は Y を dst 側に寄せる。
            if (static_cast<int>(i) == rootNodeIndex) {
                p = XMVectorSetY(p, dst[i].position.y);
            }

            XMStoreFloat3(&out[i].position, p);

            // 回転は球面線形補間する。
            XMStoreFloat4(&out[i].rotation, XMQuaternionSlerp(XMLoadFloat4(&src[i].rotation), XMLoadFloat4(&dst[i].rotation), t));

            // スケールは線形補間する。
            XMStoreFloat3(&out[i].scale, XMVectorLerp(XMLoadFloat3(&src[i].scale), XMLoadFloat3(&dst[i].scale), t));
        }
    }

    // 指定アニメーション・指定時刻における root ノード位置を取得する。
    static XMFLOAT3 SampleRootPos(Model* model, int rootNodeIndex, int animIndex, float time)
    {
        // モデルや index が不正ならゼロを返す。
        if (!model || animIndex < 0 || rootNodeIndex < 0 || rootNodeIndex >= static_cast<int>(model->GetNodes().size())) {
            return { 0.0f, 0.0f, 0.0f };
        }

        // アニメ長を取得し、時刻を安全範囲へ丸める。
        const auto& anim = model->GetAnimations()[animIndex];
        const float safeTime = (std::clamp)(time, 0.0f, anim.secondsLength);

        // root 用の pose を初期値で作る。
        Model::NodePose pose{};
        pose.position = model->GetNodes()[rootNodeIndex].position;
        pose.rotation = { 0, 0, 0, 1 };
        pose.scale = { 1, 1, 1 };

        // 指定時刻の root pose を計算する。
        model->ComputeAnimation(animIndex, rootNodeIndex, safeTime, pose);
        return pose.position;
    }

    // 指定 layer の前フレームと今フレームの root 位置差から root motion delta を計算する。
    static void ComputeRootMotion(AnimatorComponent& animator, AnimatorRuntimeEntry& runtime, const TransformComponent& transform, const AnimatorComponent::LayerState& layer, float prevTime, float currTime)
    {
        // 毎回最初にゼロ初期化する。
        animator.rootMotionDelta = { 0.0f, 0.0f, 0.0f };

        // root motion 無効、モデル無し、アニメ未設定なら終了する。
        if (!animator.enableRootMotion || !runtime.modelRef || layer.currentAnimIndex < 0) {
            return;
        }

        // 現在時刻と前時刻の root 位置差を計算する。
        XMVECTOR delta = XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, currTime)) -
            XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, prevTime));

        // ループで時間が巻き戻った場合は、
        // 終端までの差分 + 先頭から現在時刻までの差分に分けて計算する。
        if (currTime < prevTime) {
            const float duration = runtime.modelRef->GetAnimations()[layer.currentAnimIndex].secondsLength;
            delta =
                (XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, duration)) -
                    XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, prevTime))) +
                (XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, currTime)) -
                    XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, 0.0f)));
        }

        // root motion を現在の worldRotation に合わせて回転させる。
        delta = XMVector3Rotate(delta, XMLoadFloat4(&transform.worldRotation));

        // worldScale と rootMotionScale を掛ける。
        delta *= transform.worldScale.x * animator.rootMotionScale;

        // Y baked 無効なら上下移動を捨てる。
        if (!animator.bakeRootMotionY) {
            delta = XMVectorSetY(delta, 0.0f);
        }

        // 計算結果を保存する。
        XMStoreFloat3(&animator.rootMotionDelta, delta);

        // NaN が出たら安全のためゼロへ戻す。
        if (std::isnan(animator.rootMotionDelta.x) || std::isnan(animator.rootMotionDelta.z)) {
            animator.rootMotionDelta = { 0.0f, 0.0f, 0.0f };
        }
    }

    // root motion delta がほぼゼロかどうかを判定する。
// 小さすぎる移動で Transform / PhysicsBody を更新し続けないために使う。
    static bool IsNearlyZeroRootMotionDelta(const DirectX::XMFLOAT3& delta)
    {
        const float epsilon = 0.000001f;

        if (std::fabs(delta.x) > epsilon) {
            return false;
        }

        if (std::fabs(delta.y) > epsilon) {
            return false;
        }

        if (std::fabs(delta.z) > epsilon) {
            return false;
        }

        return true;
    }

    // world 座標の位置を、親がある場合だけ local 座標へ変換する。
    static DirectX::XMVECTOR ConvertWorldPositionToLocalPosition(
        Registry& registry,
        EntityID entity,
        DirectX::XMVECTOR worldPosition)
    {
        using namespace DirectX;

        HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        if (!hierarchy || Entity::IsNull(hierarchy->parent)) {
            return worldPosition;
        }

        TransformComponent* parentTransform = registry.GetComponent<TransformComponent>(hierarchy->parent);
        if (!parentTransform) {
            return worldPosition;
        }

        const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
        const XMMATRIX inverseParentWorld = XMMatrixInverse(nullptr, parentWorld);

        return XMVector3TransformCoord(worldPosition, inverseParentWorld);
    }

    // world delta を、親がある場合だけ local delta へ変換する。
    static DirectX::XMVECTOR ConvertWorldDeltaToLocalDelta(
        Registry& registry,
        EntityID entity,
        DirectX::XMVECTOR worldDelta)
    {
        using namespace DirectX;

        HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        if (!hierarchy || Entity::IsNull(hierarchy->parent)) {
            return worldDelta;
        }

        TransformComponent* parentTransform = registry.GetComponent<TransformComponent>(hierarchy->parent);
        if (!parentTransform) {
            return worldDelta;
        }

        const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
        const XMMATRIX inverseParentWorld = XMMatrixInverse(nullptr, parentWorld);

        return XMVector3TransformNormal(worldDelta, inverseParentWorld);
    }

    // root motion delta を、この entity の実際の移動元へ反映する。
    // PhysicsComponent がある場合は physics body を正とし、Transform は body の新位置へ同期する。
    // PhysicsComponent が無い場合だけ Transform に直接加算する。
    static void ApplyRootMotionToMotionOwner(
        Registry& registry,
        EntityID entity,
        TransformComponent& transform,
        const AnimatorComponent& animator)
    {
        using namespace DirectX;

        if (!animator.enableRootMotion) {
            return;
        }

        if (IsNearlyZeroRootMotionDelta(animator.rootMotionDelta)) {
            return;
        }

        PhysicsComponent* physics = registry.GetComponent<PhysicsComponent>(entity);
        if (physics && !physics->bodyID.IsInvalid()) {
            JPH::BodyInterface& bodyInterface = PhysicsManager::Instance().GetBodyInterface();

            const JPH::RVec3 currentBodyPosition = bodyInterface.GetPosition(physics->bodyID);
            const JPH::Quat currentBodyRotation = bodyInterface.GetRotation(physics->bodyID);

            const JPH::RVec3 nextBodyPosition(
                currentBodyPosition.GetX() + static_cast<double>(animator.rootMotionDelta.x),
                currentBodyPosition.GetY() + static_cast<double>(animator.rootMotionDelta.y),
                currentBodyPosition.GetZ() + static_cast<double>(animator.rootMotionDelta.z));

            bodyInterface.SetPositionAndRotation(
                physics->bodyID,
                nextBodyPosition,
                currentBodyRotation,
                JPH::EActivation::Activate);

            const XMVECTOR nextWorldPosition = XMVectorSet(
                static_cast<float>(nextBodyPosition.GetX()),
                static_cast<float>(nextBodyPosition.GetY()),
                static_cast<float>(nextBodyPosition.GetZ()),
                1.0f);

            const XMVECTOR nextLocalPosition = ConvertWorldPositionToLocalPosition(
                registry,
                entity,
                nextWorldPosition);

            XMStoreFloat3(&transform.localPosition, nextLocalPosition);
            transform.isDirty = true;
            return;
        }

        const XMVECTOR worldDelta = XMLoadFloat3(&animator.rootMotionDelta);
        const XMVECTOR localDelta = ConvertWorldDeltaToLocalDelta(registry, entity, worldDelta);

        XMVECTOR localPosition = XMLoadFloat3(&transform.localPosition);
        localPosition += localDelta;

        XMStoreFloat3(&transform.localPosition, localPosition);
        transform.isDirty = true;
    }




}

// AnimatorSystem の毎フレーム更新。
// AnimatorComponent を持つ entity について pose 計算と root motion 更新を行う。
void AnimatorSystem::Update(Registry& registry, float dt)
{
    // AnimatorService に現在の registry を設定する。
    AnimatorService::Instance().SetRegistry(&registry);

    // runtime pose 情報の管理レジストリを取得する。
    AnimatorRuntimeRegistry& runtimeRegistry = AnimatorService::Instance().GetRuntimeRegistry();

    // Mesh / Transform / Animator を持つ entity を列挙する。
    Query<MeshComponent, TransformComponent, AnimatorComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, MeshComponent& mesh, TransformComponent& transform, AnimatorComponent& animator) {
        // モデルが無ければ runtime 情報を消して終了する。
        if (!mesh.model) {
            runtimeRegistry.Remove(entity);
            return;
        }

        // entity 用 runtime entry を取得または生成する。
        AnimatorRuntimeEntry& runtime = runtimeRegistry.Ensure(entity, mesh.model.get());

        // runtime entry からモデル参照を取得する。
        Model* model = runtime.modelRef;
        if (!model) {
            return;
        }

        // 通常は dt で更新する。
        float updateDt = dt;

        // driver 接続中は driver 設定を優先する。
        if (animator.driverConnected) {
            // driver が内部更新を禁止しているなら時間更新を止める。
            if (!animator.driverAllowInternalUpdate) {
                updateDt = 0.0f;
            }

            // override 用アニメ index を取得する。
            const int overrideIdx = animator.driverOverrideAnimIndex;
            const int animCount = static_cast<int>(model->GetAnimations().size());

            // override index が有効なら action layer に反映する。
            if (overrideIdx >= 0 && overrideIdx < animCount) {
                // まだ違うアニメなら PlayAction で切り替える。
                if (animator.actionLayer.currentAnimIndex != overrideIdx) {
                    AnimatorService::Instance().PlayAction(entity, overrideIdx, animator.driverLoop, 0.0f, true);
                }

                // driver 時刻を action layer に直接反映する。
                float driverTime = animator.driverTime;
                if (!animator.driverLoop) {
                    driverTime = (std::min)(driverTime, model->GetAnimations()[overrideIdx].secondsLength);
                }

                animator.actionLayer.currentTime = driverTime;
                animator.actionLayer.isLoop = animator.driverLoop;
                animator.actionLayer.isBlending = false;
            }
        }

        // base layer の前フレーム時刻を保存する。
        float prevBaseTime = animator.baseLayer.currentTime;

        // PlaybackComponent があれば、それを base layer の再生値として使う。
        if (auto* playback = registry.GetComponent<PlaybackComponent>(entity)) {
            animator.baseLayer.currentTime = playback->currentSeconds;
            animator.baseLayer.currentSpeed = playback->playSpeed;
            animator.baseLayer.isLoop = playback->looping;
        }
        else {
            // 無ければ通常の内部更新で進める。
            UpdateLayer(model, animator.baseLayer, updateDt, true);
        }

        // transition 用オフセットブレンド時間を更新する。
        UpdateOffsetBlending(runtime, updateDt);

        // action layer が有効ならそれも更新する。
        if (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.weight > 0.0f) {
            UpdateLayer(model, animator.actionLayer, updateDt, true);
        }

        // action layer が実際に有効か判定する。
        const bool hasAction = (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.weight > 0.0f);

        // full body action 中は action layer の root motion を優先する。
        if (hasAction && animator.actionLayer.isFullBody) {
            if (updateDt > 0.0001f) {
                ComputeRootMotion(animator, runtime, transform, animator.actionLayer, runtime.prevActionTime, animator.actionLayer.currentTime);
            }
            runtime.prevActionTime = animator.actionLayer.currentTime;
        }
        else {
            // それ以外は base layer の root motion を使う。
            ComputeRootMotion(animator, runtime, transform, animator.baseLayer, prevBaseTime, animator.baseLayer.currentTime);
        }

        ApplyRootMotionToMotionOwner(registry, entity, transform, animator);

        // base layer の pose を計算する。
        if (animator.baseLayer.currentAnimIndex >= 0) {
            ComputeLayerPose(model, runtime.rootNodeIndex, animator.baseLayer, runtime.basePoses);
        }
        else {
            // base アニメが無ければ bind pose にする。
            FillBindPose(runtime.basePoses, model);
        }

        // action layer がある場合は action pose を計算する。
        if (hasAction) {
            FillBindPose(runtime.actionPoses, model);

            if (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.currentAnimIndex < static_cast<int>(model->GetAnimations().size())) {
                model->ComputeAnimation(animator.actionLayer.currentAnimIndex, animator.actionLayer.currentTime, runtime.actionPoses);
            }

            ForceRootResetXZ(runtime.actionPoses, runtime.rootNodeIndex);

            // action layer が blend 中なら前アニメと現在アニメを補間する。
            if (animator.actionLayer.isBlending && animator.actionLayer.prevAnimIndex >= 0 &&
                animator.actionLayer.prevAnimIndex < static_cast<int>(model->GetAnimations().size())) {
                FillBindPose(runtime.tempPoses, model);
                model->ComputeAnimation(animator.actionLayer.prevAnimIndex, animator.actionLayer.prevAnimTime, runtime.tempPoses);
                ForceRootResetXZ(runtime.tempPoses, runtime.rootNodeIndex);

                const float t = animator.actionLayer.blendDuration > 0.0f
                    ? (std::min)(1.0f, animator.actionLayer.blendTimer / animator.actionLayer.blendDuration)
                    : 1.0f;

                BlendPoses(runtime.tempPoses, runtime.actionPoses, t, runtime.rootNodeIndex, runtime.actionPoses);
            }
        }

        // 最終 pose を組み立てる。
        const size_t count = runtime.finalPoses.size();
        if (runtime.basePoses.size() != count) {
            // pose 数が崩れているなら bind pose へ戻す。
            FillBindPose(runtime.finalPoses, model);
        }
        else {
            // まず base pose を最終姿勢に入れる。
            for (size_t i = 0; i < count; ++i) {
                runtime.finalPoses[i] = runtime.basePoses[i];

                // action がある場合、full body または upper body 対象ノードだけ action pose を上書きする。
                if (hasAction && i < runtime.isUpperBody.size()) {
                    if (animator.actionLayer.isFullBody || runtime.isUpperBody[i]) {
                        runtime.finalPoses[i] = runtime.actionPoses[i];
                    }
                }
            }
        }

        // transition 用オフセットを加算する。
        ApplyOffsetBlending(runtime, runtime.finalPoses);

        // 最終姿勢でも root をリセットする。
        ForceRootResetXZ(runtime.finalPoses, runtime.rootNodeIndex);

        // 計算済み pose をモデルへ反映する。
        model->SetNodePoses(runtime.finalPoses);
        });
}