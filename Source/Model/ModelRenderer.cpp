#include "ModelRenderer.h"
#include <algorithm>
#include "ModelResource.h"
#include "System/Misc.h"
#include "GpuResourceUtils.h"
#include "ShaderClass/PhongShader.h"
#include "ShaderClass/PBRShader.h"
#include "ShaderClass/GBufferPBRShader.h"
#include "System/ResourceManager.h"
#include "ShadowMap.h"
#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/IBuffer.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RenderContext/IndirectDrawCommon.h"

ModelRenderer::~ModelRenderer() = default;

ModelRenderer::ModelRenderer(IResourceFactory* factory)
{
    skeletonConstantBuffer = factory->CreateBuffer(sizeof(CbSkeleton), BufferType::Constant);
    shaders[static_cast<int>(ShaderId::Phong)] = std::make_unique<PhongShader>(factory);
    shaders[static_cast<int>(ShaderId::PBR)] = std::make_unique<PBRShader>(factory);
    shaders[static_cast<int>(ShaderId::GBufferPBR)] = std::make_unique<GBufferPBRShader>(factory);
}

void ModelRenderer::Draw(ShaderId shaderId, std::shared_ptr<ModelResource> modelResource,
    const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& prevWorldMatrix,
    const DirectX::XMFLOAT4& baseColor, float metallic, float roughness, float emissive,
    BlendState blend, DepthState depth, RasterizerState raster)
{
    DrawInfo& drawInfo = drawInfos.emplace_back();
    drawInfo.shaderId = shaderId;
    drawInfo.modelResource = std::move(modelResource);
    drawInfo.worldMatrix = worldMatrix;
    drawInfo.prevWorldMatrix = prevWorldMatrix;
    drawInfo.baseColor = baseColor;
    drawInfo.metallic = metallic;
    drawInfo.roughness = roughness;
    drawInfo.emissive = emissive;
    drawInfo.blendState = blend;
    drawInfo.depthState = depth;
    drawInfo.rasterizerState = raster;
}

void ModelRenderer::FillSkeletonConstantBuffer(const ModelResource::MeshResource& meshResource,
    const DirectX::XMFLOAT4X4& worldMatrix,
    const DirectX::XMFLOAT4X4& prevWorldMatrix,
    CbSkeleton& cbSkeleton) const
{
    using namespace DirectX;

    XMMATRIX actorWorld = XMLoadFloat4x4(&worldMatrix);
    XMMATRIX actorPrevWorld = XMLoadFloat4x4(&prevWorldMatrix);

    if (!meshResource.bones.empty()) {
        for (size_t i = 0; i < meshResource.bones.size(); ++i) {
            const auto& bone = meshResource.bones[i];
            XMMATRIX modelSpaceTransform = XMLoadFloat4x4(&bone.worldTransform);
            XMMATRIX offsetTransform = XMLoadFloat4x4(&bone.offsetTransform);
            XMMATRIX boneTransform = offsetTransform * modelSpaceTransform * actorWorld;
            XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], boneTransform);

            XMMATRIX prevBoneTransform = offsetTransform * modelSpaceTransform * actorPrevWorld;
            XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[i], prevBoneTransform);
        }
    } else {
        XMMATRIX modelSpaceTransform = XMLoadFloat4x4(&meshResource.nodeWorldTransform);
        XMMATRIX worldTransform = modelSpaceTransform * actorWorld;
        XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], worldTransform);
        XMMATRIX prevTransform = modelSpaceTransform * actorPrevWorld;
        XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[0], prevTransform);
    }
}

void ModelRenderer::ApplySkeletonConstantBuffer(const RenderContext& rc, const CbSkeleton& cbSkeleton) const
{
    const bool isDX12 = Graphics::Instance().GetAPI() == GraphicsAPI::DX12;
    if (isDX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        dx12Cmd->VSSetDynamicConstantBuffer(6, &cbSkeleton, sizeof(cbSkeleton));
    } else {
        rc.commandList->UpdateBuffer(skeletonConstantBuffer.get(), &cbSkeleton, sizeof(cbSkeleton));
    }
}

