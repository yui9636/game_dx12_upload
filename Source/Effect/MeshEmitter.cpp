#include "MeshEmitter.h"
#include "Graphics.h"
#include "EffectManager.h"
#include "RenderContext/RenderContext.h"
#include "ShaderClass/ShaderCompiler.h"
#include <iostream> // デバッグ出力用
#include "RHI/ICommandList.h"

MeshEmitter::MeshEmitter() : EffectNode()
{
    // マテリアル確保
    material = std::make_shared<EffectMaterial>();

    pixelShaderVariant = EffectManager::Get().GetPixelShaderVariant(0);
}

MeshEmitter::~MeshEmitter() {}

void MeshEmitter::LoadModel(const std::string& path)
{
 /*   this->modelPath = path;
    auto device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, path.c_str(), 1.0f);*/

    this->modelPath = path;
    this->model = EffectManager::Get().GetModel(path);

}

void MeshEmitter::Update(float deltaTime)
{
    EffectNode::Update(deltaTime);
}

void MeshEmitter::UpdateWithAge(float age, float lifeTime)
{
    // 1. 親クラス（EffectNode）のトランスフォーム更新などを先に実行
    EffectNode::UpdateWithAge(age, lifeTime);

    // マテリアルが無いなら何もしない
    if (!material) return;

    m_currentAge = age;

    // 2. 正規化時間 (0.0 ~ 1.0) の算出
    // エミッターの開始時間と持続時間から、現在の進行度(t)を計算します
    float nodeLocalTime = age - startTime;
    float t = 0.0f;

    if (duration > 0.001f) {
        t = std::clamp(nodeLocalTime / duration, 0.0f, 1.0f);
    }
    ApplyCurves(t);
}

void MeshEmitter::ApplyCurves(float t)
{
    if (!material) return;
    auto& c = material->GetConstants();

    // 各種カーブの適用
    if (visibilityCurve.IsValid())         c.visibility = visibilityCurve.Evaluate(t);
    if (dissolveCurve.IsValid())           c.dissolveThreshold = dissolveCurve.Evaluate(t);
    if (emissiveCurve.IsValid())           c.emissiveIntensity = emissiveCurve.Evaluate(t);

    if (colorCurves[0].IsValid()) c.baseColor.x = colorCurves[0].Evaluate(t);
    if (colorCurves[1].IsValid()) c.baseColor.y = colorCurves[1].Evaluate(t);
    if (colorCurves[2].IsValid()) c.baseColor.z = colorCurves[2].Evaluate(t);

    if (distortionStrengthCurve.IsValid()) c.distortionStrength = distortionStrengthCurve.Evaluate(t);
    if (wpoStrengthCurve.IsValid())        c.wpoStrength = wpoStrengthCurve.Evaluate(t);
    if (flowStrengthCurve.IsValid())       c.flowStrength = flowStrengthCurve.Evaluate(t);
    if (flowSpeedCurve.IsValid())          c.flowSpeed = flowSpeedCurve.Evaluate(t);
    if (fresnelPowerCurve.IsValid())       c.fresnelPower = fresnelPowerCurve.Evaluate(t);

    if (mainUvScrollCurves[0].IsValid())   c.mainUvScrollSpeed.x = mainUvScrollCurves[0].Evaluate(t);
    if (mainUvScrollCurves[1].IsValid())   c.mainUvScrollSpeed.y = mainUvScrollCurves[1].Evaluate(t);

    if (distUvScrollCurves[0].IsValid())   c.distortionUvScrollSpeed.x = distUvScrollCurves[0].Evaluate(t);
    if (distUvScrollCurves[1].IsValid())   c.distortionUvScrollSpeed.y = distUvScrollCurves[1].Evaluate(t);

    if (clipStartCurve.IsValid())          c.clipStart = clipStartCurve.Evaluate(t);
    if (clipEndCurve.IsValid())            c.clipEnd = clipEndCurve.Evaluate(t);

    if (clipSoftnessCurve.IsValid())       c.clipSoftness = clipSoftnessCurve.Evaluate(t);
}


//void MeshEmitter::Render(const RenderContext& rc)
//{
//    if (!model) return;
//
//    model->UpdateTransform(worldMatrix);
//
//    auto shaderToUse = EffectManager::Get().GetStandardShader();
//    if (shaderToUse)
//    {
//        shaderToUse->Begin(rc);
//
//        // ★追加: 自分が持っているバリアントPSをセットする
//        if (pixelShaderVariant)
//        {
//            rc.commandList->GetNativeContext()->PSSetShader(pixelShaderVariant.Get(), nullptr, 0);
//        }
//
//        if (material) {
//            material->Apply(rc);
//        }
//
//        shaderToUse->Draw(rc, model->GetModelResource().get());
//
//        shaderToUse->End(rc);
//    }
//}

