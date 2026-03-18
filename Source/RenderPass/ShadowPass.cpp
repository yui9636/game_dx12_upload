#include "ShadowPass.h"
#include "Graphics.h"
#include "ShadowMap.h"
#include "Render/GlobalRootSignature.h"
#include "Render/ShaderCommon.h"#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"

void ShadowPass::Setup(FrameGraphBuilder& builder)
{
    // 1. ・ｽy・ｽ・ｽ・ｽ・ｽ・ｽz・ｽ・ｽ・ｽ・ｽ1・ｽﾂゑｿｽ GetHandle ・ｽ・ｽ・ｽg・ｽp
    // ・ｽ|・ｽX・ｽg・ｽv・ｽ・ｽ・ｽZ・ｽX・ｽ・ｽ・ｽﾆ難ｿｽ・ｽl・ｽﾉ、・ｽ・ｽ・ｽ・ｽ・ｽﾌ・ｿｽ・ｽ\・ｽ[・ｽX・ｽ`・ｽP・ｽb・ｽg・ｽ・ｽ・ｽ謫ｾ・ｽ・ｽ・ｽﾜゑｿｽ
    m_hShadowMap = builder.GetHandle("ShadowMap");

    // 2. ・ｽO・ｽ・ｽ・ｽt・ｽﾉ「・ｽ・ｽ・ｽﾌパ・ｽX・ｽﾍ影・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾞ」・ｽﾆ宣言
    if (m_hShadowMap.IsValid()) {
        m_hShadowMap = builder.Write(m_hShadowMap);
    }
}

void ShadowPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    auto shadowMap = const_cast<ShadowMap*>(rc.shadowMap);
    if (!shadowMap) return;

    // ・ｽ・ｽ ・ｽs・ｽ・ｽﾆカ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾌバ・ｽb・ｽN・ｽA・ｽb・ｽv (RenderContext ・ｽﾌ抵ｿｽ`・ｽﾉ搾ｿｽ・ｽ墲ｹ・ｽﾜゑｿｽ・ｽ・ｽ) ・ｽ・ｽ
    auto mainView = rc.viewMatrix;
    auto mainProj = rc.projectionMatrix;
    auto mainCamPos = rc.cameraPosition;
    auto mainCamDir = rc.cameraDirection;

    // 1. ・ｽJ・ｽX・ｽP・ｽ[・ｽh・ｽs・ｽ・ｽﾌ更・ｽV
    shadowMap->UpdateCascades(rc);

    {
        CbShadowMap shadow = {};
        for (int i = 0; i < ShadowMap::CASCADE_COUNT; ++i) {
            shadow.lightViewProjections[i] = shadowMap->GetLightViewProjection(i);
        }
        shadow.cascadeSplits = {
            shadowMap->GetCascadeEnd(0),
            shadowMap->GetCascadeEnd(1),
            shadowMap->GetCascadeEnd(2),
            0.0f
        };
        shadow.shadowColor = { rc.shadowColor.x, rc.shadowColor.y, rc.shadowColor.z, 1.0f };
        shadow.shadowBias = { 0.005f, 0.0f, 0.0f, 0.0f };
        rc.commandList->UpdateBuffer(GlobalRootSignature::Instance().GetShadowBuffer(), &shadow, sizeof(shadow));
    }

    // 2. ・ｽe・ｽJ・ｽX・ｽP・ｽ[・ｽh・ｽi・ｽﾟ景・ｽE・ｽ・ｽ・ｽi・ｽE・ｽ・ｽ・ｽi・ｽj・ｽﾖの描・ｽ・ｽ・ｽ・ｽs
    for (int i = 0; i < ShadowMap::CASCADE_COUNT; ++i) {
        shadowMap->BeginCascade(rc, i); // ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ rc ・ｽ・ｽ・ｽﾌ行・ｽｪ一時・ｽI・ｽﾉ・ｿｽ・ｽC・ｽg・ｽ・ｽ・ｽ_・ｽﾉなゑｿｽ

        for (const auto& packet : queue.opaquePackets) {
            if (packet.model && packet.castShadow) {
                shadowMap->Draw(rc, packet.model, packet.worldMatrix);
            }
        }
    }
    shadowMap->End(rc);
    if (shadowMap->GetTexture()) {
        rc.commandList->TransitionBarrier(shadowMap->GetTexture(), ResourceState::ShaderResource);
    }

    // ・ｽ・ｽ ・ｽd・ｽv・ｽF・ｽ・ｽ・ｽC・ｽ・ｽ・ｽJ・ｽ・ｽ・ｽ・ｽ・ｽﾌ擾ｿｽﾔゑｿｽ・ｽ・ｽ・ｽS・ｽﾉ包ｿｽ・ｽ・ｽ ・ｽ・ｽ
    // ・ｽ・ｽ・ｽ・ｽﾅ後続・ｽ・ｽ GBufferPass ・ｽ・ｽ LightingPass ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ_・ｽﾅ描・ｽ謔ｳ・ｽ・ｽﾜゑｿｽ
    rc.viewMatrix = mainView;
    rc.projectionMatrix = mainProj;
    rc.cameraPosition = mainCamPos;
    rc.cameraDirection = mainCamDir;
}
