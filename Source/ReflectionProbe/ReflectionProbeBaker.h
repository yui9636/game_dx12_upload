#pragma once
#include <DirectXMath.h>
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderContext.h"
#include "Component/ReflectionProbeComponent.h"

class Registry;

// Bakes the scene into a cubemap centered at each ReflectionProbeComponent.
// Works for both DX11 and DX12. The probe's resulting cubemap is stored in
// ReflectionProbeComponent::cubemapTexture as an API-neutral ITexture.
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