void ModelRenderer::ApplyMaterialOverrides(ShaderId shaderId, Shader* shader,
    const DirectX::XMFLOAT4& baseColor,
    float metallic,
    float roughness,
    float emissive) const
{
    if (shaderId == ShaderId::PBR || shaderId == ShaderId::GBufferPBR) {
        auto* pbrShader = static_cast<PBRShader*>(shader);
        pbrShader->SetMaterialProperties(baseColor, metallic, roughness, emissive);
    }
}

void ModelRenderer::Render(const RenderContext& rc, const RenderQueue& queue)
{
    RenderOpaque(rc);
    RenderTransparent(rc);
}

void ModelRenderer::RenderPreparedOpaque(const RenderContext& rc, bool forceShaderId, ShaderId forcedShaderId)
{
    if (!rc.HasPreparedOpaqueCommands()) {
        return;
    }

    const bool isDX12 = Graphics::Instance().GetAPI() == GraphicsAPI::DX12;
    if (!isDX12) {
        RenderOpaque(rc);
        return;
    }

    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);

    // Get active buffers
    IBuffer* instanceBuf = rc.activeInstanceBuffer;
    IBuffer* drawArgsBuf = rc.activeDrawArgsBuffer;
    const bool useIndirect = instanceBuf && drawArgsBuf;

    for (const auto& cmd : rc.activeDrawCommands) {
        auto modelResource = cmd.modelResource;
        if (!modelResource) continue;

        ShaderId shaderId = forceShaderId ? forcedShaderId : static_cast<ShaderId>(cmd.key.shaderId);
        Shader* shader = shaders[static_cast<int>(shaderId)].get();
        if (!shader) continue;

        const int meshIndex = static_cast<int>(cmd.meshIndex);
        const ModelResource::MeshResource* meshResource = modelResource->GetMeshResource(meshIndex);
        if (!meshResource) continue;

        const auto& material = meshResource->material;
        if (material.alphaMode == Model::AlphaMode::Blend || (material.color.w > 0.01f && material.color.w < 0.99f)) {
            continue;
        }

        rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(cmd.key.depthState), 0);
        rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(cmd.key.rasterizerState));
        rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

        if (!modelResource->BindMeshBuffers(rc.commandList, meshIndex)) continue;

        ApplyMaterialOverrides(shaderId, shader, cmd.key.baseColor, cmd.key.metallic, cmd.key.roughness, cmd.key.emissive);

        if (useIndirect && cmd.supportsInstancing &&
            cmd.instanceCount > 0 && shader->SupportsInstancing(*meshResource)) {
            // nodeWorldTransform をスケルトン定数バッファ経由で適用
            {
                CbSkeleton cbSkeleton{};
                DirectX::XMFLOAT4X4 identity;
                DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
                FillSkeletonConstantBuffer(*meshResource, identity, identity, cbSkeleton);
                ApplySkeletonConstantBuffer(rc, cbSkeleton);
            }
            shader->BeginInstanced(rc);
            rc.commandList->SetVertexBuffer(1, instanceBuf, rc.activeInstanceStride, 0);
            shader->Update(rc, *meshResource);
            uint32_t offsetBytes = cmd.drawArgsIndex * DRAW_ARGS_STRIDE;
            if (rc.useGpuCulling && rc.activeCountBuffer) {
                rc.commandList->ExecuteIndexedIndirectMulti(
                    drawArgsBuf, offsetBytes,
                    1, DRAW_ARGS_STRIDE,
                    rc.activeCountBuffer, rc.activeCountBufferOffset);
            } else {
                rc.commandList->ExecuteIndexedIndirect(drawArgsBuf, offsetBytes);
            }
            shader->End(rc);
        } else {
            // CPU fallback
            shader->Begin(rc);
            const uint32_t begin = cmd.firstInstance;
            const uint32_t end = cmd.firstInstance + cmd.instanceCount;
            for (uint32_t i = begin; i < end && i < rc.preparedInstanceData.size(); ++i) {
                const auto& instance = rc.preparedInstanceData[i];
                CbSkeleton cbSkeleton{};
                FillSkeletonConstantBuffer(*meshResource, instance.worldMatrix, instance.prevWorldMatrix, cbSkeleton);
                ApplySkeletonConstantBuffer(rc, cbSkeleton);
                shader->Update(rc, *meshResource);
                rc.commandList->DrawIndexed(modelResource->GetMeshIndexCount(meshIndex), 0, 0);
            }
            shader->End(rc);
        }
    }

    for (const auto& cmd : rc.activeSkinnedCommands) {
        auto modelResource = cmd.modelResource;
        if (!modelResource) continue;

        ShaderId shaderId = forceShaderId ? forcedShaderId : static_cast<ShaderId>(cmd.key.shaderId);
        Shader* shader = shaders[static_cast<int>(shaderId)].get();
        if (!shader) continue;

        const int meshIndex = static_cast<int>(cmd.meshIndex);
        const ModelResource::MeshResource* meshResource = modelResource->GetMeshResource(meshIndex);
        if (!meshResource) continue;

        rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(cmd.key.depthState), 0);
        rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(cmd.key.rasterizerState));
        rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

        if (!modelResource->BindMeshBuffers(rc.commandList, meshIndex)) continue;

        ApplyMaterialOverrides(shaderId, shader, cmd.key.baseColor, cmd.key.metallic, cmd.key.roughness, cmd.key.emissive);

        shader->Begin(rc);
        const uint32_t begin = cmd.firstInstance;
        const uint32_t end = cmd.firstInstance + cmd.instanceCount;
        for (uint32_t i = begin; i < end && i < rc.preparedInstanceData.size(); ++i) {
            const auto& instance = rc.preparedInstanceData[i];
            CbSkeleton cbSkeleton{};
            FillSkeletonConstantBuffer(*meshResource, instance.worldMatrix, instance.prevWorldMatrix, cbSkeleton);
            ApplySkeletonConstantBuffer(rc, cbSkeleton);
            shader->Update(rc, *meshResource);
            rc.commandList->DrawIndexed(modelResource->GetMeshIndexCount(meshIndex), 0, 0);
        }
        shader->End(rc);
    }
}

