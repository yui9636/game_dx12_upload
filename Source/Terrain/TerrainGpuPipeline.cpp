#include "TerrainGpuPipeline.h"
#include "TerrainAsset.h"
#include "RHI/DX12/DX12Device.h"
#include "Render/Graphics.h"
#include "Console/Logger.h"
#include <d3dcompiler.h>
#include <fstream>
#include <vector>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace {

struct TerrainGenParamsCB {
    int   resolution;
    int   erosionIterations;
    int   erosionRadius;
    int   erosionLifetime;

    float noiseFreq;
    int   octaves;
    float lacunarity;
    float gain;

    int   noiseSeed;
    int   erosionSeed;
    float inertia;
    float sedimentCapacityFactor;

    float minSedimentCapacity;
    float erodeSpeed;
    float depositSpeed;
    float evaporateSpeed;

    float gravity;
    float initialWater;
    float initialSpeed;
    float rockAltitudeMin;

    float rockSlopeDegrees;
    float dirtMidAltitude;
    float dirtStrength;
    float heightScale;

    float worldSizeX;
    float worldSizeZ;
    int   noiseType;
    int   padding0;

    float domainWarpStrength;
    float terraceSteps;
    int   padding1;
    int   padding2;
};
static_assert(sizeof(TerrainGenParamsCB) == 128, "TerrainGenParamsCB size mismatch");

bool LoadBlobW(const wchar_t* path, ComPtr<ID3DBlob>& outBlob)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(size), &outBlob))) return false;
    f.read(static_cast<char*>(outBlob->GetBufferPointer()), size);
    return f.good();
}

ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* d, uint64_t bytes, D3D12_RESOURCE_STATES initialState)
{
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width  = bytes; desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN; desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ComPtr<ID3D12Resource> r;
    if (FAILED(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(&r)))) return nullptr;
    return r;
}

ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* d, uint64_t bytes)
{
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width  = bytes; desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN; desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> r;
    if (FAILED(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&r)))) return nullptr;
    return r;
}

ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device* d, uint64_t bytes)
{
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width  = bytes; desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN; desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> r;
    if (FAILED(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&r)))) return nullptr;
    return r;
}

void UAVBarrier(ID3D12GraphicsCommandList* cmd, ID3D12Resource* r) {
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; b.UAV.pResource = r; cmd->ResourceBarrier(1, &b);
}
void TransitionBarrier(ID3D12GraphicsCommandList* cmd, ID3D12Resource* r,
                       D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r; b.Transition.StateBefore = before; b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &b);
}

} // anonymous namespace

TerrainGpuPipeline& TerrainGpuPipeline::Instance()
{
    static TerrainGpuPipeline g_instance;
    return g_instance;
}

bool TerrainGpuPipeline::EnsureInitialized()
{
    if (m_initialized) return true;
    m_device = Graphics::Instance().GetDX12Device();
    if (!m_device || !m_device->GetDevice()) {
        LOG_WARN("[TerrainGpuPipeline] DX12 device unavailable");
        return false;
    }
    if (!CreateRootSignature())  return false;
    if (!CreatePipelineStates()) return false;
    if (!CreateCommandList())    return false;
    m_initialized = true;
    return true;
}

void TerrainGpuPipeline::Shutdown()
{
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    m_initialized = false;
}

bool TerrainGpuPipeline::CreateRootSignature()
{
    auto* d = m_device->GetDevice();
    D3D12_ROOT_PARAMETER1 params[3] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor    = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[1].Descriptor    = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[2].Descriptor    = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
    desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    desc.Desc_1_1.NumParameters = _countof(params);
    desc.Desc_1_1.pParameters   = params;
    desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> sig, err;
    if (FAILED(D3D12SerializeVersionedRootSignature(&desc, &sig, &err))) {
        if (err) LOG_ERROR("[TerrainGpuPipeline] root sig serialize failed: %s",
                           static_cast<const char*>(err->GetBufferPointer()));
        return false;
    }
    return SUCCEEDED(d->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                                            IID_PPV_ARGS(&m_rootSig)));
}

bool TerrainGpuPipeline::CreatePipelineStates()
{
    auto* d = m_device->GetDevice();
    auto makePso = [&](const wchar_t* path, ComPtr<ID3D12PipelineState>& out, const char* label) -> bool {
        ComPtr<ID3DBlob> blob;
        if (!LoadBlobW(path, blob)) { LOG_ERROR("[TerrainGpuPipeline] missing CSO %s", label); return false; }
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = m_rootSig.Get();
        pd.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
        return SUCCEEDED(d->CreateComputePipelineState(&pd, IID_PPV_ARGS(&out)));
    };
    return makePso(L"Data/Shader/TerrainNoiseCS.cso",     m_psoNoise, "noise") &&
           makePso(L"Data/Shader/TerrainErodeCS.cso",     m_psoErode, "erode") &&
           makePso(L"Data/Shader/TerrainAutoSplatCS.cso", m_psoSplat, "splat");
}

