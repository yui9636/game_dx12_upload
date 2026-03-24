#include "OffscreenRenderer.h"
#include "ShaderCommon.h"
#include "Graphics.h"
#include "FrameBuffer.h"
#include "RHI/ICommandList.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RHI/GraphicsAPI.h"
#include "RHI/DX11/DX11CommandList.h"
#include "RHI/DX11/DX11Buffer.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Buffer.h"
#include "RHI/DX12/DX12RootSignature.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/ITexture.h"
#include "RenderContext/RenderState.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "Console/Logger.h"

static ID3D12Fence* AsFence(void* p) { return static_cast<ID3D12Fence*>(p); }
static HANDLE AsHandle(void* p) { return static_cast<HANDLE>(p); }

OffscreenRenderer::OffscreenRenderer() = default;
OffscreenRenderer::~OffscreenRenderer() {
    if (m_fenceEvent) {
        auto* fence = AsFence(m_fencePtr);
        HANDLE hEvent = AsHandle(m_fenceEvent);
        if (fence && fence->GetCompletedValue() < m_fenceValue) {
            fence->SetEventOnCompletion(m_fenceValue, hEvent);
            WaitForSingleObjectEx(hEvent, 5000, FALSE);
        }
        CloseHandle(hEvent);
        m_fenceEvent = nullptr;
    }
    if (m_fencePtr) {
        AsFence(m_fencePtr)->Release();
        m_fencePtr = nullptr;
    }
}

bool OffscreenRenderer::Initialize()
{
    m_available = false;
    m_commandList.reset();
    m_dx12RootSignature.reset();
    m_renderer.reset();
    m_localSceneBuffer.reset();

    Graphics& graphics = Graphics::Instance();
    auto* factory = graphics.GetResourceFactory();
    if (!factory) {
        LOG_ERROR("[OffscreenRenderer] Resource factory unavailable.");
        return false;
    }

    m_renderer = std::make_unique<ModelRenderer>(factory);

    if (graphics.GetAPI() == GraphicsAPI::DX12) {
        auto* device = graphics.GetDX12Device();
        if (!device) {
            LOG_ERROR("[OffscreenRenderer] DX12 device unavailable.");
            return false;
        }
        m_dx12RootSignature = std::make_unique<DX12RootSignature>(device);
        m_commandList = std::make_unique<DX12CommandList>(device, m_dx12RootSignature.get(), false);
        m_localSceneBuffer = std::make_unique<DX12Buffer>(device, sizeof(CbScene), BufferType::Constant);

        ID3D12Fence* fence = nullptr;
        HRESULT fhr = device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(fhr)) {
            LOG_ERROR("[OffscreenRenderer] Failed to create fence.");
            return false;
        }
        m_fencePtr = fence;
        m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        m_fenceValue = 0;

        m_available = true;
        LOG_INFO("[OffscreenRenderer] Initialized (DX12).");
        return true;
    }

    auto* context = graphics.GetDeviceContext();
    if (!context) {
        LOG_ERROR("[OffscreenRenderer] DX11 device context unavailable.");
        return false;
    }
    auto* device11 = graphics.GetDevice();
    m_commandList = std::make_unique<DX11CommandList>(context);
    m_localSceneBuffer = std::make_unique<DX11Buffer>(device11, sizeof(CbScene), BufferType::Constant);
    m_available = true;
    LOG_INFO("[OffscreenRenderer] Initialized (DX11).");
    return true;
}

std::shared_ptr<FrameBuffer> OffscreenRenderer::CreateFrameBuffer(int w, int h,
    float clearR, float clearG, float clearB, float clearA)
{
    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return nullptr;
    float clearColor[4] = { clearR, clearG, clearB, clearA };
    return std::make_shared<FrameBuffer>(
        factory, w, h,
        std::vector<TextureFormat>{ TextureFormat::R16G16B16A16_FLOAT },
        TextureFormat::D32_FLOAT,
        clearColor);
}

void OffscreenRenderer::BeginJob()
{
    // Full state reset for shared usage  Edelegates to Begin() which
    // resets command allocator, descriptor heap, root signature, PSO dirty flag.
    // UploadScene() overwrites all CbScene fields, so no zero-clear needed.
    Begin();
}

void OffscreenRenderer::Begin()
{
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* fence = AsFence(m_fencePtr);
        if (fence && fence->GetCompletedValue() < m_fenceValue) {
            fence->SetEventOnCompletion(m_fenceValue, AsHandle(m_fenceEvent));
            WaitForSingleObjectEx(AsHandle(m_fenceEvent), INFINITE, FALSE);
        }
        auto* dx12Cmd = static_cast<DX12CommandList*>(m_commandList.get());
        dx12Cmd->Begin();
    }
}

void OffscreenRenderer::Clear(FrameBuffer* fb, float r, float g, float b, float a)
{
    fb->Clear(m_commandList.get(), r, g, b, a);
}