void MeshEmitter::Render(const RenderContext& rc)
{
    // モデルが無ければ描画できないので終了
    if (!model) return;

    // 現在使用するシェーダーを取得
    auto shaderToUse = EffectManager::Get().GetStandardShader();
    if (!shaderToUse) return;

    shaderToUse->Begin(rc);

    // バリアントPSの適用
    if (pixelShaderVariant)
    {
        rc.commandList->GetNativeContext()->PSSetShader(pixelShaderVariant.Get(), nullptr, 0);
    }

    // マテリアル定数の参照を取得
    auto& c = material->GetConstants();

    // バックアップ
    float originalTime = c.currentTime;
    float originalVisibility = c.visibility;
    DirectX::XMFLOAT4X4 originalWorld = worldMatrix;

    // ループ回数を決定
    int maxLoop = ghostEnabled ? ghostCount : 0;

    for (int i = maxLoop; i >= 0; --i)
    {
        // -------------------------------------------------------------
        // ★ 1. ゴーストの「年齢」を計算
        // -------------------------------------------------------------
        // m_currentAge (現在の経過時間) から、遅延分を引いたものが「残像の年齢」
        float ghostAge = m_currentAge - (i * ghostTimeDelay);

        // -------------------------------------------------------------
        // ★ 2. 生存期間チェック (ここが最重要！)
        // -------------------------------------------------------------
        // A. まだ生まれていない (Age < 0) -> 描画しない
        if (ghostAge < 0.0f) continue;

        // B. 寿命(Duration)を過ぎている -> 描画しない
        // これにより、本体が消えた後、残像も遅れて順次消滅します
        if (duration > 0.001f && ghostAge > duration) continue;

        // -------------------------------------------------------------
        // ★ 3. カーブの再評価 (ApplyCurves)
        // -------------------------------------------------------------
        // 残像の時点での「進行度 t」を計算し、色やClip値をその時点のものに戻す
        float ghostLocalTime = ghostAge - startTime; // ループ等の場合はここでfmod等の計算が必要
        float ghostT = 0.0f;

        if (duration > 0.001f) {
            ghostT = std::clamp(ghostLocalTime / duration, 0.0f, 1.0f);
        }

        // これを呼ばないと、形だけ本体と同じ(完成形)になってしまう
        ApplyCurves(ghostT);


        // -------------------------------------------------------------
        // 4. 定数のセットアップ
        // -------------------------------------------------------------

        // 時間のオフセット (UVスクロール等)
        c.currentTime = originalTime - (i * ghostTimeDelay);

        // 透明度の減衰 (計算されたカーブのAlphaに対して、さらに減衰をかける)
        float alphaRatio = std::pow(ghostAlphaDecay, (float)i);
        c.visibility *= alphaRatio; // ApplyCurvesでセットされたvisibilityに掛ける

        if (c.visibility < 0.01f) continue;

        // 5. 座標のオフセット
        if (i > 0)
        {
            DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&originalWorld);
            m.r[3] = DirectX::XMVectorAdd(m.r[3],
                DirectX::XMVectorSet(
                    -ghostPosOffset.x * i,
                    -ghostPosOffset.y * i,
                    -ghostPosOffset.z * i,
                    0.0f
                )
            );
            DirectX::XMStoreFloat4x4(&worldMatrix, m);
        }
        else
        {
            worldMatrix = originalWorld;
        }

        // 6. 描画実行
        if (material) {
            material->Apply(rc);
        }
        model->UpdateTransform(worldMatrix);
        shaderToUse->Draw(rc, model->GetModelResource().get());
    }

    shaderToUse->End(rc);

    // -------------------------------------------------------------
    // 後始末 (Restore State)
    // -------------------------------------------------------------
    worldMatrix = originalWorld;
    c.currentTime = originalTime;
    c.visibility = originalVisibility;

    // ★重要: カーブの状態を「現在の時間」に戻しておく
    // これを忘れると、次のフレームの計算開始時に変な値が残る可能性がある
    float currentLocalTime = m_currentAge - startTime;
    float currentT = (duration > 0.001f) ? std::clamp(currentLocalTime / duration, 0.0f, 1.0f) : 0.0f;
    ApplyCurves(currentT);
}




void MeshEmitter::RefreshPixelShader()
{
    if (!material) return;

    auto& c = material->GetConstants();
    int flags = 0; // ShaderFlag_None

    // メインテクスチャを使う設定ならフラグON
    if (c.mainTexIndex >= 0) {
        flags |= ShaderFlag_Texture;
    }

    // 歪み機能が有効(インデックス0以上)ならフラグON
    if (c.distortionTexIndex >= 0) {
        flags |= ShaderFlag_Distort;
    }

    // 溶解機能が有効(インデックス0以上)ならフラグON
    if (c.dissolveTexIndex >= 0) {
        flags |= ShaderFlag_Dissolve;

       // 発光効果も有効ならフラグON
        if (c.dissolveGlowIntensity > 0.01f)
        {
            flags |= ShaderFlag_DissolveGlow;
        }
    }

    // マスク機能が有効(インデックス0以上)ならフラグON
    if (c.maskTexIndex >= 0) {
        flags |= ShaderFlag_Mask;
    }

    // フレネル効果が有効(閾値以上)ならフラグON
    if (c.fresnelPower > 0.01f) {
        flags |= ShaderFlag_Fresnel;
    }

    if (c.flipbookWidth > 1.0f || c.flipbookHeight > 1.0f) {
        flags |= ShaderFlag_Flipbook;
    }

    if(c.gradientTexIndex >= 0) {
		flags |= ShaderFlag_GradientMap;
	}

    if (c.chromaticAberrationStrength > 0.001f) {
		flags |= ShaderFlag_ChromaticAberration;
	}

    if(c.matCapTexIndex >= 0) {
		flags |= ShaderFlag_MatCap;
	}

    if (c.normalTexIndex >= 0)
    {
        flags |= ShaderFlag_NormalMap;
    }

    if (c.flowTexIndex >= 0) 
    {
        flags |= ShaderFlag_FlowMap;
    }

    if (c.sideFadeWidth > 0.001f)
    {
        flags |= ShaderFlag_SideFade;
    }

    //if (c.visibility < 0.999f) // 1.0より小さければフェード計算を有効にする
    //{
    //    flags |= ShaderFlag_AlphaFade;
    //}

    flags |= ShaderFlag_AlphaFade;


    if (c.subTexIndex >= 0)
    {
        flags |= ShaderFlag_SubTexture;
    }

    if (c.toonRampTexIndex >=0)
    {
        flags |= ShaderFlag_Toon;
    }


    // シェーダー取得
    this->pixelShaderVariant = EffectManager::Get().GetPixelShaderVariant(flags);
}