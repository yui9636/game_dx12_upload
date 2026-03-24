#include "ComputeCullingPass.h"
#include "Graphics.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12Buffer.h"
#include "Render/GlobalRootSignature.h"
#include "Model/ModelResource.h"
#include "RenderContext/IndirectDrawCommon.h"
#include "Console/Logger.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
    struct CullingParams {
        XMFLOAT4 frustumPlanes[6];
        uint32_t commandCount;
        uint32_t maxInstancesPerCmd;
        uint32_t pad[2];
    };

    void TransitionBuffer(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after)
    {
        if (!commandList || !resource || before == after) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
    }
}

void ComputeCullingPass::InitGpuResources(DX12Device* device)
{
    if (m_initialized || !device) {
        return;
    }

    auto* d3dDevice = device->GetDevice();

    D3D12_ROOT_PARAMETER1 params[5] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[1].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[2].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[3].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
    params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[4].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
    params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rsDesc.Desc_1_1.NumParameters = 5;
    rsDesc.Desc_1_1.pParameters = params;

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeVersionedRootSignature(&rsDesc, &blob, &error);
    if (FAILED(hr)) {
        if (error) {
            LOG_ERROR("[ComputeCulling] RootSig serialize: %s", static_cast<const char*>(error->GetBufferPointer()));
        }
        return;
    }

    hr = d3dDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSig));
    if (FAILED(hr)) {
        LOG_ERROR("[ComputeCulling] CreateRootSignature failed");
        return;
    }

    ComPtr<ID3DBlob> csBlob;
    hr = D3DReadFileToBlob(L"Data/Shader/FrustumCullCS.cso", &csBlob);
    if (FAILED(hr)) {
        LOG_ERROR("[ComputeCulling] Failed to load FrustumCullCS.cso");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_computeRootSig.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
    hr = d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_computePSO));
    if (FAILED(hr)) {
        LOG_ERROR("[ComputeCulling] CreateComputePipelineState failed");
        return;
    }

    m_initialized = true;
}

void ComputeCullingPass::ExtractFrustumPlanes(const XMFLOAT4X4& vp, XMFLOAT4 planesOut[6])
{
    const float* m = reinterpret_cast<const float*>(&vp);
    planesOut[0] = { m[3] + m[0], m[7] + m[4], m[11] + m[8],  m[15] + m[12] };
    planesOut[1] = { m[3] - m[0], m[7] - m[4], m[11] - m[8],  m[15] - m[12] };
    planesOut[2] = { m[3] + m[1], m[7] + m[5], m[11] + m[9],  m[15] + m[13] };
    planesOut[3] = { m[3] - m[1], m[7] - m[5], m[11] - m[9],  m[15] - m[13] };
    planesOut[4] = { m[2],      m[6],      m[10],       m[14] };
    planesOut[5] = { m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14] };

    for (int i = 0; i < 6; ++i) {
        XMVECTOR p = XMLoadFloat4(&planesOut[i]);
        const float len = XMVectorGetX(XMVector3Length(p));
        if (len > 1e-6f) {
            p = XMVectorScale(p, 1.0f / len);
            XMStoreFloat4(&planesOut[i], p);
        }
    }
}

void ComputeCullingPass::Setup(FrameGraphBuilder& builder)
{
    (void)builder;
}

void ComputeCullingPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    (void)resources;
    (void)queue;

    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        return;
    }
    if (rc.activeDrawCommands.empty()) {
        return;
    }

    auto& graphics = Graphics::Instance();
    auto* dx12Device = graphics.GetDX12Device();
    if (!dx12Device) {
        return;
    }

    if (auto* computeFence = dx12Device->GetComputeFence()) {
        const uint64_t completedValue = computeFence->GetCompletedValue();
        auto it = m_inFlightSubmissions.begin();
        while (it != m_inFlightSubmissions.end()) {
            if (it->fenceValue <= completedValue) {
                it = m_inFlightSubmissions.erase(it);
            } else {
                ++it;
            }
        }
    }

    InitGpuResources(dx12Device);
    if (!m_initialized) {
        return;
    }

    auto* factory = graphics.GetResourceFactory();
    if (!factory) {
        return;
    }

    std::vector<CullCommandMeta> metaEntries;
    std::vector<DrawArgs> initialDrawArgs;
    std::vector<size_t> commandIndices;

    uint32_t outputStart = 0;
    uint32_t maxInstancesPerCmd = 0;

    for (size_t commandIndex = 0; commandIndex < rc.activeDrawCommands.size(); ++commandIndex) {
        const auto& cmd = rc.activeDrawCommands[commandIndex];
        if (!cmd.supportsInstancing || cmd.instanceCount == 0 || !cmd.modelResource) {
            continue;
        }

        const auto* meshRes = cmd.modelResource->GetMeshResource(static_cast<int>(cmd.meshIndex));
        if (!meshRes) {
            continue;
        }

        float cx = 0.0f;
        float cy = 0.0f;
        float cz = 0.0f;
        float radius = 5.0f;
        const auto bounds = cmd.modelResource->GetLocalBounds();
        cx = bounds.Center.x;
        cy = bounds.Center.y;
        cz = bounds.Center.z;
        const auto& ext = bounds.Extents;
        radius = sqrtf(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z);

        CullCommandMeta meta{};
        meta.firstInstance = cmd.firstInstance;
        meta.instanceCount = cmd.instanceCount;
        meta.outputInstanceStart = outputStart;
        meta.drawArgsIndex = static_cast<uint32_t>(metaEntries.size());
        meta.indexCount = cmd.modelResource->GetMeshIndexCount(static_cast<int>(cmd.meshIndex));
        meta.baseVertex = 0;
        meta.boundsCenterX = cx;
        meta.boundsCenterY = cy;
        meta.boundsCenterZ = cz;
        meta.boundsRadius = radius;
        metaEntries.push_back(meta);

        DrawArgs args{};
        args.indexCountPerInstance = meta.indexCount;
        args.instanceCount = 0;
        args.startIndexLocation = 0;
        args.baseVertexLocation = 0;
        args.startInstanceLocation = outputStart;
        initialDrawArgs.push_back(args);

        commandIndices.push_back(commandIndex);
        maxInstancesPerCmd = (std::max)(maxInstancesPerCmd, cmd.instanceCount);
        outputStart += cmd.instanceCount;
    }

    if (metaEntries.empty()) {
        return;
    }

    const uint32_t commandCount = static_cast<uint32_t>(metaEntries.size());
    const uint32_t totalOutputSlots = outputStart;
    const uint32_t drawArgsBytes = commandCount * DRAW_ARGS_STRIDE;
    const uint32_t metaBytes = commandCount * static_cast<uint32_t>(sizeof(CullCommandMeta));

    const bool needsInitialInstanceBuffer = !m_culledInstanceBuffer;
    const bool needsInitialDrawArgsBuffer = !m_culledDrawArgsBuffer;
    const bool needGrowInstance = m_culledInstanceBuffer && m_instanceCapacity < totalOutputSlots;
    const bool needGrowDrawArgs = m_culledDrawArgsBuffer && m_drawArgsCapacity < commandCount;
    if (needGrowInstance || needGrowDrawArgs) {
        m_needsGrow = true;
        return;
    }

    if (needsInitialInstanceBuffer || needsInitialDrawArgsBuffer || m_needsGrow) {
        m_needsGrow = false;
        if (needsInitialInstanceBuffer || m_instanceCapacity < totalOutputSlots) {
            const uint32_t cap = ((totalOutputSlots + 255u) / 256u) * 256u;
            m_culledInstanceBuffer = factory->CreateBuffer(cap * INSTANCE_DATA_STRIDE, BufferType::UAVStorage, nullptr);
            m_instanceCapacity = cap;
            m_instanceInVBState = false;
        }
        if (needsInitialDrawArgsBuffer || m_drawArgsCapacity < commandCount) {
            const uint32_t cap = ((commandCount * DRAW_ARGS_STRIDE + 255u) / 256u) * 256u;
            m_culledDrawArgsBuffer = factory->CreateBuffer(cap, BufferType::UAVStorage, nullptr);
            m_drawArgsCapacity = commandCount;
            m_drawArgsInIndirectState = false;
        }
    }

    if (!m_stagingBuffer || m_stagingCapacity < drawArgsBytes) {
        const uint32_t cap = ((drawArgsBytes + 255u) / 256u) * 256u;
        m_stagingBuffer = factory->CreateBuffer(cap, BufferType::Vertex, nullptr);
        m_stagingCapacity = cap;
    }
    {
        auto* staging = static_cast<DX12Buffer*>(m_stagingBuffer.get());
        void* mapped = staging ? staging->Map() : nullptr;
        if (!mapped) {
            LOG_ERROR("[ComputeCulling] staging Map failed");
            return;
        }
        memcpy(mapped, initialDrawArgs.data(), drawArgsBytes);
        staging->Unmap();
    }

    if (!m_cullMetaBuffer || m_cullMetaCapacity < metaBytes) {
        const uint32_t cap = ((metaBytes + 255u) / 256u) * 256u;
        m_cullMetaBuffer = factory->CreateBuffer(cap, BufferType::Vertex, nullptr);
        m_cullMetaCapacity = cap;
    }
    {
        auto* meta = static_cast<DX12Buffer*>(m_cullMetaBuffer.get());
        void* mapped = meta ? meta->Map() : nullptr;
        if (!mapped) {
            LOG_ERROR("[ComputeCulling] meta Map failed");
            return;
        }
        memcpy(mapped, metaEntries.data(), metaBytes);
        meta->Unmap();
    }

    if (!m_paramsBuffer) {
        m_paramsBuffer = factory->CreateBuffer(256, BufferType::Constant, nullptr);
    }

    CullingParams cbParams{};
    XMMATRIX V = XMLoadFloat4x4(&rc.viewMatrix);
    XMMATRIX P = XMLoadFloat4x4(&rc.projectionMatrix);
    XMFLOAT4X4 vp;
    XMStoreFloat4x4(&vp, V * P);
    ExtractFrustumPlanes(vp, cbParams.frustumPlanes);
    cbParams.commandCount = commandCount;
    cbParams.maxInstancesPerCmd = maxInstancesPerCmd;

    {
        auto* params = static_cast<DX12Buffer*>(m_paramsBuffer.get());
        void* mapped = params ? params->Map() : nullptr;
        if (!mapped) {
            LOG_ERROR("[ComputeCulling] params Map failed");
            return;
        }
        memcpy(mapped, &cbParams, sizeof(cbParams));
        params->Unmap();
    }

    if (!m_countBuffer) {
        m_countBuffer = factory->CreateBuffer(256, BufferType::UAVStorage, nullptr);
        m_countInIndirectState = false;
    }
    if (!m_countStagingBuffer) {
        m_countStagingBuffer = factory->CreateBuffer(256, BufferType::Vertex, nullptr);
    }
    {
        auto* countStaging = static_cast<DX12Buffer*>(m_countStagingBuffer.get());
        void* mapped = countStaging ? countStaging->Map() : nullptr;
        if (!mapped) {
            LOG_ERROR("[ComputeCulling] count staging Map failed");
            return;
        }
        memcpy(mapped, &commandCount, sizeof(uint32_t));
        countStaging->Unmap();
    }

    auto* inputBuf = static_cast<DX12Buffer*>(rc.preparedVisibleInstanceStructuredBuffer.get());
    auto* metaBuf = static_cast<DX12Buffer*>(m_cullMetaBuffer.get());
    auto* paramsBuf = static_cast<DX12Buffer*>(m_paramsBuffer.get());
    auto* stagingBuf = static_cast<DX12Buffer*>(m_stagingBuffer.get());
    auto* gpuDrawArgs = static_cast<DX12Buffer*>(m_culledDrawArgsBuffer.get());
    auto* gpuInstance = static_cast<DX12Buffer*>(m_culledInstanceBuffer.get());
    auto* countBuf = static_cast<DX12Buffer*>(m_countBuffer.get());
    auto* countStagingBuf = static_cast<DX12Buffer*>(m_countStagingBuffer.get());
    if (!inputBuf || !metaBuf || !paramsBuf || !stagingBuf || !gpuDrawArgs || !gpuInstance || !countBuf || !countStagingBuf) {
        return;
    }

    ComPtr<ID3D12CommandAllocator> allocator;
    HRESULT hr = dx12Device->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&allocator));
    if (FAILED(hr)) {
        LOG_ERROR("[ComputeCulling] CreateCommandAllocator failed");
        return;
    }

    ComPtr<ID3D12GraphicsCommandList> commandList;
    hr = dx12Device->GetDevice()->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) {
        LOG_ERROR("[ComputeCulling] CreateCommandList failed");
        return;
    }

    TransitionBuffer(
        commandList.Get(),
        gpuDrawArgs->GetNativeResource(),
        m_drawArgsInIndirectState ? D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT : D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyBufferRegion(
        gpuDrawArgs->GetNativeResource(), 0,
        stagingBuf->GetNativeResource(), 0,
        drawArgsBytes);
    TransitionBuffer(
        commandList.Get(),
        gpuDrawArgs->GetNativeResource(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    TransitionBuffer(
        commandList.Get(),
        countBuf->GetNativeResource(),
        m_countInIndirectState ? D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT : D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyBufferRegion(
        countBuf->GetNativeResource(), 0,
        countStagingBuf->GetNativeResource(), 0,
        sizeof(uint32_t));
    TransitionBuffer(
        commandList.Get(),
        countBuf->GetNativeResource(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    m_countInIndirectState = true;

    commandList->SetComputeRootSignature(m_computeRootSig.Get());
    commandList->SetPipelineState(m_computePSO.Get());
    commandList->SetComputeRootConstantBufferView(0, paramsBuf->GetGPUVirtualAddress());
    commandList->SetComputeRootShaderResourceView(1, inputBuf->GetGPUVirtualAddress());
    commandList->SetComputeRootShaderResourceView(2, metaBuf->GetGPUVirtualAddress());

    TransitionBuffer(
        commandList.Get(),
        gpuInstance->GetNativeResource(),
        m_instanceInVBState ? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER : D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->SetComputeRootUnorderedAccessView(3, gpuInstance->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(4, gpuDrawArgs->GetGPUVirtualAddress());

    const uint32_t groupsX = (maxInstancesPerCmd + CULL_THREAD_GROUP_SIZE - 1u) / CULL_THREAD_GROUP_SIZE;
    const uint32_t groupsY = commandCount;
    commandList->Dispatch(groupsX, groupsY, 1);

    TransitionBuffer(
        commandList.Get(),
        gpuInstance->GetNativeResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_instanceInVBState = true;

    TransitionBuffer(
        commandList.Get(),
        gpuDrawArgs->GetNativeResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    m_drawArgsInIndirectState = true;

    hr = commandList->Close();
    if (FAILED(hr)) {
        LOG_ERROR("[ComputeCulling] Close command list failed");
        return;
    }

    ID3D12CommandList* computeLists[] = { commandList.Get() };
    const uint64_t computeFenceValue = dx12Device->ExecuteComputeCommandLists(computeLists, 1);
    InFlightComputeSubmission submission{};
    submission.allocator = allocator;
    submission.commandList = commandList;
    submission.fenceValue = computeFenceValue;
    m_inFlightSubmissions.push_back(std::move(submission));
    dx12Device->QueueGraphicsWaitForCompute(computeFenceValue);

    rc.activeInstanceBuffer = m_culledInstanceBuffer.get();
    rc.activeDrawArgsBuffer = m_culledDrawArgsBuffer.get();
    rc.activeCountBuffer = m_countBuffer.get();
    rc.activeCountBufferOffset = 0;
    rc.activeMaxDrawCount = commandCount;
    rc.useGpuCulling = true;

    for (size_t i = 0; i < commandIndices.size(); ++i) {
        rc.activeDrawCommands[commandIndices[i]].drawArgsIndex = static_cast<uint32_t>(i);
    }

    static bool s_loggedOnce = false;
    if (!s_loggedOnce) {
        LOG_INFO("[ComputeCulling] async compute dispatch=(%u x %u) commands=%u maxInst=%u",
            groupsX, groupsY, commandCount, maxInstancesPerCmd);
        s_loggedOnce = true;
    }
}