void OffscreenRenderer::SetRenderTarget(FrameBuffer* fb)
{
    fb->SetRenderTargets(m_commandList.get());
}

void OffscreenRenderer::SetViewport(float w, float h)
{
    m_commandList->SetViewport(RhiViewport(0.0f, 0.0f, w, h));
}

void OffscreenRenderer::UploadScene(
    const DirectX::XMFLOAT4X4& viewProj,
    const DirectX::XMFLOAT3& camPos,
    const DirectX::XMFLOAT3& lightDir,
    const DirectX::XMFLOAT3& lightColor,
    float renderW, float renderH)
{
    CbScene scene{};
    scene.viewProjection = viewProj;
    scene.viewProjectionUnjittered = viewProj;
    scene.prevViewProjection = viewProj;
    scene.lightDirection = { lightDir.x, lightDir.y, lightDir.z, 0.0f };
    scene.lightColor     = { lightColor.x, lightColor.y, lightColor.z, 1.0f };
    scene.cameraPosition = { camPos.x, camPos.y, camPos.z, 1.0f };
    scene.shadowColor    = { 1.0f, 1.0f, 1.0f, 1.0f };
    scene.renderW = renderW;
    scene.renderH = renderH;

    m_commandList->UpdateBuffer(m_localSceneBuffer.get(), &scene, sizeof(scene));
}

void OffscreenRenderer::BindScene()
{
    m_commandList->VSSetConstantBuffer(7, m_localSceneBuffer.get());
    m_commandList->PSSetConstantBuffer(7, m_localSceneBuffer.get());
}

void OffscreenRenderer::BindSampler()
{
    const RenderState* rs = Graphics::Instance().GetRenderState();
    if (!rs) return;
    ISampler* linearSampler = rs->GetSamplerState(SamplerState::LinearWrap);
    m_commandList->PSSetSampler(0, linearSampler);
}

void OffscreenRenderer::Submit(FrameBuffer* fb)
{
    RenderContext rc = {};
    rc.commandList = m_commandList.get();
    rc.renderState = Graphics::Instance().GetRenderState();
    rc.shadowMap = nullptr;
    rc.mainRenderTarget = fb->GetColorTexture(0);
    rc.mainDepthStencil = fb->GetDepthTexture();

    RenderQueue emptyQueue;
    m_renderer->Render(rc, emptyQueue);

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        m_commandList->TransitionBarrier(fb->GetColorTexture(0), ResourceState::ShaderResource);
        auto* dx12Cmd = static_cast<DX12CommandList*>(m_commandList.get());
        dx12Cmd->FlushResourceBarriers();
        dx12Cmd->End();
        dx12Cmd->Submit();

        Graphics::Instance().GetDX12Device()->GetCommandQueue()->Signal(AsFence(m_fencePtr), m_fenceValue);
    }
}

void OffscreenRenderer::ClearExternalRT(ITexture* color, ITexture* depth,
                                         float r, float g, float b, float a)
{
    float clearColor[4] = { r, g, b, a };
    m_commandList->TransitionBarrier(color, ResourceState::RenderTarget);
    m_commandList->TransitionBarrier(depth, ResourceState::DepthWrite);
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(m_commandList.get());
        dx12Cmd->FlushResourceBarriers();
    }
    m_commandList->ClearColor(color, clearColor);
    m_commandList->ClearDepthStencil(depth, 1.0f, 0);
}

void OffscreenRenderer::SetExternalRenderTarget(ITexture* color, ITexture* depth)
{
    ITexture* rts[] = { color };
    m_commandList->SetRenderTargets(1, rts, depth);
}

void OffscreenRenderer::SubmitDirect(ITexture* color)
{
    RenderContext rc = {};
    rc.commandList = m_commandList.get();
    rc.renderState = Graphics::Instance().GetRenderState();
    rc.shadowMap = nullptr;
    rc.mainRenderTarget = color;
    rc.mainDepthStencil = nullptr;

    RenderQueue emptyQueue;
    m_renderer->Render(rc, emptyQueue);

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        m_commandList->TransitionBarrier(color, ResourceState::ShaderResource);
        auto* dx12Cmd = static_cast<DX12CommandList*>(m_commandList.get());
        dx12Cmd->FlushResourceBarriers();
        dx12Cmd->End();
        dx12Cmd->Submit();

        ++m_fenceValue;
        Graphics::Instance().GetDX12Device()->GetCommandQueue()->Signal(AsFence(m_fencePtr), m_fenceValue);
    }
}

uint64_t OffscreenRenderer::GetCompletedFenceValue() const
{
    auto* fence = AsFence(m_fencePtr);
    return fence ? fence->GetCompletedValue() : m_fenceValue;
}

bool OffscreenRenderer::IsGpuIdle() const
{
    auto* fence = AsFence(m_fencePtr);
    if (!fence) return true;
    return fence->GetCompletedValue() >= m_fenceValue;
}
