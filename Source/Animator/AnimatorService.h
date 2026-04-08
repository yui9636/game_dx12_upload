#pragma once

#include "Entity/Entity.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

class Registry;
class Model;
class AnimatorRuntimeRegistry;
struct AnimatorComponent;

class AnimatorService
{
public:
    static AnimatorService& Instance();

    void SetRegistry(Registry* registry);
    Registry* GetRegistry() const { return m_registry; }

    void EnsureAnimator(EntityID entity);
    void RemoveAnimator(EntityID entity);

    void PlayBase(EntityID entity, int animIndex, bool loop = true, float blendTime = 0.2f, float speed = 1.0f);
    void PlayAction(EntityID entity, int animIndex, bool loop = false, float blendTime = 0.1f, bool isFullBody = true);
    void StopAction(EntityID entity, float blendTime = 0.2f);
    void SetActionTime(EntityID entity, float time);

    void SetDriver(EntityID entity, float time, int overrideAnimIndex, bool loop, bool allowInternalUpdate);
    void ClearDriver(EntityID entity);

    std::vector<std::string> GetAnimationNameList(EntityID entity) const;
    int GetAnimationIndexByName(EntityID entity, const std::string& name) const;
    DirectX::XMFLOAT3 GetRootMotionDelta(EntityID entity) const;

    AnimatorRuntimeRegistry& GetRuntimeRegistry() { return *m_runtimeRegistry; }

private:
    AnimatorService();
    AnimatorComponent* GetAnimator(EntityID entity) const;
    Model* GetModel(EntityID entity) const;

private:
    Registry* m_registry = nullptr;
    AnimatorRuntimeRegistry* m_runtimeRegistry = nullptr;
};
