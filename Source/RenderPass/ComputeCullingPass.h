#pragma once
#include "IRenderPass.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

class DX12Device;
class IBuffer;

// GPU frustum culling via compute shader (2D dispatch).
// Input:  preparedInstanceData (all instances, unculled)
// Output: compacted visible instances + indirect draw args on GPU
// Overwrites rc.active* so renderers see culled data transparently.
class ComputeCullingPass : public IRenderPass {
public:
    std::string GetName() const override { return "ComputeCullingPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    void InitGpuResources(DX12Device* device);
    void ExtractFrustumPlanes(const DirectX::XMFLOAT4X4& viewProj, DirectX::XMFLOAT4 planesOut[6]);

    bool m_initialized = false;

    // Compute pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_computeRootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_computePSO;

    // GPU buffers (DEFAULT heap, UAV) — owned by this pass
    std::shared_ptr<IBuffer> m_culledInstanceBuffer;   // UAVStorage
    std::shared_ptr<IBuffer> m_culledDrawArgsBuffer;    // UAVStorage
    uint32_t m_instanceCapacity = 0;
    uint32_t m_drawArgsCapacity = 0;

    // Staging buffer for DrawArgs initialization (IBuffer, UPLOAD)
    std::shared_ptr<IBuffer> m_stagingBuffer;
    uint32_t m_stagingCapacity = 0;

    // CullCommandMeta upload buffer (IBuffer, UPLOAD)
    std::shared_ptr<IBuffer> m_cullMetaBuffer;
    uint32_t m_cullMetaCapacity = 0;

    // Count buffer for multi-draw ExecuteIndirect
    std::shared_ptr<IBuffer> m_countBuffer;     // UAVStorage, holds uint32_t commandCount

    // Track buffer states across frames
    bool m_instanceInVBState = false;
    bool m_drawArgsInIndirectState = false;
};
