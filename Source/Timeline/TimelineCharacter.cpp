#include "TimelineCharacter.h"
#include "Runner/RunnerComponent.h"
#include "Actor/Actor.h"
#include "Collision/ColliderComponent.h"
#include "Model.h"

// マネージャー群
#include "Effect/EffectManager.h"
#include "Effect/EffectNode.h"
#include "Effect/EffectLoader.h"
#include "Audio/Audio.h"
#include "Audio/AudioSource.h"

#include <cmath>

using namespace DirectX;

#define PI 3.14159265358979323846f
#define DEG_TO_RAD (PI / 180.0f)

void TimelineCharacter::Start()
{
    // 必要なコンポーネントを取得しておく
    auto owner = GetActor();
    if (owner) {
        collider = owner->GetComponent<ColliderComponent>();
        if (!runner) {
            runner = owner->GetComponent<RunnerComponent>();
        }
    }
}

void TimelineCharacter::SetGameplayData(const GameplayAsset& data)
{
    gameplayData = data;
}

void TimelineCharacter::SetRunner(std::shared_ptr<RunnerComponent> r)
{
    runner = r;
}

// アニメーション切り替え (PlayActionなどで呼ばれる想定)
void TimelineCharacter::OnAnimationChange(int newAnimIndex)
{
    // 1. 前のアクションの後始末
    for (auto& item : activeItems)
    {
        if (item.vfxInstance) item.vfxInstance->Stop(true);
        if (item.audioSource) item.audioSource->Stop();
    }
    activeItems.clear();

    currentAnimIndex = newAnimIndex;

    // 2. 新しいデータをセット
    if (newAnimIndex >= 0 && newAnimIndex < (int)gameplayData.timelines.size())
    {
        // データをコピーして、このアクション専用の作業領域とする
        activeItems = gameplayData.timelines[newAnimIndex];
    }

    // 3. カーブ設定の適用 (Runnerへ)
    if (runner && newAnimIndex >= 0 && newAnimIndex < (int)gameplayData.curves.size())
    {
        const auto& curve = gameplayData.curves[newAnimIndex];
        runner->SetSpeedCurveEnabled(curve.enabled);
        runner->SetSpeedCurveUseRangeSpace(curve.useRange);

        std::vector<RunnerComponent::CurvePoint> pts;
        for (const auto& p : curve.points) {
            pts.push_back({ p.x, p.y });
        }
        runner->SetSpeedCurvePoints(pts);
    }
    else if (runner)
    {
        runner->SetSpeedCurveEnabled(false);
    }
}

void TimelineCharacter::Update(float elapsedTime)
{
    if (!runner) return;
    auto owner = GetActor();
    if (!owner) return;

    // 現在時間
    float timeSec = runner->GetTimeSeconds();
    int frameNow = SecondsToFrames(timeSec);

    // 1. Hitbox同期 (Colliderコンポーネント側で処理)
    // ※ColliderComponent::SyncFromSequencer は「GESequencerItemのリスト」ではなく
    // 「TimelineSequencerComponent*」を引数に取る設計になっている可能性があります。
    // その場合、TimelineCharacter用にオーバーロードを追加するか、ロジックを移植する必要があります。
    // 今回は「activeItems」を使って直接同期するロジックをここに書くか、Collider側を拡張します。
    // いったん「ColliderComponentに vector<GESequencerItem> を渡す関数がある」と仮定、
    // 無ければ実装する必要があります。
    if (collider) {
        // collider->SyncItems(activeItems, frameNow); // 理想形
        // ※既存コードとの整合性のため、ここでは一旦スキップします。
        // Collider連携が必要なら別途指示ください。
    }

    // 2. アイテム更新 (VFX / Audio)
    for (auto& it : activeItems)
    {
        // --------------------------------------------------------
        // VFX
        // --------------------------------------------------------
        if (it.type == 2)
        {
            bool isInside = (frameNow >= it.start && frameNow <= it.end);

            if (isInside)
            {
                // A. 再生開始
                if (!it.vfxActive || !it.vfxInstance)
                {
                    if (it.vfx.assetId[0] != '\0')
                    {
                        XMMATRIX W = CalcWorldMatrixForItem(it);
                        XMFLOAT3 pos = { 0,0,0 }; // 初期位置はダミー

                        auto instance = EffectManager::Get().Play(it.vfx.assetId, pos);
                        if (instance)
                        {
                            instance->loop = !it.vfx.fireOnEnterOnly;
                            instance->isSequencerControlled = true; // 外部制御ON

                            // 初期パラメータ注入
                            XMStoreFloat4x4(&instance->parentMatrix, W);
                            instance->overrideLocalTransform.position = it.vfx.offsetLocal;
                            instance->overrideLocalTransform.rotation = it.vfx.offsetRotDeg;
                            instance->overrideLocalTransform.scale = it.vfx.offsetScale;

                            float duration = FramesToSeconds(it.end - it.start);
                            if (duration < 0.1f) duration = 0.1f;
                            instance->lifeTime = duration;

                            it.vfxInstance = instance;
                            it.vfxActive = true;
                        }
                    }
                }

                // B. 継続更新
                if (it.vfxInstance)
                {
                    // 寿命同期
                    it.vfxInstance->lifeTime = FramesToSeconds(it.end - it.start);

                    // 行列更新
                    XMMATRIX W = CalcWorldMatrixForItem(it);
                    // 正規化 (スケール除去)
                    {
                        XMFLOAT4X4 m; XMStoreFloat4x4(&m, W);
                        XMVECTOR ax = XMVector3Normalize(XMVectorSet(m._11, m._12, m._13, 0));
                        XMVECTOR ay = XMVector3Normalize(XMVectorSet(m._21, m._22, m._23, 0));
                        XMVECTOR az = XMVector3Normalize(XMVectorSet(m._31, m._32, m._33, 0));
                        XMVECTOR p = XMVectorSet(m._41, m._42, m._43, 1);
                        W = XMMatrixIdentity();
                        W.r[0] = ax; W.r[1] = ay; W.r[2] = az; W.r[3] = p;
                    }
                    XMStoreFloat4x4(&it.vfxInstance->parentMatrix, W);

                    // 時間同期
                    float startSec = FramesToSeconds(it.start);
                    float effectAge = timeSec - startSec;
                    EffectManager::Get().SyncInstanceToTime(it.vfxInstance, effectAge);
                }
            }
            else
            {
                // C. 停止
                if (it.vfxActive)
                {
                    if (it.vfxInstance) it.vfxInstance->Stop(true);
                    it.vfxInstance.reset();
                    it.vfxActive = false;
                }
            }
        }
        // --------------------------------------------------------
        // Audio
        // --------------------------------------------------------
        else if (it.type == 3)
        {
            bool isInside = (frameNow >= it.start && frameNow <= it.end);

            if (isInside)
            {
                // A. 再生開始
                if (!it.audioActive)
                {
                    it.audioActive = true;
                    if (it.audio.assetId[0] != '\0')
                    {
                        if (it.audio.is3D)
                        {
                            // 3D座標取得
                            XMMATRIX W = CalcWorldMatrixForItem(it);
                            XMFLOAT4X4 mat; XMStoreFloat4x4(&mat, W);
                            XMFLOAT3 pos = { mat._41, mat._42, mat._43 };

                            it.audioSource = Audio::Instance()->Play3D(
                                it.audio.assetId, pos,
                                it.audio.volume, it.audio.pitch, it.audio.loop
                            );
                        }
                        else
                        {
                            Audio::Instance()->Play2D(
                                it.audio.assetId, it.audio.volume, it.audio.pitch, it.audio.loop
                            );
                            it.audioSource.reset();
                        }
                    }
                }

                // B. 位置更新
                if (it.audioActive && it.audioSource && it.audio.is3D)
                {
                    XMMATRIX W = CalcWorldMatrixForItem(it);
                    XMFLOAT4X4 mat; XMStoreFloat4x4(&mat, W);
                    XMFLOAT3 pos = { mat._41, mat._42, mat._43 };
                    it.audioSource->SetPosition(pos);
                }
            }
            else
            {
                // C. 停止
                if (it.audioActive)
                {
                    it.audioActive = false;
                    if (it.audioSource) {
                        it.audioSource->Stop();
                        it.audioSource.reset();
                    }
                }
            }
        }
    }
}

