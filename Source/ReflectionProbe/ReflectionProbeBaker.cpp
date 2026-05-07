#include "ReflectionProbeBaker.h"
#include "Graphics.h"
#include "Render/GlobalRootSignature.h"
#include "Scene/SceneDataUploadSystem.h"
#include "RHI/ICommandList.h"
#include "RHI/DX11/DX11Texture.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12Texture.h"
#include <System/Query.h>
#include <RenderPass/GBufferPass.h>
#include <RenderPass/DeferredLightingPass.h>
#include <RenderPass/SkyboxPass.h>
#include "RenderGraph/FrameGraph.h"
#include "RenderGraph/FrameGraphResources.h"
#include <wrl.h>
#include <d3d11.h>
#include <d3d12.h>

DirectX::XMMATRIX ReflectionProbeBaker::GetViewMatrixForFace(const DirectX::XMFLOAT3& pos, int faceIndex)
{
    using namespace DirectX;
    XMVECTOR eye = XMLoadFloat3(&pos);
    XMVECTOR lookAt, up;

    switch (faceIndex) {
    case 0: lookAt = eye + XMVectorSet(1, 0, 0, 0); up = XMVectorSet(0, 1, 0, 0); break; // +X
    case 1: lookAt = eye + XMVectorSet(-1, 0, 0, 0); up = XMVectorSet(0, 1, 0, 0); break; // -X
    case 2: lookAt = eye + XMVectorSet(0, 1, 0, 0); up = XMVectorSet(0, 0, -1, 0); break; // +Y
    case 3: lookAt = eye + XMVectorSet(0, -1, 0, 0); up = XMVectorSet(0, 0, 1, 0); break; // -Y
    case 4: lookAt = eye + XMVectorSet(0, 0, 1, 0); up = XMVectorSet(0, 1, 0, 0); break; // +Z
    case 5: lookAt = eye + XMVectorSet(0, 0, -1, 0); up = XMVectorSet(0, 1, 0, 0); break; // -Z
    default: return XMMatrixIdentity();
    }
    return XMMatrixLookAtLH(eye, lookAt, up);
}

void ReflectionProbeBaker::Bake(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc)
{
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        BakeDX12(probe, queue, rc);
    } else {
        BakeDX11(probe, queue, rc);
    }
}

