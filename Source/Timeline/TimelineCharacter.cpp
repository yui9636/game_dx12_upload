#include "TimelineCharacter.h"
#include "Runner/RunnerComponent.h"
#include "Actor/Actor.h"
#include "Collision/ColliderComponent.h"
#include "Model.h"
#include "Engine/EngineKernel.h"

#include "Effect/EffectManager.h"
#include "Effect/EffectNode.h"
#include "Effect/EffectLoader.h"

#include <cmath>

using namespace DirectX;

#define PI 3.14159265358979323846f
#define DEG_TO_RAD (PI / 180.0f)

void TimelineCharacter::Start()
{
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

void TimelineCharacter::OnAnimationChange(int newAnimIndex)
{
    for (auto& item : activeItems)
    {
        if (item.vfxInstance) item.vfxInstance->Stop(true);
        if (item.audioHandle != 0) EngineKernel::Instance().GetAudioWorld().StopVoice(item.audioHandle);
        item.audioHandle = 0;
    }
    activeItems.clear();

    currentAnimIndex = newAnimIndex;

    if (newAnimIndex >= 0 && newAnimIndex < (int)gameplayData.timelines.size())
    {
        activeItems = gameplayData.timelines[newAnimIndex];
    }

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

    float timeSec = runner->GetTimeSeconds();
    int frameNow = SecondsToFrames(timeSec);

    if (collider) {
    }

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
                if (!it.vfxActive || !it.vfxInstance)
                {
                    if (it.vfx.assetId[0] != '\0')
                    {
                        XMMATRIX W = CalcWorldMatrixForItem(it);
                        XMFLOAT3 pos = { 0,0,0 };

                        auto instance = EffectManager::Get().Play(it.vfx.assetId, pos);
                        if (instance)
                        {
                            instance->loop = !it.vfx.fireOnEnterOnly;
                            instance->isSequencerControlled = true;

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

                if (it.vfxInstance)
                {
                    it.vfxInstance->lifeTime = FramesToSeconds(it.end - it.start);

                    XMMATRIX W = CalcWorldMatrixForItem(it);
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

                    float startSec = FramesToSeconds(it.start);
                    float effectAge = timeSec - startSec;
                    EffectManager::Get().SyncInstanceToTime(it.vfxInstance, effectAge);
                }
            }
            else
            {
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
                if (!it.audioActive)
                {
                    it.audioActive = true;
                    if (it.audio.assetId[0] != '\0')
                    {
                        if (it.audio.is3D)
                        {
                            XMMATRIX W = CalcWorldMatrixForItem(it);
                            XMFLOAT4X4 mat; XMStoreFloat4x4(&mat, W);
                            XMFLOAT3 pos = { mat._41, mat._42, mat._43 };

                            it.audioHandle = EngineKernel::Instance().GetAudioWorld().PlayTransient3D(
                                it.audio.assetId, pos,
                                it.audio.volume, it.audio.pitch, it.audio.loop
                            );
                        }
                        else
                        {
                            EngineKernel::Instance().GetAudioWorld().PlayTransient2D(
                                it.audio.assetId, it.audio.volume, it.audio.pitch, it.audio.loop
                            );
                            it.audioHandle = 0;
                        }
                    }
                }

                if (it.audioActive && it.audioHandle != 0 && it.audio.is3D)
                {
                    XMMATRIX W = CalcWorldMatrixForItem(it);
                    XMFLOAT4X4 mat; XMStoreFloat4x4(&mat, W);
                    XMFLOAT3 pos = { mat._41, mat._42, mat._43 };
                    EngineKernel::Instance().GetAudioWorld().SetVoicePosition(it.audioHandle, pos);
                }
            }
            else
            {
                if (it.audioActive)
                {
                    it.audioActive = false;
                    if (it.audioHandle != 0) {
                        EngineKernel::Instance().GetAudioWorld().StopVoice(it.audioHandle);
                        it.audioHandle = 0;
                    }
                }
            }
        }
    }
}

DirectX::XMMATRIX TimelineCharacter::CalcWorldMatrixForItem(const GESequencerItem& item)
{
    using namespace DirectX;
    XMMATRIX W_Base = XMMatrixIdentity();
    auto owner = GetActor();

    if (owner)
    {

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
