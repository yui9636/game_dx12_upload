#include "EffectVariantShader.h"
#include "Graphics.h"
#include "RenderContext/RenderContext.h"
#include "Model/ModelResource.h"
#include "GpuResourceUtils.h"
#include "RHI/ICommandList.h"
#include "RHI/DX11/DX11Buffer.h"

using namespace Microsoft::WRL;
using namespace DirectX;

EffectVariantShader::EffectVariantShader(ID3D11Device* device)
{
    // ---------------------------------------------------------
    // ---------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // ---------------------------------------------------------
    // ---------------------------------------------------------
    HRESULT hr = GpuResourceUtils::LoadVertexShader(
        device,
        "Data/Shader/EffectVS.cso",
        inputElementDesc,
        _countof(inputElementDesc),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // ---------------------------------------------------------
    // ---------------------------------------------------------
    GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbScene), sceneConstantBuffer.GetAddressOf());
    GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbSkeleton), skeletonConstantBuffer.GetAddressOf());

}

void EffectVariantShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    // dc->PSSetShader(nullptr, nullptr, 0); 

    ID3D11Buffer* constantBuffers[] =
    {
        sceneConstantBuffer.Get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        skeletonConstantBuffer.Get(),
    };
    rc.commandList->GetNativeContext()->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
    rc.commandList->GetNativeContext()->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);




    //ID3D11SamplerState* sampleStates[] = {
    //    rc.renderState->GetSamplerState(SamplerState::LinearWrap),
    //    rc.renderState->GetSamplerState(SamplerState::LinearClamp)
    //};
    //dc->PSSetSamplers(0, _countof(sampleStates), sampleStates);

    //const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    //dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Additive), blendFactor, 0xFFFFFFFF);
    //dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
    //dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

    CbScene cbScene{};
    DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
    DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
    DirectX::XMStoreFloat4x4(&cbScene.viewProjection, V * P);
    const DirectionalLight& directionalLight = rc.directionalLight;
    cbScene.lightDirection.x = directionalLight.direction.x;
    cbScene.lightDirection.y = directionalLight.direction.y;
    cbScene.lightDirection.z = directionalLight.direction.z;
    cbScene.lightColor.x = directionalLight.color.x;
    cbScene.lightColor.y = directionalLight.color.y;
    cbScene.lightColor.z = directionalLight.color.z;
    cbScene.cameraPosition = { rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z, 1.0f };
    cbScene.lightViewProjection = rc.shadowMap->GetLightViewProjection(0);


    dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);
}

void EffectVariantShader::Draw(const RenderContext& rc, const ModelResource* modelResource)
{
    if (!modelResource) return;
    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

    for (const ModelResource::MeshResource& mesh : modelResource->GetMeshResources())
    {
        auto* vertexBuffer = dynamic_cast<DX11Buffer*>(mesh.vertexBuffer.get());
        auto* indexBuffer = dynamic_cast<DX11Buffer*>(mesh.indexBuffer.get());
        if (!vertexBuffer || !indexBuffer || mesh.indexCount == 0)
        {
            continue;
        }

        UINT stride = mesh.vertexStride;
        UINT offset = 0;
        ID3D11Buffer* vb = vertexBuffer->GetNative();
        ID3D11Buffer* ib = indexBuffer->GetNative();
        dc->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        dc->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CbSkeleton cbSkeleton{};
        if (!mesh.bones.empty())
        {
            for (size_t i = 0; i < mesh.bones.size() && i < _countof(cbSkeleton.boneTransforms); ++i)
            {
                const ModelResource::BoneResource& bone = mesh.bones[i];
                XMMATRIX worldTransform = DirectX::XMLoadFloat4x4(&bone.worldTransform);
                XMMATRIX offsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
                XMMATRIX boneTransform = offsetTransform * worldTransform;
                XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], boneTransform);
            }
        }
        else
        {
            cbSkeleton.boneTransforms[0] = mesh.nodeWorldTransform;
        }

        dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);
        dc->VSSetConstantBuffers(6, 1, skeletonConstantBuffer.GetAddressOf());
        dc->DrawIndexed(mesh.indexCount, 0, 0);
    }
}

void EffectVariantShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);
}
