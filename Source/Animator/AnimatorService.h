#pragma once

#include "Entity/Entity.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

// 前方宣言。
class Registry;
class Model;
class AnimatorRuntimeRegistry;
struct AnimatorComponent;

// アニメーション再生を entity 単位で管理するサービスクラス。
// AnimatorComponent の生成・削除、base/action 再生、driver 制御、
// アニメ名取得、root motion 取得などの窓口になる。
class AnimatorService
{
public:
    // singleton インスタンスを取得する。
    static AnimatorService& Instance();

    // このサービスが参照する ECS Registry を設定する。
    void SetRegistry(Registry* registry);

    // 現在参照している Registry を返す。
    Registry* GetRegistry() const { return m_registry; }

    // 指定 entity に AnimatorComponent が無ければ追加する。
    void EnsureAnimator(EntityID entity);

    // 指定 entity から AnimatorComponent を削除する。
    // 併せて runtime pose キャッシュも破棄する。
    void RemoveAnimator(EntityID entity);

    // base layer のアニメーションを再生する。
    // 待機や移動など、常時流れる基本モーション向け。
    void PlayBase(
        EntityID entity,
        int animIndex,
        bool loop = true,
        float blendTime = 0.2f,
        float speed = 1.0f);

    // action layer のアニメーションを再生する。
    // 攻撃や一時的な上書きアクション向け。
    void PlayAction(
        EntityID entity,
        int animIndex,
        bool loop = false,
        float blendTime = 0.1f,
        bool isFullBody = true);

    // action layer の再生を停止する。
    void StopAction(EntityID entity, float blendTime = 0.2f);

    // action layer の再生時刻を外部から直接設定する。
    void SetActionTime(EntityID entity, float time);

    // 外部 driver から animator を制御する。
    // Timeline や StateMachine から再生時刻や override animation を与える用途。
    void SetDriver(
        EntityID entity,
        float time,
        int overrideAnimIndex,
        bool loop,
        bool allowInternalUpdate);

    // 外部 driver との接続を解除する。
    void ClearDriver(EntityID entity);

    // 指定 entity のモデルが持つアニメーション名一覧を返す。
    std::vector<std::string> GetAnimationNameList(EntityID entity) const;

    // アニメーション名から対応する index を返す。
    // 見つからない場合は -1 を返す。
    int GetAnimationIndexByName(EntityID entity, const std::string& name) const;

    // 現在の root motion delta を返す。
    // AnimatorComponent が無い場合はゼロベクトルを返す。
    DirectX::XMFLOAT3 GetRootMotionDelta(EntityID entity) const;

    // entity ごとの runtime pose 情報を管理するレジストリを返す。
    AnimatorRuntimeRegistry& GetRuntimeRegistry() { return *m_runtimeRegistry; }

private:
    // singleton 用の private コンストラクタ。
    AnimatorService();

    // 指定 entity の AnimatorComponent を取得する。
    // 取れない場合は nullptr を返す。
    AnimatorComponent* GetAnimator(EntityID entity) const;

    // 指定 entity の MeshComponent から Model を取得する。
    // 取れない場合は nullptr を返す。
    Model* GetModel(EntityID entity) const;

private:
    // AnimatorService が参照する ECS Registry。
    Registry* m_registry = nullptr;

    // entity ごとの pose バッファや補間状態を保持する runtime registry。
    AnimatorRuntimeRegistry* m_runtimeRegistry = nullptr;
};