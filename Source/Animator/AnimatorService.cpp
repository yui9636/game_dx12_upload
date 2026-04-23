#include "AnimatorService.h"

#include "AnimatorComponent.h"
#include "AnimatorRuntime.h"
#include "Component/MeshComponent.h"
#include "Registry/Registry.h"

namespace
{
    // モデルの bind pose 相当の姿勢を poses に書き込む。
    // 各ノードの初期 position / rotation / scale をそのままコピーする。
    static void FillBindPose(std::vector<Model::NodePose>& poses, const Model* model)
    {
        // モデルが無いなら何もしない。
        if (!model) return;

        // ノード配列を取得する。
        const auto& nodes = model->GetNodes();

        // ノード数に合わせて pose 配列を確保する。
        poses.resize(nodes.size());

        // 各ノードの初期 transform を poses へコピーする。
        for (size_t i = 0; i < nodes.size(); ++i) {
            poses[i].position = nodes[i].position;
            poses[i].rotation = nodes[i].rotation;
            poses[i].scale = nodes[i].scale;
        }
    }

    // ルートノードの平行移動を強制的にゼロへ戻す。
    // transition 補間時に root のずれを抑え、足元が大きく飛ぶのを防ぐ。
    static void ForceRootResetXZ(std::vector<Model::NodePose>& poses, int rootNodeIndex)
    {
        // rootNodeIndex が有効範囲にある時だけ処理する。
        if (rootNodeIndex >= 0 && rootNodeIndex < static_cast<int>(poses.size())) {
            poses[rootNodeIndex].position.x = 0.0f;
            poses[rootNodeIndex].position.y = 0.0f;
            poses[rootNodeIndex].position.z = 0.0f;
        }
    }

    // 現在姿勢から次アニメ先頭姿勢への差分を取り、transition 用の blend offset を作る。
    // これによりアニメ切替時の姿勢の飛びを和らげる。
    static void CaptureTransitionOffsets(AnimatorRuntimeEntry& runtime, const std::vector<Model::NodePose>& currentPose, int nextAnimIndex)
    {
        // モデル参照が無いなら何もしない。
        if (!runtime.modelRef) return;

        // 一時バッファを bind pose で初期化する。
        FillBindPose(runtime.tempPoses, runtime.modelRef);

        // 次に再生するアニメーションの先頭姿勢を tempPoses に計算する。
        runtime.modelRef->ComputeAnimation(nextAnimIndex, 0.0f, runtime.tempPoses);

        // root の位置ずれを抑えるため、ルート位置をリセットする。
        ForceRootResetXZ(runtime.tempPoses, runtime.rootNodeIndex);

        // 現在姿勢と次姿勢との差分を各ノードごとに計算する。
        const size_t count = currentPose.size();
        for (size_t i = 0; i < count; ++i) {
            // 位置差分を計算する。
            const DirectX::XMVECTOR pCurr = DirectX::XMLoadFloat3(&currentPose[i].position);
            const DirectX::XMVECTOR pNext = DirectX::XMLoadFloat3(&runtime.tempPoses[i].position);
            DirectX::XMVECTOR pDiff = DirectX::XMVectorSubtract(pCurr, pNext);

            // Y成分をゼロにして、縦方向の急激な飛びを抑える。
            pDiff = DirectX::XMVectorSetY(pDiff, 0.0f);

            // 補間用オフセットへ保存する。
            DirectX::XMStoreFloat3(&runtime.blendOffsets[i].position, pDiff);

            // 回転差分をクォータニオンで計算する。
            const DirectX::XMVECTOR qCurr = DirectX::XMLoadFloat4(&currentPose[i].rotation);
            const DirectX::XMVECTOR qNext = DirectX::XMLoadFloat4(&runtime.tempPoses[i].rotation);
            DirectX::XMVECTOR qDiff = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(qNext), qCurr);

            // root は回転差分を持たせない。
            if (static_cast<int>(i) == runtime.rootNodeIndex) {
                qDiff = DirectX::XMQuaternionIdentity();
            }

            // 補間用オフセットへ保存する。
            DirectX::XMStoreFloat4(&runtime.blendOffsets[i].rotation, qDiff);

            // scale 差分を計算して保存する。
            const DirectX::XMVECTOR sCurr = DirectX::XMLoadFloat3(&currentPose[i].scale);
            const DirectX::XMVECTOR sNext = DirectX::XMLoadFloat3(&runtime.tempPoses[i].scale);
            DirectX::XMStoreFloat3(&runtime.blendOffsets[i].scale, DirectX::XMVectorSubtract(sCurr, sNext));
        }
    }
}