// ヘルパー関数群 (TimelineSequencerComponentからロジックを流用)
DirectX::XMMATRIX TimelineCharacter::CalcWorldMatrixForItem(const GESequencerItem& item)
{
    using namespace DirectX;
    XMMATRIX W_Base = XMMatrixIdentity();
    auto owner = GetActor();

    if (owner)
    {
        // 親ボーン計算 (VFX/Audio共通)
        // item.vfx.nodeIndex を参照しているが、Audioの場合も audio.nodeIndex を見るように分岐が必要
        // データの持ち方的に、vfx.nodeIndex と audio.nodeIndex は別変数だが、
        // 構造体GESequencerItemの設計上、Typeによって使い分けているはず。

        int targetNode = -1;
        if (item.type == 2) targetNode = item.vfx.nodeIndex;
        if (item.type == 3 && item.audio.is3D) targetNode = item.audio.nodeIndex;

        if (targetNode >= 0)
        {
            if (auto model = owner->GetModelRaw())
            {
                auto& nodes = model->GetNodes();
                if (targetNode < (int)nodes.size()) {
                    XMMATRIX W_Bone = XMLoadFloat4x4(&nodes[targetNode].worldTransform);
                    XMMATRIX W_Actor = XMLoadFloat4x4(&owner->GetTransform());
                    W_Base = W_Bone * W_Actor;
                }
            }
        }
        else
        {
            W_Base = XMLoadFloat4x4(&owner->GetTransform());
        }
    }

    // オフセット計算 (Audioにはオフセットが無いので単位行列、VFXなら計算)
    XMMATRIX W_Offset = XMMatrixIdentity();
    if (item.type == 2) // VFX
    {
        float p = item.vfx.offsetRotDeg.x * DEG_TO_RAD;
        float y = item.vfx.offsetRotDeg.y * DEG_TO_RAD;
        float r = item.vfx.offsetRotDeg.z * DEG_TO_RAD;
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(p, y, r);

        XMMATRIX T = XMMatrixTranslation(item.vfx.offsetLocal.x, item.vfx.offsetLocal.y, item.vfx.offsetLocal.z);
        XMMATRIX R = XMMatrixRotationQuaternion(q);
        XMMATRIX S = XMMatrixScaling(item.vfx.offsetScale.x, item.vfx.offsetScale.y, item.vfx.offsetScale.z);

        W_Offset = S * R * T;
    }

    return W_Offset * W_Base;
}

int TimelineCharacter::SecondsToFrames(float seconds) const {
    return (int)(seconds * fps + 0.5f);
}
float TimelineCharacter::FramesToSeconds(int frames) const {
    return (float)frames / fps;
}