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

    // ECSから needsBake=true のプローブを探し出して全て焼き付ける
    void BakeAllDirtyProbes(Registry& registry, const RenderQueue& queue, RenderContext& rc);

private:
    // 1つのプローブを焼き付けるメイン処理
    void Bake(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc);

    // 6方向（+X, -X, +Y, -Y, +Z, -Z）のカメラ行列を作るヘルパー
    DirectX::XMMATRIX GetViewMatrixForFace(const DirectX::XMFLOAT3& pos, int faceIndex);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
};