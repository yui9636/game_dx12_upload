#include "EffectMeshPass.h"

#include "Console/Logger.h"
#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "RenderContext/RenderState.h"
#include "Model/ModelResource.h"
#include "ShaderClass/EffectMeshShader.h"

namespace {
    constexpr int kMaxBones = 256;

    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4 boneTransforms[kMaxBones];
        DirectX::XMFLOAT4X4 prevBoneTransforms[kMaxBones];
    };

    void FillSkeletonCb(const ModelResource::MeshResource& mesh,
        const DirectX::XMFLOAT4X4& world,
        const DirectX::XMFLOAT4X4& prevWorld,
        CbSkeleton& cb)
    {
        using namespace DirectX;
        XMMATRIX W     = XMLoadFloat4x4(&world);
        XMMATRIX prevW = XMLoadFloat4x4(&prevWorld);

        if (!mesh.bones.empty()) {
            for (size_t i = 0; i < mesh.bones.size() && i < kMaxBones; ++i) {
                const auto& bone = mesh.bones[i];
                XMMATRIX modelS = XMLoadFloat4x4(&bone.worldTransform);
                XMMATRIX offset = XMLoadFloat4x4(&bone.offsetTransform);
                XMStoreFloat4x4(&cb.boneTransforms[i],     offset * modelS * W);
                XMStoreFloat4x4(&cb.prevBoneTransforms[i], offset * modelS * prevW);
            }
        } else {
            // Effect entities have no MeshComponent, so ModelUpdateSystem never ran and
            // mesh.nodeWorldTransform stayed zeroed. Fall back to identity * W — effect
            // meshes render at the actor's world transform directly.
            XMStoreFloat4x4(&cb.boneTransforms[0],     W);
            XMStoreFloat4x4(&cb.prevBoneTransforms[0], prevW);
        }
    }
}

EffectMeshPass::EffectMeshPass() = default;
EffectMeshPass::~EffectMeshPass() = default;

