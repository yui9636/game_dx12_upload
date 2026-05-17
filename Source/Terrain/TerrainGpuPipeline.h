#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

class DX12Device;
struct TerrainAsset;

// All terrain generation runs on GPU compute. The pipeline owns three PSOs
// (noise / erode / autosplat) and a pair of structured buffers (heights +
// splats). The single Instance() lazily initializes against the global
// DX12 device the first time it is used.
class TerrainGpuPipeline {
public:
    enum Stage : uint32_t {
        StageNoise     = 1u << 0,
        StageErode     = 1u << 1,
        StageAutoSplat = 1u << 2,
        StageAll       = StageNoise | StageErode | StageAutoSplat
    };

    static TerrainGpuPipeline& Instance();

    // Runs the requested stages and mirrors the results back into asset.
    // If StageNoise is omitted, the current asset.heightData is uploaded as
    // the starting height map (so Erode / AutoSplat can run independently).
    bool Run(TerrainAsset& asset, uint32_t stageFlags);

    void Shutdown();

private:
    bool EnsureInitialized();
    bool CreateRootSignature();
    bool CreatePipelineStates();
    bool CreateCommandList();
    bool EnsureResources(uint32_t resolution);
    void UploadParams(const TerrainAsset& asset);
    bool UploadHeights(const TerrainAsset& asset);
    bool ReadbackHeights(TerrainAsset& asset);
    bool ReadbackSplat(TerrainAsset& asset);
    void ExecuteAndWait();

    bool m_initialized = false;
    DX12Device* m_device = nullptr;
    uint32_t    m_resolution = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature>        m_rootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>        m_psoNoise;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>        m_psoErode;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>        m_psoSplat;

    Microsoft::WRL::ComPtr<ID3D12Resource>             m_heightBuf;
    Microsoft::WRL::ComPtr<ID3D12Resource>             m_splatBuf;
    Microsoft::WRL::ComPtr<ID3D12Resource>             m_heightReadback;
    Microsoft::WRL::ComPtr<ID3D12Resource>             m_splatReadback;
    Microsoft::WRL::ComPtr<ID3D12Resource>             m_heightUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource>             m_paramsCB;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     m_alloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>  m_cmd;
    Microsoft::WRL::ComPtr<ID3D12Fence>                m_fence;
    HANDLE                                             m_fenceEvent = nullptr;
    uint64_t                                           m_fenceValue = 0;
};