void ModelRenderer::RenderOpaque(const RenderContext& rc)
{
    const bool isDX12 = Graphics::Instance().GetAPI() == GraphicsAPI::DX12;
    if (!isDX12) {
        rc.commandList->VSSetConstantBuffer(6, skeletonConstantBuffer.get());
    }

    DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat3(&rc.cameraPosition);
    DirectX::XMVECTOR cameraFront = DirectX::XMLoadFloat3(&rc.cameraDirection);

    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);

    for (DrawInfo& drawInfo : drawInfos)
    {
        Shader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
        auto modelResource = drawInfo.modelResource;
        if (!shader || !modelResource) {
            continue;
        }

        shader->Begin(rc);
        for (int meshIndex = 0; meshIndex < modelResource->GetMeshCount(); ++meshIndex)
        {
            const ModelResource::MeshResource* meshResource = modelResource->GetMeshResource(meshIndex);
            if (!meshResource) continue;

            const Model::Material& material = meshResource->material;
            if (material.alphaMode == Model::AlphaMode::Blend || (material.color.w > 0.01f && material.color.w < 0.99f))
            {
                TransparencyDrawInfo& tInfo = transparencyDrawInfos.emplace_back();
                tInfo.modelResource = modelResource;
                tInfo.meshIndex = meshIndex;
                tInfo.shaderId = (drawInfo.shaderId == ShaderId::GBufferPBR) ? ShaderId::PBR : drawInfo.shaderId;
                tInfo.worldMatrix = drawInfo.worldMatrix;
                tInfo.prevWorldMatrix = drawInfo.prevWorldMatrix;
                tInfo.baseColor = drawInfo.baseColor;
                tInfo.metallic = drawInfo.metallic;
                tInfo.roughness = drawInfo.roughness;
                tInfo.emissive = drawInfo.emissive;
                tInfo.blendState = drawInfo.blendState;
                tInfo.depthState = drawInfo.depthState;
                tInfo.rasterizerState = drawInfo.rasterizerState;

                DirectX::XMMATRIX actorWorld = DirectX::XMLoadFloat4x4(&drawInfo.worldMatrix);
                DirectX::XMVECTOR meshPosModelSpace = DirectX::XMVectorSet(
                    meshResource->nodeWorldTransform._41,
                    meshResource->nodeWorldTransform._42,
                    meshResource->nodeWorldTransform._43,
                    1.0f);
                DirectX::XMVECTOR meshPosWorld = DirectX::XMVector3Transform(meshPosModelSpace, actorWorld);
                DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(meshPosWorld, cameraPosition);
                tInfo.distance = DirectX::XMVectorGetX(DirectX::XMVector3Dot(cameraFront, vec));
                continue;
            }

            rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(drawInfo.depthState), 0);
            rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(drawInfo.rasterizerState));
            rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

            if (!modelResource->BindMeshBuffers(rc.commandList, meshIndex)) {
                continue;
            }

            CbSkeleton cbSkeleton{};
            FillSkeletonConstantBuffer(*meshResource, drawInfo.worldMatrix, drawInfo.prevWorldMatrix, cbSkeleton);
            ApplySkeletonConstantBuffer(rc, cbSkeleton);
            ApplyMaterialOverrides(drawInfo.shaderId, shader, drawInfo.baseColor, drawInfo.metallic, drawInfo.roughness, drawInfo.emissive);
            shader->Update(rc, *meshResource);
            rc.commandList->DrawIndexed(modelResource->GetMeshIndexCount(meshIndex), 0, 0);
        }
        shader->End(rc);
    }
    drawInfos.clear();
    if (!isDX12) {
        rc.commandList->VSSetConstantBuffer(6, nullptr);
    }
}