void EffectMeshPass::Setup(FrameGraphBuilder& builder, const RenderContext&)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth      = builder.GetHandle("GBufferDepth");

    if (m_hSceneColor.IsValid()) {
        m_hSceneColor = builder.Write(m_hSceneColor);
        builder.RegisterHandle("SceneColor", m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
}

void EffectMeshPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    if (queue.effectMeshPackets.empty()) {
        return;
    }

    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        return;  // Phase B is DX12-only
    }

    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsReal  = resources.GetTexture(m_hDepth);
    if (!rtScene || !dsReal) {
        LOG_ERROR("[EffectMeshPass] rtScene=%p dsReal=%p — aborting", (void*)rtScene, (void*)dsReal);
        return;
    }

    if (!m_shader) {
        m_shader = std::make_unique<EffectMeshShader>(Graphics::Instance().GetResourceFactory());
        LOG_INFO("[EffectMeshPass] shader created (PSO=%p)", (void*)m_shader->GetPipelineState());
    }

    static int s_frameCount = 0;
    const bool verbose = (s_frameCount++ < 5);
    if (verbose) {
        LOG_INFO("[EffectMeshPass] Execute: %zu packets rtScene=%dx%d",
            queue.effectMeshPackets.size(), rtScene->GetWidth(), rtScene->GetHeight());
    }

    auto* cmd = rc.commandList;
    auto* dx12 = static_cast<DX12CommandList*>(cmd);

    cmd->TransitionBarrier(rtScene, ResourceState::RenderTarget);
    cmd->TransitionBarrier(dsReal,  ResourceState::DepthWrite);
    cmd->SetRenderTarget(rtScene, dsReal);

    rc.mainRenderTarget = rtScene;
    rc.mainDepthStencil = dsReal;
    rc.mainViewport = RhiViewport(0.0f, 0.0f,
        static_cast<float>(rtScene->GetWidth()),
        static_cast<float>(rtScene->GetHeight()));
    cmd->SetViewport(rc.mainViewport);

    cmd->SetPipelineState(m_shader->GetPipelineState());
    cmd->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    BlendState      lastBlend = BlendState::EnumCount;
    DepthState      lastDepth = DepthState::EnumCount;
    RasterizerState lastRaster = RasterizerState::EnumCount;

    const DirectX::XMFLOAT4 lightDir4 = {
        rc.directionalLight.direction.x,
        rc.directionalLight.direction.y,
        rc.directionalLight.direction.z,
        0.0f
    };
    const DirectX::XMFLOAT4 camPos4 = {
        rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z, 1.0f
    };

    for (const auto& packet : queue.effectMeshPackets) {
        auto modelResource = packet.modelResource;
        if (!modelResource) continue;

        if (lastBlend != packet.blendState) {
            cmd->SetBlendState(rc.renderState->GetBlendState(packet.blendState), blendFactor, 0xFFFFFFFF);
            lastBlend = packet.blendState;
        }
        if (lastDepth != packet.depthState) {
            cmd->SetDepthStencilState(rc.renderState->GetDepthStencilState(packet.depthState), 0);
            lastDepth = packet.depthState;
        }
        if (lastRaster != packet.rasterizerState) {
            cmd->SetRasterizerState(rc.renderState->GetRasterizerState(packet.rasterizerState));
            lastRaster = packet.rasterizerState;
        }

        // Build and upload per-packet CbMeshEffect (b3)
        const auto& params = packet.meshVariantParams;
        const auto& K = params.constants;
        EffectMeshShader::CbMeshEffect cb{};
        cb.dissolveAmount    = K.dissolveAmount;
        cb.dissolveEdge      = K.dissolveEdge;
        cb.flowSpeed         = K.flowSpeed;
        cb.dissolveGlowColor = K.dissolveGlowColor;
        cb.fresnelPower      = K.fresnelPower;
        cb.fresnelColor      = K.fresnelColor;
        cb.flowStrength      = K.flowStrength;
        cb.alphaFade         = K.alphaFade * packet.lifetimeFade;
        cb.scrollSpeed       = K.scrollSpeed;
        cb.distortStrength   = K.distortStrength;
        cb.rimColor          = K.rimColor;
        cb.rimPower          = K.rimPower;
        cb.emissionColor     = K.emissionColor;
        cb.emissionIntensity = K.emissionIntensity;
        cb.effectTime        = K.effectTime;
        cb.baseColor         = packet.baseColor;
        cb.variantFlags      = params.shaderFlags;
        cb.lightDirection    = lightDir4;
        cb.cameraPosition    = camPos4;
        m_shader->UploadConstants(cmd, cb);

        // Slot 0 (base albedo): prefer the template-authored baseTexture on the
        // packet (set from MeshRenderer.stringValue / variantParams.baseTexturePath).
        // Fall back to the FBX material's own albedoMap only when the template
        // hasn't supplied one — this is what lets e.g. the Sword Slash Glow
        // template overlay Aura01_T.png onto a model whose source material has
        // no albedo texture (otherwise the shader saw only the tint color).
        ITexture* baseTex = packet.baseTexture.get();
        if (!baseTex && packet.modelResource->GetMeshCount() > 0) {
            baseTex = packet.modelResource->GetMeshResource(0)->material.albedoMap.get();
        }
        m_shader->BindTextures(cmd,
            baseTex,
            packet.maskTexture.get(),
            packet.normalMapTexture.get(),
            packet.flowMapTexture.get(),
            packet.subTexture.get(),
            packet.emissionTexture.get());

        const int meshCount = modelResource->GetMeshCount();
        if (verbose) {
            LOG_INFO("[EffectMeshPass] packet flags=0x%08x baseColor=(%.2f,%.2f,%.2f,%.2f) meshCount=%d worldT=(%.2f,%.2f,%.2f)",
                cb.variantFlags, cb.baseColor.x, cb.baseColor.y, cb.baseColor.z, cb.baseColor.w,
                meshCount,
                packet.worldMatrix._41, packet.worldMatrix._42, packet.worldMatrix._43);
        }
        for (int meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
            const auto* meshRes = modelResource->GetMeshResource(meshIndex);
            if (!meshRes) continue;

            CbSkeleton skel{};
            FillSkeletonCb(*meshRes, packet.worldMatrix, packet.prevWorldMatrix, skel);
            dx12->VSSetDynamicConstantBuffer(6, &skel, sizeof(skel));

            if (!modelResource->BindMeshBuffers(cmd, meshIndex)) {
                if (verbose) LOG_WARN("[EffectMeshPass] BindMeshBuffers failed mesh=%d", meshIndex);
                continue;
            }
            const uint32_t idxCount = modelResource->GetMeshIndexCount(meshIndex);
            if (verbose) LOG_INFO("[EffectMeshPass] DrawIndexed mesh=%d idx=%u bones=%zu", meshIndex, idxCount, meshRes->bones.size());
            cmd->DrawIndexed(idxCount, 0, 0);
        }
    }

    m_shader->UnbindTextures(cmd);
    cmd->SetRenderTarget(nullptr, nullptr);
}
