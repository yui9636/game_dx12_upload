#include "ComputeCullingPass.h"
#include "Graphics.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12Buffer.h"
#include "RHI/DX12/DX12CommandList.h"
#include "Render/GlobalRootSignature.h"
#include "Model/ModelResource.h"
#include "RenderContext/IndirectDrawCommon.h"
#include "Console/Logger.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <cassert>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// CullingParams CBV — must match HLSL
struct CullingParams {
    XMFLOAT4 frustumPlanes[6];   // 96 bytes
    uint32_t commandCount;        // 4 bytes
    uint32_t maxInstancesPerCmd;  // 4 bytes
    uint32_t pad[2];              // 8 bytes → 112 bytes
};

// ─── Init ───

void ComputeCullingPass::InitGpuResources(DX12Device* device) {
    if (m_initialized) return;
    auto* d3dDevice = device->GetDevice();

    // Compute root signature: b0(CBV) t0(SRV) t1(SRV) u0(UAV) u1(UAV)
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

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeVersionedRootSignature(&rsDesc, &blob, &error);
    if (FAILED(hr)) {
        if (error) LOG_ERROR("[ComputeCulling] RootSig serialize: %s", (const char*)error->GetBufferPointer());
        return;
    }
    hr = d3dDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSig));
    if (FAILED(hr)) { LOG_ERROR("[ComputeCulling] CreateRootSignature failed"); return; }

    // Load compute shader
    ComPtr<ID3DBlob> csBlob;
    hr = D3DReadFileToBlob(L"Data/Shader/FrustumCullCS.cso", &csBlob);
    if (FAILED(hr)) { LOG_ERROR("[ComputeCulling] Failed to load FrustumCullCS.cso"); return; }

    // Compute PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_computeRootSig.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
    hr = d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_computePSO));
    if (FAILED(hr)) { LOG_ERROR("[ComputeCulling] CreateComputePipelineState failed"); return; }

    m_initialized = true;
    LOG_INFO("[ComputeCulling] GPU resources initialized");
}

// ─── Frustum extraction (Gribb-Hartmann) ───

void ComputeCullingPass::ExtractFrustumPlanes(const XMFLOAT4X4& vp, XMFLOAT4 planes[6]) {
    const float* m = reinterpret_cast<const float*>(&vp);
    planes[0] = { m[3]+m[0], m[7]+m[4], m[11]+m[8],  m[15]+m[12] }; // Left
    planes[1] = { m[3]-m[0], m[7]-m[4], m[11]-m[8],  m[15]-m[12] }; // Right
    planes[2] = { m[3]+m[1], m[7]+m[5], m[11]+m[9],  m[15]+m[13] }; // Bottom
    planes[3] = { m[3]-m[1], m[7]-m[5], m[11]-m[9],  m[15]-m[13] }; // Top
    planes[4] = { m[2],      m[6],      m[10],        m[14]       }; // Near
    planes[5] = { m[3]-m[2], m[7]-m[6], m[11]-m[10],  m[15]-m[14] }; // Far

    for (int i = 0; i < 6; i++) {
        XMVECTOR p = XMLoadFloat4(&planes[i]);
        float len = XMVectorGetX(XMVector3Length(p));
        if (len > 1e-6f) {
            p = XMVectorScale(p, 1.0f / len);
            XMStoreFloat4(&planes[i], p);
        }
    }
}

// ─── Setup / Execute ───

void ComputeCullingPass::Setup(FrameGraphBuilder& builder) {
    (void)builder;
}

void ComputeCullingPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    (void)resources;
    (void)queue;

    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) return;
    if (rc.activeDrawCommands.empty()) return;

    auto& graphics = Graphics::Instance();
    auto* dx12Device = graphics.GetDX12Device();
    if (!dx12Device) return;

    InitGpuResources(dx12Device);
    if (!m_initialized) return;

    auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
    if (!dx12Cmd) return;
    auto* factory = graphics.GetResourceFactory();

    // ─── Build CullCommandMeta + initial DrawArgs on CPU ───
    // One entry per activeDrawCommand (non-skinned only)
    std::vector<CullCommandMeta> metaEntries;
    std::vector<DrawArgs> initialDrawArgs;
    std::vector<size_t> cmdIndices; // map meta entry → activeDrawCommands index

    uint32_t outputStart = 0;
    uint32_t maxInstancesPerCmd = 0;

    for (size_t ci = 0; ci < rc.activeDrawCommands.size(); ++ci) {
        const auto& cmd = rc.activeDrawCommands[ci];
        if (!cmd.supportsInstancing || cmd.instanceCount == 0) continue;

        const auto* meshRes = cmd.modelResource ? cmd.modelResource->GetMeshResource(static_cast<int>(cmd.meshIndex)) : nullptr;
        if (!meshRes) continue;

        // Bounds from model
        float cx = 0, cy = 0, cz = 0, radius = 5.0f;
        if (cmd.modelResource) {
            auto bounds = cmd.modelResource->GetLocalBounds();
            cx = bounds.Center.x;
            cy = bounds.Center.y;
            cz = bounds.Center.z;
            auto& ext = bounds.Extents;
            radius = sqrtf(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z);
        }

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
        args.instanceCount = 0; // filled by compute
        args.startIndexLocation = 0;
        args.baseVertexLocation = 0;
        args.startInstanceLocation = outputStart;
        initialDrawArgs.push_back(args);

        cmdIndices.push_back(ci);
        if (cmd.instanceCount > maxInstancesPerCmd) maxInstancesPerCmd = cmd.instanceCount;
        outputStart += cmd.instanceCount;
    }

    if (metaEntries.empty()) return;

    const uint32_t commandCount = static_cast<uint32_t>(metaEntries.size());
    const uint32_t totalOutputSlots = outputStart;

    // ─── Transition buffers from previous frame ───
    if (m_culledInstanceBuffer && m_instanceInVBState) {
        auto* buf = static_cast<DX12Buffer*>(m_culledInstanceBuffer.get());
        dx12Cmd->BufferBarrier(buf->GetNativeResource(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_instanceInVBState = false;
    }
    if (m_culledDrawArgsBuffer && m_drawArgsInIndirectState) {
        auto* buf = static_cast<DX12Buffer*>(m_culledDrawArgsBuffer.get());
        dx12Cmd->BufferBarrier(buf->GetNativeResource(),
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_drawArgsInIndirectState = false;
    }

    // ─── Ensure GPU buffers (grow only) ───
    if (!m_culledInstanceBuffer || m_instanceCapacity < totalOutputSlots) {
        uint32_t cap = ((totalOutputSlots + 255) / 256) * 256;
        m_culledInstanceBuffer = factory->CreateBuffer(
            cap * INSTANCE_DATA_STRIDE, BufferType::UAVStorage, nullptr);
        m_instanceCapacity = cap;
        m_instanceInVBState = false;
    }

    const uint32_t drawArgsBytes = commandCount * DRAW_ARGS_STRIDE;
    if (!m_culledDrawArgsBuffer || m_drawArgsCapacity < commandCount) {
        uint32_t cap = ((drawArgsBytes + 255) / 256) * 256;
        m_culledDrawArgsBuffer = factory->CreateBuffer(
            cap, BufferType::UAVStorage, nullptr);
        m_drawArgsCapacity = commandCount;
        m_drawArgsInIndirectState = false;
    }

    // ─── Upload initial DrawArgs via staging buffer (IBuffer, UPLOAD) ───
    if (!m_stagingBuffer || m_stagingCapacity < drawArgsBytes) {
        uint32_t cap = ((drawArgsBytes + 255) / 256) * 256;
        m_stagingBuffer = factory->CreateBuffer(cap, BufferType::Vertex, nullptr);
        m_stagingCapacity = cap;
    }
    {
        auto* staging = static_cast<DX12Buffer*>(m_stagingBuffer.get());
        void* mapped = staging->Map();
        if (!mapped) { LOG_ERROR("[ComputeCulling] staging Map failed"); return; }
        memcpy(mapped, initialDrawArgs.data(), drawArgsBytes);
        staging->Unmap();
    }

    // Copy staging → culledDrawArgs
    auto* gpuDrawArgs = static_cast<DX12Buffer*>(m_culledDrawArgsBuffer.get());
    auto* gpuInstance = static_cast<DX12Buffer*>(m_culledInstanceBuffer.get());

    dx12Cmd->BufferBarrier(gpuDrawArgs->GetNativeResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
    dx12Cmd->CopyBufferRegion(
        gpuDrawArgs->GetNativeResource(), 0,
        static_cast<DX12Buffer*>(m_stagingBuffer.get())->GetNativeResource(), 0, drawArgsBytes);
    dx12Cmd->BufferBarrier(gpuDrawArgs->GetNativeResource(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // ─── Upload CullCommandMeta via IBuffer (UPLOAD) ───
    const uint32_t metaBytes = commandCount * static_cast<uint32_t>(sizeof(CullCommandMeta));
    if (!m_cullMetaBuffer || m_cullMetaCapacity < metaBytes) {
        uint32_t cap = ((metaBytes + 255) / 256) * 256;
        m_cullMetaBuffer = factory->CreateBuffer(cap, BufferType::Vertex, nullptr);
        m_cullMetaCapacity = cap;
    }
    {
        auto* meta = static_cast<DX12Buffer*>(m_cullMetaBuffer.get());
        void* mapped = meta->Map();
        if (!mapped) { LOG_ERROR("[ComputeCulling] meta Map failed"); return; }
        memcpy(mapped, metaEntries.data(), metaBytes);
        meta->Unmap();
    }

    // ─── Upload CullingParams via dynamic CB ring ───
    CullingParams cbParams{};
    XMMATRIX V = XMLoadFloat4x4(&rc.viewMatrix);
    XMMATRIX P = XMLoadFloat4x4(&rc.projectionMatrix);
    XMFLOAT4X4 vp;
    XMStoreFloat4x4(&vp, V * P);
    ExtractFrustumPlanes(vp, cbParams.frustumPlanes);
    cbParams.commandCount = commandCount;
    cbParams.maxInstancesPerCmd = maxInstancesPerCmd;

    auto cbAlloc = dx12Cmd->AllocateDynamicConstantBuffer(&cbParams, sizeof(cbParams));

    // ─── Dispatch compute (2D: X=instances, Y=commands) ───
    auto* inputBuf = static_cast<DX12Buffer*>(rc.preparedInstanceBuffer.get());
    auto* metaBuf = static_cast<DX12Buffer*>(m_cullMetaBuffer.get());

    dx12Cmd->SetComputeRootSignature(m_computeRootSig.Get());
    dx12Cmd->SetComputePipelineState(m_computePSO.Get());
    dx12Cmd->SetComputeRootCBV(0, cbAlloc.gpuVA);
    dx12Cmd->SetComputeRootSRV(1, inputBuf->GetGPUVirtualAddress());
    dx12Cmd->SetComputeRootSRV(2, metaBuf->GetGPUVirtualAddress());
    dx12Cmd->SetComputeRootUAV(3, gpuInstance->GetGPUVirtualAddress());
    dx12Cmd->SetComputeRootUAV(4, gpuDrawArgs->GetGPUVirtualAddress());

    uint32_t groupsX = (maxInstancesPerCmd + CULL_THREAD_GROUP_SIZE - 1) / CULL_THREAD_GROUP_SIZE;
    uint32_t groupsY = commandCount;
    dx12Cmd->Dispatch(groupsX, groupsY, 1);

    // ─── Transition outputs for rendering ───
    dx12Cmd->BufferBarrier(gpuInstance->GetNativeResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_instanceInVBState = true;

    dx12Cmd->BufferBarrier(gpuDrawArgs->GetNativeResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    m_drawArgsInIndirectState = true;

    // ─── Overwrite rc.active* so renderers see culled data ───
    rc.activeInstanceBuffer = m_culledInstanceBuffer.get();
    rc.activeDrawArgsBuffer = m_culledDrawArgsBuffer.get();

    // Update drawArgsIndex in activeDrawCommands
    for (size_t i = 0; i < cmdIndices.size(); ++i) {
        rc.activeDrawCommands[cmdIndices[i]].drawArgsIndex = static_cast<uint32_t>(i);
    }

    // ─── Restore graphics pipeline ───
    dx12Cmd->RestoreDescriptorHeap();
    GlobalRootSignature::Instance().BindAll(rc.commandList, rc.renderState, rc.shadowMap);

    static bool s_loggedOnce = false;
    if (!s_loggedOnce) {
        LOG_INFO("[ComputeCulling] 2D dispatch (%u x %u) for %u commands, maxInst=%u",
            groupsX, groupsY, commandCount, maxInstancesPerCmd);
        s_loggedOnce = true;
    }
}
