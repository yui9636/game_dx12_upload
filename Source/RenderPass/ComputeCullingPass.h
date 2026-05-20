#pragma once
#include "IRenderPass.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

class DX12Device;
class IBuffer;

// compute shader の 2D dispatch で GPU frustum culling を行う pass。
// 入力は culling 前の全 instance を含む preparedInstanceData。
// 出力は GPU 上で compact した visible instance と indirect draw args。
// rc.active* を上書きし、renderer 側からは culling 後データを透過的に見せる。
class ComputeCullingPass : public IRenderPass {
public:
    std::string GetName() const override { return "ComputeCullingPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // async compute queue に投げた command list と fence を、完了まで保持する。
    struct InFlightComputeSubmission
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
        uint64_t fenceValue = 0;
        uint32_t timingSlot = UINT32_MAX;
        uint32_t commandCount = 0;
        uint32_t instanceCount = 0;
    };

    void InitGpuResources(DX12Device* device);
    void InitTimingResources(DX12Device* device);
    void RetireCompletedSubmissions(DX12Device* device);
    uint32_t AcquireTimingSlot() const;
    void ExtractFrustumPlanes(const DirectX::XMFLOAT4X4& viewProj, DirectX::XMFLOAT4 planesOut[6]);

    bool m_initialized = false; // root signature / PSO の初期化完了フラグ。

    // compute pipeline を保持する。
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_computeRootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_computePSO;

    // この pass が所有する DEFAULT heap / UAV の GPU buffer。
    std::shared_ptr<IBuffer> m_culledInstanceBuffer;   // GPU culling 後の visible InstanceData。
    std::shared_ptr<IBuffer> m_culledDrawArgsBuffer;   // GPU culling 後の ExecuteIndirect DrawArgs。
    uint32_t m_instanceCapacity = 0;                   // 確保済み instance 数。
    uint32_t m_drawArgsCapacity = 0;                   // 確保済み command 数。

    // DrawArgs 初期化用の UPLOAD staging buffer。
    std::shared_ptr<IBuffer> m_stagingBuffer;
    uint32_t m_stagingCapacity = 0;

    std::shared_ptr<IBuffer> m_paramsBuffer; // frustum plane などを渡す constant buffer。

    // multi-draw ExecuteIndirect 用の count buffer。
    std::shared_ptr<IBuffer> m_countBuffer;        // ExecuteIndirect の draw count を保持する UAV。
    std::shared_ptr<IBuffer> m_countStagingBuffer; // count 初期化用 staging。

    // フレームをまたいで buffer state を追跡する。
    // buffer の現在 state を簡易追跡し、明示 barrier の before state に使う。
    bool m_instanceInVBState = false;
    bool m_drawArgsInIndirectState = false;
    bool m_countInIndirectState = false;
    bool m_needsGrow = false; // 使用中 buffer を次 frame に拡張するためのフラグ。
    std::vector<InFlightComputeSubmission> m_inFlightSubmissions;

    static constexpr uint32_t kTimingSlotCount = 16;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_computeTimestampHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_computeTimestampReadback;
    double m_lastAsyncGpuMs = 0.0; // async compute の直近 GPU 時間。
};