// AnimatorService の singleton インスタンスを返す。
AnimatorService& AnimatorService::Instance()
{
    static AnimatorService instance;
    return instance;
}

// AnimatorService のコンストラクタ。
// entity ごとの runtime pose 状態を保持する registry を生成する。
AnimatorService::AnimatorService()
{
    m_runtimeRegistry = new AnimatorRuntimeRegistry();
}

// AnimatorService が参照する ECS Registry を設定する。
void AnimatorService::SetRegistry(Registry* registry)
{
    m_registry = registry;
}

// 指定 entity に AnimatorComponent が無ければ追加する。
void AnimatorService::EnsureAnimator(EntityID entity)
{
    // registry が無い、entity が null、生存していない場合は処理しない。
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }

    // AnimatorComponent が無ければ追加する。
    if (!m_registry->GetComponent<AnimatorComponent>(entity)) {
        m_registry->AddComponent(entity, AnimatorComponent{});
    }
}

// 指定 entity から AnimatorComponent を除去し、runtime pose 情報も破棄する。
void AnimatorService::RemoveAnimator(EntityID entity)
{
    // registry が無い、entity が null、生存していない場合は処理しない。
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }

    // AnimatorComponent が存在するなら削除する。
    if (m_registry->GetComponent<AnimatorComponent>(entity)) {
        m_registry->RemoveComponent<AnimatorComponent>(entity);
    }

    // runtime pose 情報も削除する。
    m_runtimeRegistry->Remove(entity);
}

// base layer のアニメーションを再生する。
// locomotion や待機など、常時流れる基本姿勢に使う。
void AnimatorService::PlayBase(EntityID entity, int animIndex, bool loop, float blendTime, float speed)
{
    // AnimatorComponent を保証する。
    EnsureAnimator(entity);

    // AnimatorComponent を取得する。
    AnimatorComponent* animator = GetAnimator(entity);
    if (!animator) return;

    // 対応するモデルを取得する。
    Model* model = GetModel(entity);

    // モデルが無い、またはアニメーション index が不正なら終了する。
    if (!model || animIndex < 0 || animIndex >= static_cast<int>(model->GetAnimations().size())) {
        return;
    }

    // base layer を参照する。
    auto& layer = animator->baseLayer;

    // 既に同じアニメーションなら、loop と speed だけ更新して終わる。
    if (layer.currentAnimIndex == animIndex) {
        layer.isLoop = loop;
        layer.currentSpeed = speed;
        return;
    }

    // 既に姿勢が存在し、blendTime が正なら transition 補間用オフセットを取得する。
    if (auto* runtime = m_runtimeRegistry->Find(entity); runtime && layer.currentAnimIndex >= 0 && blendTime > 0.0f && !runtime->finalPoses.empty()) {
        CaptureTransitionOffsets(*runtime, runtime->finalPoses, animIndex);
        runtime->useOffsetBlending = true;
        runtime->offsetBlendDuration = blendTime;
        runtime->offsetBlendTimer = 0.0f;
    }

    // 新しい base animation を設定する。
    layer.currentAnimIndex = animIndex;
    layer.currentTime = 0.0f;
    layer.currentSpeed = speed;
    layer.isLoop = loop;
    layer.weight = 1.0f;
}

// action layer のアニメーションを再生する。
// 攻撃や上半身アクションなど、一時的に重ねる動作に使う。
void AnimatorService::PlayAction(EntityID entity, int animIndex, bool loop, float blendTime, bool isFullBody)
{
    // AnimatorComponent を保証する。
    EnsureAnimator(entity);

    // AnimatorComponent とモデルを取得する。
    AnimatorComponent* animator = GetAnimator(entity);
    Model* model = GetModel(entity);

    // どちらか無い、またはアニメーション index が不正なら終了する。
    if (!animator || !model || animIndex < 0 || animIndex >= static_cast<int>(model->GetAnimations().size())) {
        return;
    }

    // action layer を参照する。
    auto& layer = animator->actionLayer;

    // 既に action 再生中なら、前回アニメを保存して blend 開始状態へ入る。
    if (layer.currentAnimIndex >= 0 && layer.weight > 0.01f && blendTime > 0.0f) {
        layer.prevAnimIndex = layer.currentAnimIndex;
        layer.prevAnimTime = layer.currentTime;
        layer.isBlending = true;
        layer.blendDuration = blendTime;
        layer.blendTimer = 0.0f;
    }
    else {
        // 既存再生が無いなら blend はしない。
        layer.isBlending = false;
    }

    // 新しい action animation を設定する。
    layer.currentAnimIndex = animIndex;
    layer.currentTime = 0.0f;
    layer.currentSpeed = 1.0f;
    layer.isLoop = loop;
    layer.weight = 1.0f;
    layer.isFullBody = isFullBody;

    // runtime 側の前回 action 時刻もリセットする。
    if (auto* runtime = m_runtimeRegistry->Find(entity)) {
        runtime->prevActionTime = 0.0f;
    }
}