void ReflectionProbeBaker::BakeDX11(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc)
{
    Graphics& g = Graphics::Instance();
    ID3D11Device* device = g.GetDevice();
    if (!device) return;

    auto dc = rc.commandList->GetNativeContext();
    FrameBuffer* sceneBuffer = g.GetFrameBuffer(FrameBufferId::Scene);
    if (!sceneBuffer) return;

    auto sceneTex = static_cast<DX11Texture*>(sceneBuffer->GetColorTexture());
    ID3D11ShaderResourceView* sceneSRV = sceneTex ? sceneTex->GetNativeSRV() : nullptr;

    if (!sceneSRV) return;

    Microsoft::WRL::ComPtr<ID3D11Resource> srcRes;
    sceneSRV->GetResource(srcRes.GetAddressOf());

    D3D11_TEXTURE2D_DESC srcDesc;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTex;
    srcRes.As(&srcTex);
    srcTex->GetDesc(&srcDesc);

    UINT cubeSize = srcDesc.Height;
    UINT cropLeft = (srcDesc.Width - srcDesc.Height) / 2;

    if (!probe.cubemapSRV) {
        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = cubeSize;
        texDesc.Height = cubeSize;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 6;
        texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> cubemapTex;
        device->CreateTexture2D(&texDesc, nullptr, cubemapTex.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        device->CreateShaderResourceView(cubemapTex.Get(), &srvDesc, probe.cubemapSRV.ReleaseAndGetAddressOf());
    }

    Microsoft::WRL::ComPtr<ID3D11Resource> cubemapRes;
    probe.cubemapSRV->GetResource(cubemapRes.GetAddressOf());

    GBufferPass gbufferPass;
    DeferredLightingPass lightingPass(g.GetResourceFactory());
    SkyboxPass skyboxPass;

    auto originalView = rc.viewMatrix;
    auto originalProj = rc.projectionMatrix;
    auto originalUnjittered = rc.viewProjectionUnjittered;
    auto originalPos = rc.cameraPosition;
    float originalAspect = rc.aspect;
    float originalFovY = rc.fovY;

    float clearColor[4] = { 0, 0, 0, 0 };
    if (auto* ssgi = g.GetFrameBuffer(FrameBufferId::SSGIBlur)) ssgi->Clear(rc.commandList, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    if (auto* ssr = g.GetFrameBuffer(FrameBufferId::SSRBlur))   ssr->Clear(rc.commandList, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    float aspect = (float)srcDesc.Width / (float)srcDesc.Height;
    DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.0f), aspect, 0.1f, 1000.0f);
    DirectX::XMStoreFloat4x4(&rc.projectionMatrix, projMatrix);
    rc.cameraPosition = probe.position;
    rc.aspect = aspect;
    rc.fovY = DirectX::XMConvertToRadians(90.0f);

    SceneDataUploadSystem uploadSystem;

    D3D11_BOX srcBox;
    srcBox.left = cropLeft; srcBox.right = cropLeft + cubeSize;
    srcBox.top = 0;         srcBox.bottom = cubeSize;
    srcBox.front = 0;       srcBox.back = 1;

    for (int i = 0; i < 6; ++i)
    {
        DirectX::XMMATRIX viewMatrix = GetViewMatrixForFace(probe.position, i);
        DirectX::XMStoreFloat4x4(&rc.viewMatrix, viewMatrix);
        DirectX::XMStoreFloat4x4(&rc.viewProjectionUnjittered, viewMatrix * projMatrix);

        uploadSystem.Upload(rc, GlobalRootSignature::Instance());

        GlobalRootSignature::Instance().BindAll(rc.commandList, rc.renderState, rc.shadowMap);

        FrameGraph dummyGraph;
        FrameGraphResources dummyResources(dummyGraph);

        gbufferPass.Execute(dummyResources, queue, rc);
        lightingPass.Execute(dummyResources, queue, rc);
        skyboxPass.Execute(dummyResources, queue, rc);

        dc->CopySubresourceRegion(cubemapRes.Get(), i, 0, 0, 0, srcRes.Get(), 0, &srcBox);
    }

    rc.viewMatrix = originalView;
    rc.projectionMatrix = originalProj;
    rc.viewProjectionUnjittered = originalUnjittered;
    rc.cameraPosition = originalPos;
    rc.aspect = originalAspect;
    rc.fovY = originalFovY;

    probe.needsBake = false;
}

void ReflectionProbeBaker::BakeDX12(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc)
{
    Graphics& g = Graphics::Instance();
    DX12Device* dx12Device = g.GetDX12Device();
    if (!dx12Device) return;

    FrameBuffer* sceneBuffer = g.GetFrameBuffer(FrameBufferId::Scene);
    if (!sceneBuffer) return;

    auto* sceneTex = static_cast<DX12Texture*>(sceneBuffer->GetColorTexture());
    if (!sceneTex) return;
    ID3D12Resource* sceneResource = sceneTex->GetNativeResource();
    if (!sceneResource) return;

    D3D12_RESOURCE_DESC sceneDesc = sceneResource->GetDesc();
    const UINT cubeSize = static_cast<UINT>(sceneDesc.Height);
    if (cubeSize == 0) return;
    const UINT cropLeft = (static_cast<UINT>(sceneDesc.Width) > sceneDesc.Height)
        ? (static_cast<UINT>(sceneDesc.Width) - cubeSize) / 2
        : 0;

    auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
    if (!dx12Cmd) return;
    ID3D12GraphicsCommandList* nativeCmd = dx12Cmd->GetNativeCommandList();
    if (!nativeCmd) return;

    // Lazy-create the cubemap texture (R16G16B16A16_FLOAT, 6 array slices,
    // committed in DEFAULT heap). We wrap it with the existing DX12Texture
    // file-loaded constructor to get a TextureCube SRV; the SRV starts in
    // ShaderResource state in that wrapper, so we override to CopyDest below.
    DX12Texture* cubemapDx12 = nullptr;
    if (!probe.cubemapTexture) {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = cubeSize;
        desc.Height = cubeSize;
        desc.DepthOrArraySize = 6;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;

        Microsoft::WRL::ComPtr<ID3D12Resource> cubemapRes;
        HRESULT hr = dx12Device->GetDevice()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&cubemapRes));
        if (FAILED(hr) || !cubemapRes) return;

        auto wrapper = std::make_shared<DX12Texture>(
            dx12Device, cubemapRes, cubeSize, cubeSize,
            DXGI_FORMAT_R16G16B16A16_FLOAT, true /*isCubemap*/);
        wrapper->SetCurrentState(ResourceState::CopyDest);
        probe.cubemapTexture = wrapper;
    }
    cubemapDx12 = static_cast<DX12Texture*>(probe.cubemapTexture.get());
    if (!cubemapDx12) return;
    ID3D12Resource* cubemapResource = cubemapDx12->GetNativeResource();
    if (!cubemapResource) return;

    // Save and override RenderContext camera state.
    auto originalView = rc.viewMatrix;
    auto originalProj = rc.projectionMatrix;
    auto originalUnjittered = rc.viewProjectionUnjittered;
    auto originalPos = rc.cameraPosition;
    float originalAspect = rc.aspect;
    float originalFovY = rc.fovY;

    float clearColor[4] = { 0, 0, 0, 0 };
    if (auto* ssgi = g.GetFrameBuffer(FrameBufferId::SSGIBlur)) ssgi->Clear(rc.commandList, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    if (auto* ssr = g.GetFrameBuffer(FrameBufferId::SSRBlur))   ssr->Clear(rc.commandList, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    const float aspect = static_cast<float>(sceneDesc.Width) / static_cast<float>(sceneDesc.Height);
    DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(90.0f), aspect, 0.1f, 1000.0f);
    DirectX::XMStoreFloat4x4(&rc.projectionMatrix, projMatrix);
    rc.cameraPosition = probe.position;
    rc.aspect = aspect;
    rc.fovY = DirectX::XMConvertToRadians(90.0f);

    SceneDataUploadSystem uploadSystem;
    GBufferPass gbufferPass;
    DeferredLightingPass lightingPass(g.GetResourceFactory());
    SkyboxPass skyboxPass;

    for (int i = 0; i < 6; ++i)
    {
        DirectX::XMMATRIX viewMatrix = GetViewMatrixForFace(probe.position, i);
        DirectX::XMStoreFloat4x4(&rc.viewMatrix, viewMatrix);
        DirectX::XMStoreFloat4x4(&rc.viewProjectionUnjittered, viewMatrix * projMatrix);

        uploadSystem.Upload(rc, GlobalRootSignature::Instance());
        GlobalRootSignature::Instance().BindAll(rc.commandList, rc.renderState, rc.shadowMap);

        FrameGraph dummyGraph;
        FrameGraphResources dummyResources(dummyGraph);

        gbufferPass.Execute(dummyResources, queue, rc);
        lightingPass.Execute(dummyResources, queue, rc);
        skyboxPass.Execute(dummyResources, queue, rc);

        // Copy scene color (cropped center square) into face slice [i].
        // Transition states to allow the copy, then restore the scene as a
        // render target for the next iteration.
        dx12Cmd->TransitionBarrier(sceneTex, ResourceState::CopySource);
        dx12Cmd->TransitionBarrier(cubemapDx12, ResourceState::CopyDest);
        dx12Cmd->FlushResourceBarriers();

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = cubemapResource;
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = static_cast<UINT>(i);

        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = sceneResource;
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;

        D3D12_BOX srcBox = {};
        srcBox.left = cropLeft;
        srcBox.top = 0;
        srcBox.front = 0;
        srcBox.right = cropLeft + cubeSize;
        srcBox.bottom = cubeSize;
        srcBox.back = 1;

        nativeCmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

        // Put the scene texture back into RenderTarget so the next pass /
        // next iteration can write to it.
        dx12Cmd->TransitionBarrier(sceneTex, ResourceState::RenderTarget);
    }

    // Final transition: cubemap is now sampled by lighting passes.
    dx12Cmd->TransitionBarrier(cubemapDx12, ResourceState::ShaderResource);
    dx12Cmd->FlushResourceBarriers();

    rc.viewMatrix = originalView;
    rc.projectionMatrix = originalProj;
    rc.viewProjectionUnjittered = originalUnjittered;
    rc.cameraPosition = originalPos;
    rc.aspect = originalAspect;
    rc.fovY = originalFovY;

    probe.needsBake = false;
}

void ReflectionProbeBaker::BakeAllDirtyProbes(Registry& registry, const RenderQueue& queue, RenderContext& rc)
{
    Query<ReflectionProbeComponent> query(registry);
    query.ForEach([this, &queue, &rc](ReflectionProbeComponent& probe) {
        if (probe.needsBake) {
            this->Bake(probe, queue, rc);
        }
        });
}
