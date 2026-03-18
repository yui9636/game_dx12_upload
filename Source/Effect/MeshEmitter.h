#pragma once
#include "EffectNode.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include "Model/Model.h"
#include "ShaderClass/EffectVariantShader.h"
#include "Effect/EffectMaterial.h"
#include "EffectCurve.h"

class MeshEmitter : public EffectNode
{
public:
    MeshEmitter();
    ~MeshEmitter() override;

    // データ
    std::shared_ptr<::Model> model;
    std::shared_ptr<EffectMaterial> material;
    std::shared_ptr<EffectVariantShader> baseShader;

 
    std::string modelPath;
    std::string GetModelPath() const { return modelPath; }

    // ★追加: このエミッター専用の「バリアント」ピクセルシェーダー
    // (これがセットされていれば、baseShaderのPSの代わりにこれを使う)
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShaderVariant;

    // 種類を返す
    EffectNodeType GetType() const override { return EffectNodeType::MeshEmitter; }

    // 更新処理
    void Update(float deltaTime) override;

    // 描画処理
    void Render(const struct RenderContext& rc);

    // 便利機能
    void LoadModel(const std::string& path);

    void RefreshPixelShader();

    // ★追加: バリアントをセットする関数
    void SetPixelShader(Microsoft::WRL::ComPtr<ID3D11PixelShader> ps) { pixelShaderVariant = ps; }

public:
 
    EffectCurve visibilityCurve;      // 透明度 (0.0 ~ 1.0)
    EffectCurve dissolveCurve;        // ディゾルブしきい値 (0.0 ~ 1.0)
    EffectCurve emissiveCurve;        // 発光強度 (0.0 ~ 100.0以上も可)
    EffectCurve colorCurves[3];       // RGB 各成分 (0.0 ~ 1.0)
    EffectCurve distortionStrengthCurve; // 歪み強度 (0.0 ~ 2.0程度)
    EffectCurve wpoStrengthCurve;        // WPO/頂点揺れ強度 (0.0 ~ 5.0程度)
    EffectCurve flowStrengthCurve;      // フロー強度
    EffectCurve flowSpeedCurve;         // フロー速度
    EffectCurve fresnelPowerCurve;      // フレネル強度
    EffectCurve mainUvScrollCurves[2];  // メインUVスクロール速度 (X, Y)
    EffectCurve distUvScrollCurves[2];  // 歪みUVスクロール速度 (X, Y)
    EffectCurve clipStartCurve;         
    EffectCurve clipEndCurve;		 // UVクリップ開始・終了位置 (0.0 ~ 1.0)
    EffectCurve clipSoftnessCurve;

    std::shared_ptr<::Model> GetModel() const { return model; }

    // ★追加: 時間経過による更新関数のオーバーライド
    void UpdateWithAge(float age, float lifeTime) override;

    // ★追加: ゴーストトレイル設定
    bool  ghostEnabled = false;      // 残像有効化
    int   ghostCount = 3;            // 残像の数
    float ghostTimeDelay = 0.05f;    // 時間の遅れ幅
    float ghostAlphaDecay = 0.5f;    // 透明度の減衰率
    DirectX::XMFLOAT3 ghostPosOffset = { 0.0f, 0.0f, 0.0f }; // 座標ずらし幅

    float m_currentAge = 0.0f;

private:
  
    void ApplyCurves(float t);


};