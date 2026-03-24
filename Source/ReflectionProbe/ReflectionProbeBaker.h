#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderContext.h"
#include "Component/ReflectionProbeComponent.h"

class Registry;

class ReflectionProbeBaker {
public:
    ReflectionProbeBaker(ID3D11Device* device);
    ~ReflectionProbeBaker() = default;

    void BakeAllDirtyProbes(Registry& registry, const RenderQueue& queue, RenderContext& rc);

private:
    void Bake(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc);

    DirectX::XMMATRIX GetViewMatrixForFace(const DirectX::XMFLOAT3& pos, int faceIndex);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
};
