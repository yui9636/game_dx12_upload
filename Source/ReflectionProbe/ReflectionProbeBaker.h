#pragma once
#include <DirectXMath.h>
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderContext.h"
#include "Component/ReflectionProbeComponent.h"

class Registry;

// 各 ReflectionProbeComponent の位置を中心に scene を cubemap へ焼き込む。
// DX11 と DX12 の両方で動作し、焼き込んだ cubemap は
// API 非依存の ITexture として ReflectionProbeComponent::cubemapTexture へ格納する。
class ReflectionProbeBaker {
public:
    ReflectionProbeBaker() = default;
    ~ReflectionProbeBaker() = default;

    void BakeAllDirtyProbes(Registry& registry, const RenderQueue& queue, RenderContext& rc);

private:
    void Bake(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc);
    void BakeDX11(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc);
    void BakeDX12(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc);

    DirectX::XMMATRIX GetViewMatrixForFace(const DirectX::XMFLOAT3& pos, int faceIndex);
};