bool TerrainGpuPipeline::CreateCommandList()
{
    auto* d = m_device->GetDevice();
    if (FAILED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_alloc)))) return false;
    if (FAILED(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_alloc.Get(), nullptr, IID_PPV_ARGS(&m_cmd)))) return false;
    m_cmd->Close();
    if (FAILED(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) return false;
    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return m_fenceEvent != nullptr;
}

bool TerrainGpuPipeline::EnsureResources(uint32_t resolution)
{
    if (m_resolution == resolution && m_heightBuf && m_splatBuf && m_paramsCB && m_heightUpload) return true;
    m_resolution = resolution;
    auto* d = m_device->GetDevice();
    const uint64_t cellCount = static_cast<uint64_t>(resolution) * resolution;
    const uint64_t heightBytes = cellCount * sizeof(float);
    const uint64_t splatBytes  = cellCount * sizeof(uint32_t);
    m_heightBuf      = CreateDefaultBuffer(d, heightBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_splatBuf       = CreateDefaultBuffer(d, splatBytes,  D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_heightReadback = CreateReadbackBuffer(d, heightBytes);
    m_splatReadback  = CreateReadbackBuffer(d, splatBytes);
    m_heightUpload   = CreateUploadBuffer(d, heightBytes);
    const uint64_t cbBytes = (sizeof(TerrainGenParamsCB) + 255u) & ~255u;
    m_paramsCB = CreateUploadBuffer(d, cbBytes);
    return m_heightBuf && m_splatBuf && m_heightReadback && m_splatReadback && m_heightUpload && m_paramsCB;
}

void TerrainGpuPipeline::UploadParams(const TerrainAsset& asset)
{
    TerrainGenParamsCB cb{};
    cb.resolution             = static_cast<int>(asset.resolution);
    cb.erosionIterations      = asset.erosion.iterations;
    cb.erosionRadius          = asset.erosion.erosionRadius;
    cb.erosionLifetime        = asset.erosion.maxDropletLifetime;
    cb.noiseFreq              = asset.noiseFreq;
    cb.octaves                = asset.octaves;
    cb.lacunarity             = asset.lacunarity;
    cb.gain                   = asset.gain;
    cb.noiseSeed              = asset.seed;
    cb.erosionSeed            = asset.erosion.seed;
    cb.inertia                = asset.erosion.inertia;
    cb.sedimentCapacityFactor = asset.erosion.sedimentCapacityFactor;
    cb.minSedimentCapacity    = asset.erosion.minSedimentCapacity;
    cb.erodeSpeed             = asset.erosion.erodeSpeed;
    cb.depositSpeed           = asset.erosion.depositSpeed;
    cb.evaporateSpeed         = asset.erosion.evaporateSpeed;
    cb.gravity                = asset.erosion.gravity;
    cb.initialWater           = asset.erosion.initialWater;
    cb.initialSpeed           = asset.erosion.initialSpeed;
    cb.rockAltitudeMin        = asset.autoSplat.rockAltitudeMin;
    cb.rockSlopeDegrees       = asset.autoSplat.rockSlopeDegrees;
    cb.dirtMidAltitude        = asset.autoSplat.dirtMidAltitude;
    cb.dirtStrength           = asset.autoSplat.dirtStrength;
    cb.heightScale            = asset.heightScale;
    cb.worldSizeX             = asset.worldSizeX;
    cb.worldSizeZ             = asset.worldSizeZ;
    cb.noiseType              = asset.noiseType;
    cb.domainWarpStrength     = asset.domainWarpStrength;
    cb.terraceSteps           = asset.terraceSteps;

    void* p = nullptr;
    D3D12_RANGE noRead{ 0, 0 };
    if (SUCCEEDED(m_paramsCB->Map(0, &noRead, &p))) {
        std::memcpy(p, &cb, sizeof(cb));
        m_paramsCB->Unmap(0, nullptr);
    }
}

bool TerrainGpuPipeline::UploadHeights(const TerrainAsset& asset)
{
    const uint64_t cellCount = static_cast<uint64_t>(asset.resolution) * asset.resolution;
    if (asset.heightData.size() != cellCount) return false;

    void* p = nullptr;
    D3D12_RANGE noRead{ 0, 0 };
    if (FAILED(m_heightUpload->Map(0, &noRead, &p))) return false;
    std::memcpy(p, asset.heightData.data(), cellCount * sizeof(float));
    m_heightUpload->Unmap(0, nullptr);

    TransitionBarrier(m_cmd.Get(), m_heightBuf.Get(),
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
    m_cmd->CopyBufferRegion(m_heightBuf.Get(), 0, m_heightUpload.Get(), 0, cellCount * sizeof(float));
    TransitionBarrier(m_cmd.Get(), m_heightBuf.Get(),
                      D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    return true;
}

bool TerrainGpuPipeline::ReadbackHeights(TerrainAsset& asset)
{
    const uint64_t cellCount = static_cast<uint64_t>(asset.resolution) * asset.resolution;
    void* src = nullptr;
    D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(cellCount * sizeof(float)) };
    if (FAILED(m_heightReadback->Map(0, &readRange, &src))) return false;
    asset.heightData.assign(static_cast<const float*>(src),
                            static_cast<const float*>(src) + cellCount);
    D3D12_RANGE writeRange{ 0, 0 };
    m_heightReadback->Unmap(0, &writeRange);
    return true;
}

bool TerrainGpuPipeline::ReadbackSplat(TerrainAsset& asset)
{
    const uint64_t cellCount = static_cast<uint64_t>(asset.resolution) * asset.resolution;
    void* src = nullptr;
    D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(cellCount * sizeof(uint32_t)) };
    if (FAILED(m_splatReadback->Map(0, &readRange, &src))) return false;
    const uint32_t* pix = static_cast<const uint32_t*>(src);
    asset.splatData.resize(cellCount * 4u);
    for (uint64_t i = 0; i < cellCount; ++i) {
        const uint32_t v = pix[i];
        asset.splatData[i * 4u + 0] = static_cast<uint8_t>(v & 0xFFu);
        asset.splatData[i * 4u + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        asset.splatData[i * 4u + 2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
        asset.splatData[i * 4u + 3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
    }
    D3D12_RANGE writeRange{ 0, 0 };
    m_splatReadback->Unmap(0, &writeRange);
    return true;
}

void TerrainGpuPipeline::ExecuteAndWait()
{
    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd.Get() };
    m_device->GetCommandQueue()->ExecuteCommandLists(1, lists);
    ++m_fenceValue;
    m_device->GetCommandQueue()->Signal(m_fence.Get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

bool TerrainGpuPipeline::Run(TerrainAsset& asset, uint32_t stages)
{
    if (stages == 0) return true;
    if (asset.resolution < 4) return false;
    if (!EnsureInitialized()) return false;
    if (!EnsureResources(asset.resolution)) return false;

    UploadParams(asset);

    const uint32_t res = asset.resolution;
    const uint32_t groups2D = (res + 7u) / 8u;

    m_alloc->Reset();
    m_cmd->Reset(m_alloc.Get(), nullptr);

    // If Noise stage is not requested, seed the GPU height buffer from the
    // existing CPU height data so subsequent stages have valid input.
    if (!(stages & StageNoise) && (stages & (StageErode | StageAutoSplat))) {
        if (!UploadHeights(asset)) {
            m_cmd->Close();
            return false;
        }
    }

    m_cmd->SetComputeRootSignature(m_rootSig.Get());
    m_cmd->SetComputeRootConstantBufferView(0, m_paramsCB->GetGPUVirtualAddress());
    m_cmd->SetComputeRootUnorderedAccessView(1, m_heightBuf->GetGPUVirtualAddress());
    m_cmd->SetComputeRootUnorderedAccessView(2, m_splatBuf->GetGPUVirtualAddress());

    if (stages & StageNoise) {
        m_cmd->SetPipelineState(m_psoNoise.Get());
        m_cmd->Dispatch(groups2D, groups2D, 1);
        UAVBarrier(m_cmd.Get(), m_heightBuf.Get());
    }
    if (stages & StageErode) {
        const uint32_t dropletGroups = (static_cast<uint32_t>(asset.erosion.iterations) + 63u) / 64u;
        if (dropletGroups > 0) {
            m_cmd->SetPipelineState(m_psoErode.Get());
            m_cmd->Dispatch(dropletGroups, 1, 1);
            UAVBarrier(m_cmd.Get(), m_heightBuf.Get());
        }
    }
    if (stages & StageAutoSplat) {
        m_cmd->SetPipelineState(m_psoSplat.Get());
        m_cmd->Dispatch(groups2D, groups2D, 1);
        UAVBarrier(m_cmd.Get(), m_splatBuf.Get());
    }

    const bool needHeightReadback = (stages & (StageNoise | StageErode)) != 0;
    const bool needSplatReadback  = (stages & StageAutoSplat) != 0;
    const uint64_t cellCount = static_cast<uint64_t>(res) * res;

    if (needHeightReadback) {
        TransitionBarrier(m_cmd.Get(), m_heightBuf.Get(),
                          D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_cmd->CopyBufferRegion(m_heightReadback.Get(), 0, m_heightBuf.Get(), 0, cellCount * sizeof(float));
        TransitionBarrier(m_cmd.Get(), m_heightBuf.Get(),
                          D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    if (needSplatReadback) {
        TransitionBarrier(m_cmd.Get(), m_splatBuf.Get(),
                          D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_cmd->CopyBufferRegion(m_splatReadback.Get(), 0, m_splatBuf.Get(), 0, cellCount * sizeof(uint32_t));
        TransitionBarrier(m_cmd.Get(), m_splatBuf.Get(),
                          D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    ExecuteAndWait();

    bool ok = true;
    if (needHeightReadback) ok = ok && ReadbackHeights(asset);
    if (needSplatReadback)  ok = ok && ReadbackSplat(asset);
    return ok;
}
