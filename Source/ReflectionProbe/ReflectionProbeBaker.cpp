//#include "ReflectionProbeBaker.h"
//#include "Graphics.h"
//#include "RenderPass/GBufferPass.h"
//#include "RenderPass/DeferredLightingPass.h"
//#include "RenderPass/SkyboxPass.h"
//#include "Scene/SceneDataUploadSystem.h"
//#include "Render/GlobalRootSignature.h"
//#include "Query.h"
//#include <System\Query.h>
//#include "RHI/ICommandList.h"
//
//ReflectionProbeBaker::ReflectionProbeBaker(ID3D11Device* device) : m_device(device) {}
//
//DirectX::XMMATRIX ReflectionProbeBaker::GetViewMatrixForFace(const DirectX::XMFLOAT3& pos, int faceIndex)
//{
//    using namespace DirectX;
//    XMVECTOR eye = XMLoadFloat3(&pos);
//    XMVECTOR lookAt, up;
//
//    switch (faceIndex) {
//    case 0: lookAt = eye + XMVectorSet(1, 0, 0, 0); up = XMVectorSet(0, 1, 0, 0); break; // +X
//    case 1: lookAt = eye + XMVectorSet(-1, 0, 0, 0); up = XMVectorSet(0, 1, 0, 0); break; // -X
//    case 2: lookAt = eye + XMVectorSet(0, 1, 0, 0); up = XMVectorSet(0, 0, -1, 0); break; // +Y
//    case 3: lookAt = eye + XMVectorSet(0, -1, 0, 0); up = XMVectorSet(0, 0, 1, 0); break; // -Y
//    case 4: lookAt = eye + XMVectorSet(0, 0, 1, 0); up = XMVectorSet(0, 1, 0, 0); break; // +Z
//    case 5: lookAt = eye + XMVectorSet(0, 0, -1, 0); up = XMVectorSet(0, 1, 0, 0); break; // -Z
//    default: return XMMatrixIdentity();
//    }
//    return XMMatrixLookAtLH(eye, lookAt, up);
//}
//
//void ReflectionProbeBaker::Bake(ReflectionProbeComponent& probe, const RenderQueue& queue, RenderContext& rc)
//{
//    Graphics& g = Graphics::Instance();
//    auto dc = rc.commandList->GetNativeContext();
//    FrameBuffer* sceneBuffer = g.GetFrameBuffer(FrameBufferId::Scene);
//
//    Microsoft::WRL::ComPtr<ID3D11Resource> srcRes;
//    sceneBuffer->GetColorMap()->GetResource(srcRes.GetAddressOf());
//    D3D11_TEXTURE2D_DESC srcDesc;
//    Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTex;
//    srcRes.As(&srcTex);
//    srcTex->GetDesc(&srcDesc);
//
//    UINT cubeSize = srcDesc.Height;
//    UINT cropLeft = (srcDesc.Width - srcDesc.Height) / 2;
//
//    // =========================================================
//    // =========================================================
//    if (!probe.cubemapSRV) {
//        D3D11_TEXTURE2D_DESC texDesc{};
//        texDesc.Width = cubeSize;
//        texDesc.Height = cubeSize;
//        texDesc.MipLevels = 1;
//        texDesc.ArraySize = 6;
//        texDesc.SampleDesc.Count = 1;
//        texDesc.Usage = D3D11_USAGE_DEFAULT;
//        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
//        texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
//
//        Microsoft::WRL::ComPtr<ID3D11Texture2D> cubemapTex;
//        m_device->CreateTexture2D(&texDesc, nullptr, cubemapTex.GetAddressOf());
//
//        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
//        srvDesc.Format = texDesc.Format;
//        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
//        srvDesc.TextureCube.MipLevels = 1;
//        m_device->CreateShaderResourceView(cubemapTex.Get(), &srvDesc, probe.cubemapSRV.ReleaseAndGetAddressOf());
//    }
//
//    Microsoft::WRL::ComPtr<ID3D11Resource> cubemapRes;
//    probe.cubemapSRV->GetResource(cubemapRes.GetAddressOf());
//
//    // =========================================================
//    // =========================================================
//    GBufferPass gbufferPass;
//    DeferredLightingPass lightingPass(Graphics::Instance().GetResourceFactory());
//    SkyboxPass skyboxPass;
//
//    auto originalView = rc.viewMatrix;
//    auto originalProj = rc.projectionMatrix;
//    auto originalUnjittered = rc.viewProjectionUnjittered;
//    auto originalPos = rc.cameraPosition;
//    float originalAspect = rc.aspect;
//    float originalFovY = rc.fovY;
//
//    float clearColor[4] = { 0, 0, 0, 0 };
//    if (auto* ssgi = g.GetFrameBuffer(FrameBufferId::SSGIBlur)) ssgi->Clear(dc, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
//    if (auto* ssr = g.GetFrameBuffer(FrameBufferId::SSRBlur))   ssr->Clear(dc, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
//
//    float aspect = (float)srcDesc.Width / (float)srcDesc.Height;
//    DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.0f), aspect, 0.1f, 1000.0f);
//    DirectX::XMStoreFloat4x4(&rc.projectionMatrix, projMatrix);
//    rc.cameraPosition = probe.position;
//    rc.aspect = aspect;
//    rc.fovY = DirectX::XMConvertToRadians(90.0f);
//
//    SceneDataUploadSystem uploadSystem;
//
//    D3D11_BOX srcBox;
//    srcBox.left = cropLeft; srcBox.right = cropLeft + cubeSize;
//    srcBox.top = 0;         srcBox.bottom = cubeSize;
//    srcBox.front = 0;       srcBox.back = 1;
//
//    for (int i = 0; i < 6; ++i)
//    {
//        DirectX::XMMATRIX viewMatrix = GetViewMatrixForFace(probe.position, i);
//        DirectX::XMStoreFloat4x4(&rc.viewMatrix, viewMatrix);
//        DirectX::XMStoreFloat4x4(&rc.viewProjectionUnjittered, viewMatrix * projMatrix);
//
//        uploadSystem.Upload(rc, GlobalRootSignature::Instance());
//        GlobalRootSignature::Instance().BindAll(dc, rc.renderState, rc.shadowMap);
//
//        gbufferPass.Execute(queue, rc);
//        lightingPass.Execute(queue, rc);
//        skyboxPass.Execute(queue, rc);
//
//        dc->CopySubresourceRegion(cubemapRes.Get(), i, 0, 0, 0, srcRes.Get(), 0, &srcBox);
//    }
//
//    // =========================================================
//    // =========================================================
//    rc.viewMatrix = originalView;
//    rc.projectionMatrix = originalProj;
//    rc.viewProjectionUnjittered = originalUnjittered;
//    rc.cameraPosition = originalPos;
//    rc.aspect = originalAspect;
//    rc.fovY = originalFovY;
//
//    probe.needsBake = false;
//}
//
//void ReflectionProbeBaker::BakeAllDirtyProbes(Registry& registry, const RenderQueue& queue, RenderContext& rc)
//{
//  
//    Query<ReflectionProbeComponent> query(registry);
//    query.ForEach([this, &queue, &rc](ReflectionProbeComponent& probe) {
//        if (probe.needsBake) {
//            this->Bake(probe, queue, rc);
//        }
//        });
//}
#include "ReflectionProbeBaker.h"
#include "Graphics.h"
#include "Render/GlobalRootSignature.h"
#include "Scene/SceneDataUploadSystem.h"
#include "RHI/ICommandList.h"
#include "RHI/DX11/DX11Texture.h"
#include <System\Query.h>
#include <RenderPass\GBufferPass.h>
#include <RenderPass\DeferredLightingPass.h>
#include <RenderPass\SkyboxPass.h>
#include "RenderGraph/FrameGraph.h"
#include "RenderGraph/FrameGraphResources.h"

ReflectionProbeBaker::ReflectionProbeBaker(ID3D11Device* device) : m_device(device) {}

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
    Graphics& g = Graphics::Instance();
    auto dc = rc.commandList->GetNativeContext();
    FrameBuffer* sceneBuffer = g.GetFrameBuffer(FrameBufferId::Scene);

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
        m_device->CreateTexture2D(&texDesc, nullptr, cubemapTex.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        m_device->CreateShaderResourceView(cubemapTex.Get(), &srvDesc, probe.cubemapSRV.ReleaseAndGetAddressOf());
    }

    Microsoft::WRL::ComPtr<ID3D11Resource> cubemapRes;
    probe.cubemapSRV->GetResource(cubemapRes.GetAddressOf());

    GBufferPass gbufferPass;
    DeferredLightingPass lightingPass(Graphics::Instance().GetResourceFactory());
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

void ReflectionProbeBaker::BakeAllDirtyProbes(Registry& registry, const RenderQueue& queue, RenderContext& rc)
{
    Query<ReflectionProbeComponent> query(registry);
    query.ForEach([this, &queue, &rc](ReflectionProbeComponent& probe) {
        if (probe.needsBake) {
            this->Bake(probe, queue, rc);
        }
        });
}
