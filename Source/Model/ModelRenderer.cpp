#include "ModelRenderer.h"
#include <algorithm>
#include "System/Misc.h"
#include "GpuResourceUtils.h"
#include "ShaderClass/PhongShader.h"
#include "ShaderClass/PBRShader.h"
#include "ShaderClass/GBufferPBRShader.h"
#include "System/ResourceManager.h"
#include "ShadowMap.h"
#include "Graphics.h"

// RHI �C���N���[�h
#include "RHI/ICommandList.h"
#include "RHI/IBuffer.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"

ModelRenderer::~ModelRenderer() = default;

ModelRenderer::ModelRenderer(IResourceFactory* factory)
{
    // �X�P���g���p�萔�o�b�t�@�̐��� (RHI)
    skeletonConstantBuffer = factory->CreateBuffer(sizeof(CbSkeleton), BufferType::Constant);

    shaders[static_cast<int>(ShaderId::Phong)] = std::make_unique<PhongShader>(factory);
    shaders[static_cast<int>(ShaderId::PBR)] = std::make_unique<PBRShader>(factory);
    shaders[static_cast<int>(ShaderId::GBufferPBR)] = std::make_unique<GBufferPBRShader>(factory);
}

void ModelRenderer::Draw(ShaderId shaderId, std::shared_ptr<Model> model,
    const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& prevWorldMatrix,
    const DirectX::XMFLOAT4& baseColor, float metallic, float roughness, float emissive,
    BlendState blend, DepthState depth, RasterizerState raster)
{
    DrawInfo& drawInfo = drawInfos.emplace_back();
    drawInfo.shaderId = shaderId;
    drawInfo.model = model;
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
    // �� �C���Fdc ��r��
    rc.commandList->VSSetConstantBuffer(6, skeletonConstantBuffer.get());

    DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&rc.cameraPosition);
    DirectX::XMVECTOR CameraFront = DirectX::XMLoadFloat3(&rc.cameraDirection);

    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);

    for (DrawInfo& drawInfo : drawInfos)
    {
        Shader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
        shader->Begin(rc);

        for (const Model::Mesh& mesh : drawInfo.model->GetMeshes())
        {
            // �����x�ɂ�锼�������X�g�ւ̐U�蕪��
            if (mesh.material->alphaMode == Model::AlphaMode::Blend || (mesh.material->color.w > 0.01f && mesh.material->color.w < 0.99f))
            {
                TransparencyDrawInfo& tInfo = transparencyDrawInfos.emplace_back();
                tInfo.mesh = &mesh;
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

                DirectX::XMMATRIX W_Actor = DirectX::XMLoadFloat4x4(&drawInfo.worldMatrix);
                DirectX::XMVECTOR MeshPosModelSpace = DirectX::XMVectorSet(mesh.node->worldTransform._41, mesh.node->worldTransform._42, mesh.node->worldTransform._43, 1.0f);
                DirectX::XMVECTOR MeshPosWorld = DirectX::XMVector3Transform(MeshPosModelSpace, W_Actor);
                DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(MeshPosWorld, CameraPosition);
                tInfo.distance = DirectX::XMVectorGetX(DirectX::XMVector3Dot(CameraFront, Vec));
                continue;
            }

            // �X�e�[�g�ݒ� (RHI)
            rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(drawInfo.depthState), 0);
            rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(drawInfo.rasterizerState));
            rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

            // �� �C���F���b�V���̃o�b�t�@�o�C���h�� RHI �� (IBuffer���g�p)
            rc.commandList->SetVertexBuffer(0, mesh.vertexBuffer.get(), sizeof(Model::Vertex), 0);
            rc.commandList->SetIndexBuffer(mesh.indexBuffer.get(), IndexFormat::Uint32, 0);

            DirectX::XMMATRIX W_Actor = DirectX::XMLoadFloat4x4(&drawInfo.worldMatrix);
            DirectX::XMMATRIX W_ActorPrev = DirectX::XMLoadFloat4x4(&drawInfo.prevWorldMatrix);

            // �X�P���g���萔�o�b�t�@�v�Z
            CbSkeleton cbSkeleton{};
            if (mesh.bones.size() > 0) {
                for (size_t i = 0; i < mesh.bones.size(); ++i) {
                    DirectX::XMMATRIX ModelSpaceTransform = DirectX::XMLoadFloat4x4(&mesh.bones[i].node->worldTransform);
                    DirectX::XMMATRIX OffsetTransform = DirectX::XMLoadFloat4x4(&mesh.bones[i].offsetTransform);
                    DirectX::XMMATRIX BoneTransform = OffsetTransform * ModelSpaceTransform * W_Actor;
                    DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], BoneTransform);

                    DirectX::XMMATRIX PrevBoneTransform = OffsetTransform * ModelSpaceTransform * W_ActorPrev;
                    DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[i], PrevBoneTransform);
                }
            }
            else {
                DirectX::XMMATRIX ModelSpaceTransform = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
                DirectX::XMMATRIX WorldTransform = ModelSpaceTransform * W_Actor;
                DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], WorldTransform);
                DirectX::XMMATRIX PrevWorldTransform = ModelSpaceTransform * W_ActorPrev;
                DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[0], PrevWorldTransform);
            }

            // �o�b�t�@�X�V (RHI)
            rc.commandList->UpdateBuffer(skeletonConstantBuffer.get(), &cbSkeleton, sizeof(cbSkeleton));

            if (drawInfo.shaderId == ShaderId::PBR || drawInfo.shaderId == ShaderId::GBufferPBR) {
                auto pbrShader = static_cast<PBRShader*>(shader);
                pbrShader->SetMaterialProperties(drawInfo.baseColor, drawInfo.metallic, drawInfo.roughness, drawInfo.emissive);
            }

            shader->Update(rc, mesh);

            // �� �C���F�`����s (RHI)
            rc.commandList->DrawIndexed(static_cast<uint32_t>(mesh.indices.size()), 0, 0);
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

    // �����Ń\�[�g
    std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
        [](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs) { return lhs.distance > rhs.distance; });

    for (const TransparencyDrawInfo& info : transparencyDrawInfos)
    {
        Shader* shader = shaders[static_cast<int>(info.shaderId)].get();
        shader->Begin(rc);

        rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(info.depthState), 0);
        rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(info.rasterizerState));
        rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

        // �� �C���F�o�b�t�@�o�C���h�� RHI ��
        rc.commandList->SetVertexBuffer(0, info.mesh->vertexBuffer.get(), sizeof(Model::Vertex), 0);
        rc.commandList->SetIndexBuffer(info.mesh->indexBuffer.get(), IndexFormat::Uint32, 0);

        DirectX::XMMATRIX W_Actor = DirectX::XMLoadFloat4x4(&info.worldMatrix);
        DirectX::XMMATRIX W_ActorPrev = DirectX::XMLoadFloat4x4(&info.prevWorldMatrix);

        CbSkeleton cbSkeleton{};
        if (info.mesh->bones.size() > 0) {
            for (size_t i = 0; i < info.mesh->bones.size(); ++i) {
                DirectX::XMMATRIX ModelSpaceTransform = DirectX::XMLoadFloat4x4(&info.mesh->bones[i].node->worldTransform);
                DirectX::XMMATRIX OffsetTransform = DirectX::XMLoadFloat4x4(&info.mesh->bones[i].offsetTransform);
                DirectX::XMMATRIX BoneTransform = OffsetTransform * ModelSpaceTransform * W_Actor;
                DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], BoneTransform);
                DirectX::XMMATRIX PrevBoneTransform = OffsetTransform * ModelSpaceTransform * W_ActorPrev;
                DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[i], PrevBoneTransform);
            }
        }
        else {
            DirectX::XMMATRIX ModelSpaceTransform = DirectX::XMLoadFloat4x4(&info.mesh->node->worldTransform);
            DirectX::XMMATRIX WorldTransform = ModelSpaceTransform * W_Actor;
            DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], WorldTransform);
            DirectX::XMMATRIX PrevWorldTransform = ModelSpaceTransform * W_ActorPrev;
            DirectX::XMStoreFloat4x4(&cbSkeleton.prevBoneTransforms[0], PrevWorldTransform);
        }

        rc.commandList->UpdateBuffer(skeletonConstantBuffer.get(), &cbSkeleton, sizeof(cbSkeleton));

        if (info.shaderId == ShaderId::PBR) {
            auto pbrShader = static_cast<PBRShader*>(shader);
            pbrShader->SetMaterialProperties(info.baseColor, info.metallic, info.roughness, info.emissive);
        }

        shader->Update(rc, *info.mesh);

        // �� �C���F�`����s (RHI)
        rc.commandList->DrawIndexed(static_cast<uint32_t>(info.mesh->indices.size()), 0, 0);

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

        // �� �C���FPBRShader::SetIBLTextures �� ITexture* ���󂯎��悤�ɂȂ������߁A�L���X�g�s�v�I
        pbrShader->SetIBLTextures(currentDiffuseIBL.get(), currentSpecularIBL.get());
    }
}