void ModelRenderer::RenderTransparent(const RenderContext& rc)
{
    const bool isDX12 = Graphics::Instance().GetAPI() == GraphicsAPI::DX12;
    if (!isDX12) {
        rc.commandList->VSSetConstantBuffer(6, skeletonConstantBuffer.get());
    }

    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), blendFactor, 0xFFFFFFFF);

    std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
        [](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs) { return lhs.distance > rhs.distance; });

    for (const TransparencyDrawInfo& info : transparencyDrawInfos)
    {
        auto modelResource = info.modelResource;
        if (!modelResource) continue;
        const ModelResource::MeshResource* meshResource = modelResource->GetMeshResource(info.meshIndex);
        if (!meshResource) continue;

        Shader* shader = shaders[static_cast<int>(info.shaderId)].get();
        if (!shader) continue;
        shader->Begin(rc);

        rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(info.depthState), 0);
        rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(info.rasterizerState));
        rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

        if (!modelResource->BindMeshBuffers(rc.commandList, info.meshIndex)) {
            shader->End(rc);
            continue;
        }

        CbSkeleton cbSkeleton{};
        FillSkeletonConstantBuffer(*meshResource, info.worldMatrix, info.prevWorldMatrix, cbSkeleton);
        ApplySkeletonConstantBuffer(rc, cbSkeleton);
        ApplyMaterialOverrides(info.shaderId, shader, info.baseColor, info.metallic, info.roughness, info.emissive);
        shader->Update(rc, *meshResource);
        rc.commandList->DrawIndexed(modelResource->GetMeshIndexCount(info.meshIndex), 0, 0);
        shader->End(rc);
    }
    transparencyDrawInfos.clear();
    if (!isDX12) {
        rc.commandList->VSSetConstantBuffer(6, nullptr);
    }
}

void ModelRenderer::SetIBL(const std::string& diffusePath, const std::string& specularPath)
{
    if (!diffusePath.empty()) {
        currentDiffuseIBL = ResourceManager::Instance().GetTexture(diffusePath);
    }
    if (!specularPath.empty()) {
        currentSpecularIBL = ResourceManager::Instance().GetTexture(specularPath);
    }

    if (shaders[static_cast<int>(ShaderId::PBR)]) {
        auto* pbrShader = static_cast<PBRShader*>(shaders[static_cast<int>(ShaderId::PBR)].get());
        pbrShader->SetIBLTextures(currentDiffuseIBL.get(), currentSpecularIBL.get());
    }
}