// action layer の再生を停止する。
void AnimatorService::StopAction(EntityID entity, float)
{
    // AnimatorComponent が取れたら action layer を初期状態へ戻す。
    if (auto* animator = GetAnimator(entity)) {
        animator->actionLayer.weight = 0.0f;
        animator->actionLayer.currentAnimIndex = -1;
        animator->actionLayer.currentTime = 0.0f;
        animator->actionLayer.isBlending = false;
    }
}

// action layer の再生時刻を外部から直接設定する。
void AnimatorService::SetActionTime(EntityID entity, float time)
{
    // AnimatorComponent が取れたら action layer の再生位置を変更する。
    if (auto* animator = GetAnimator(entity)) {
        animator->actionLayer.currentTime = time;
        animator->actionLayer.isBlending = false;
    }
}

// 外部ドライバから animator を制御するための設定を行う。
// Timeline や StateMachine などから、再生時刻や override animation を与える用途。
void AnimatorService::SetDriver(EntityID entity, float time, int overrideAnimIndex, bool loop, bool allowInternalUpdate)
{
    // AnimatorComponent を保証する。
    EnsureAnimator(entity);

    // AnimatorComponent が取れたら driver 設定を書き込む。
    if (auto* animator = GetAnimator(entity)) {
        animator->driverConnected = true;
        animator->driverTime = time;
        animator->driverOverrideAnimIndex = overrideAnimIndex;
        animator->driverLoop = loop;
        animator->driverAllowInternalUpdate = allowInternalUpdate;
    }
}

// 外部 driver との接続を解除する。
void AnimatorService::ClearDriver(EntityID entity)
{
    // AnimatorComponent が取れたら driver 情報を初期化する。
    if (auto* animator = GetAnimator(entity)) {
        animator->driverConnected = false;
        animator->driverTime = 0.0f;
        animator->driverOverrideAnimIndex = -1;
        animator->driverLoop = false;
        animator->driverAllowInternalUpdate = false;
    }
}

// entity が持つモデルから、アニメーション名一覧を取得する。
std::vector<std::string> AnimatorService::GetAnimationNameList(EntityID entity) const
{
    std::vector<std::string> names;

    // モデルが取れなければ空配列を返す。
    Model* model = GetModel(entity);
    if (!model) return names;

    // モデルが持つ全アニメーション名を列挙する。
    for (const auto& anim : model->GetAnimations()) {
        names.push_back(anim.name);
    }

    return names;
}

// アニメーション名から対応 index を取得する。
int AnimatorService::GetAnimationIndexByName(EntityID entity, const std::string& name) const
{
    // モデルが無ければ失敗。
    Model* model = GetModel(entity);
    if (!model) return -1;

    // モデルへ問い合わせて index を返す。
    return model->GetAnimationIndex(name.c_str());
}

// 現在の root motion delta を取得する。
DirectX::XMFLOAT3 AnimatorService::GetRootMotionDelta(EntityID entity) const
{
    // AnimatorComponent があれば、その root motion delta を返す。
    if (const AnimatorComponent* animator = GetAnimator(entity)) {
        return animator->rootMotionDelta;
    }

    // 無ければゼロベクトルを返す。
    return { 0.0f, 0.0f, 0.0f };
}

// entity から AnimatorComponent を取得する。
AnimatorComponent* AnimatorService::GetAnimator(EntityID entity) const
{
    // registry が無い、entity が無効、生存していないなら nullptr。
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return nullptr;
    }

    // AnimatorComponent を返す。
    return m_registry->GetComponent<AnimatorComponent>(entity);
}

// entity から MeshComponent 経由で Model を取得する。
Model* AnimatorService::GetModel(EntityID entity) const
{
    // registry が無い、entity が無効、生存していないなら nullptr。
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return nullptr;
    }

    // MeshComponent が存在し、model が有効なら生ポインタを返す。
    if (auto* mesh = m_registry->GetComponent<MeshComponent>(entity); mesh && mesh->model) {
        return mesh->model.get();
    }

    // モデルが無ければ nullptr。
    return nullptr;
}