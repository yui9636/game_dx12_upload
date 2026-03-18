#include "ModelRenderer.h"
#include "ModelResource.h"
#include <algorithm>
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

void ModelRenderer::Render(const RenderContext& rc, const RenderQueue& queue)
{
    RenderOpaque(rc);
    RenderTransparent(rc);
}

void ModelRenderer::RenderOpaque(const RenderContext& rc)
{
    rc.commandList->VSSetConstantBuffer(6, skeletonConstantBuffer.get());

    DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat3(&rc.cameraPosition);
    DirectX::XMVECTOR cameraFront = DirectX::XMLoadFloat3(&rc.cameraDirection);

    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);

    for (DrawInfo& drawInfo : drawInfos)
    {
        Shader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
        shader->Begin(rc);

        auto modelResource = drawInfo.modelResource;
        if (!modelResource) {
            shader->End(rc);
            continue;
        }

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
                DirectX::XMVECTOR meshPosModelSpace = DirectX::XMVectorSet(meshResource->nodeWorldTransform._41, meshResource->nodeWorldTransform._42, meshResource->nodeWorldTransform._43, 1.0f);
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

            DirectX::XMMATRIX actorWorld = DirectX::XMLoadFloat4x4(&drawInfo.worldMatrix);
            DirectX::XMMATRIX actorPrevWorld = DirectX::XMLoadFloat4x4(&drawInfo.prevWorldMatrix);

            CbSkeleton cbSkeleton{};
            if (!meshResource->bones.empty()) {
                for (size_t i = 0; i < meshResource->bones.size(); ++i) {
                    const auto& bone = meshResource->bones[i];
                    DirectX::XMMATRIX modelSpaceTransform = DirectX::XMLoadFloat4x4(&bone.worldTransform);
                    DirectX::XMMATRIX offsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
                    DirectX::XMMATRIX boneTransform = offsetTransform * modelSpaceTransform * actorWorld;
                    DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], boneTransform);

                    DirectX::XMMATRIX prevBoneTransform = offsetTransform * modelSpaceTransform * actorPrevWorld;
                    DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[i], prevBoneTransform);
                }
            }
            else {
                DirectX::XMMATRIX modelSpaceTransform = DirectX::XMLoadFloat4x4(&meshResource->nodeWorldTransform);
                DirectX::XMMATRIX worldTransform = modelSpaceTransform * actorWorld;
                DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], worldTransform);
                DirectX::XMMATRIX prevWorldTransform = modelSpaceTransform * actorPrevWorld;
                DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[0], prevWorldTransform);
            }

            rc.commandList->UpdateBuffer(skeletonConstantBuffer.get(), &cbSkeleton, sizeof(cbSkeleton));

            if (drawInfo.shaderId == ShaderId::PBR || drawInfo.shaderId == ShaderId::GBufferPBR) {
                auto pbrShader = static_cast<PBRShader*>(shader);
                pbrShader->SetMaterialProperties(drawInfo.baseColor, drawInfo.metallic, drawInfo.roughness, drawInfo.emissive);
            }

            shader->Update(rc, *meshResource);
            rc.commandList->DrawIndexed(modelResource->GetMeshIndexCount(meshIndex), 0, 0);
        }
        shader->End(rc);
    }
    drawInfos.clear();
    rc.commandList->VSSetConstantBuffer(6, nullptr);
}

void ModelRenderer::RenderTransparent(const RenderContext& rc)
{
    rc.commandList->VSSetConstantBuffer(6, skeletonConstantBuffer.get());

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
        shader->Begin(rc);

        rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(info.depthState), 0);
        rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(info.rasterizerState));
        rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

        if (!modelResource->BindMeshBuffers(rc.commandList, info.meshIndex)) {
            shader->End(rc);
            continue;
        }

        DirectX::XMMATRIX actorWorld = DirectX::XMLoadFloat4x4(&info.worldMatrix);
        DirectX::XMMATRIX actorPrevWorld = DirectX::XMLoadFloat4x4(&info.prevWorldMatrix);

        CbSkeleton cbSkeleton{};
        if (!meshResource->bones.empty()) {
            for (size_t i = 0; i < meshResource->bones.size(); ++i) {
                const auto& bone = meshResource->bones[i];
                DirectX::XMMATRIX modelSpaceTransform = DirectX::XMLoadFloat4x4(&bone.worldTransform);
                DirectX::XMMATRIX offsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
                DirectX::XMMATRIX boneTransform = offsetTransform * modelSpaceTransform * actorWorld;
                DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], boneTransform);
                DirectX::XMMATRIX prevBoneTransform = offsetTransform * modelSpaceTransform * actorPrevWorld;
                DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[i], prevBoneTransform);
            }
        }
        else {
            DirectX::XMMATRIX modelSpaceTransform = DirectX::XMLoadFloat4x4(&meshResource->nodeWorldTransform);
            DirectX::XMMATRIX worldTransform = modelSpaceTransform * actorWorld;
            DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], worldTransform);
            DirectX::XMMATRIX prevWorldTransform = modelSpaceTransform * actorPrevWorld;
            DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[0], prevWorldTransform);
        }

        rc.commandList->UpdateBuffer(skeletonConstantBuffer.get(), &cbSkeleton, sizeof(cbSkeleton));

        if (info.shaderId == ShaderId::PBR) {
            auto pbrShader = static_cast<PBRShader*>(shader);
            pbrShader->SetMaterialProperties(info.baseColor, info.metallic, info.roughness, info.emissive);
        }

        shader->Update(rc, *meshResource);
        rc.commandList->DrawIndexed(modelResource->GetMeshIndexCount(info.meshIndex), 0, 0);
        shader->End(rc);
    }
    transparencyDrawInfos.clear();
    rc.commandList->VSSetConstantBuffer(6, nullptr);
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
