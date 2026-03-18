#pragma once

//#include "Camera/Camera.h"
#include "RenderState.h"
#include "Light/Light.h"
#include <string>
#include <functional>
#include <DirectXMath.h>
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"

class ShadowMap;
class Skybox;


// ブルーム設定
struct BloomData {
    float luminanceLowerEdge = 0.6f;
    float luminanceHigherEdge = 0.8f;
    float bloomIntensity = 1.0f;
    float gaussianSigma = 1.0f;
};

// カラーフィルター設定
struct ColorFilterData {
    float exposure = 1.2f;
    float monoBlend = 0.0f;
    float hueShift = 0.0f;
    float flashAmount = 0.0f;
    float vignetteAmount = 0.0f;
};


// DoF（被写界深度）設定
struct DepthOfFieldData
{
    bool  enable = false;         // 有効/無効
    float focusDistance = 10.0f;  // ピントが合う距離 (m)
    float focusRange = 5.0f;      // ピントが合う範囲 (m)
    float bokehRadius = 4.0f;     // ボケの強さ
};

struct MotionBlurData
{
    float intensity = 0.0f;
    float samples = 8.0f;
};

struct RenderEnvironment
{
    std::string skyboxPath = "";
    std::string diffuseIBLPath = "";
    std::string specularIBLPath = "";
};

// UV スクロール情報
struct UVScrollData
{
    DirectX::XMFLOAT2 uvScrollValue;
};

// マスクデータ
struct MaskData
{
    ID3D11ShaderResourceView* maskTexture;
    float dissolveThreshold;
    float edgeThreshold;
    DirectX::XMFLOAT4 edgeColor;
};

// ラジアルブラー情報
struct RadialBlurData
{
    float radius = 10.0f;
    int samplingCount = 10;
    DirectX::XMFLOAT2 center = { 0.5f, 0.5f };
    float mask_radius = 0;
};

// ガウスフィルター計算情報
struct GaussianFilterData {
    int kernelSize = 8;
    float deviation = 10.0f;
    DirectX::XMFLOAT2 textureSize;
};

static const int MaxKernelSize = 16;

struct RenderPipelineSettings
{
    bool enableShadow = true;
    bool enableSkybox = true;
    bool enablePostProcess = true;
    bool enableGrid = true;
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};


struct RenderContext
{
    //ID3D11DeviceContext* deviceContext;
    ICommandList* commandList;
    const RenderState* renderState;

    RhiViewport mainViewport;
    ITexture* mainRenderTarget = nullptr;
    ITexture* mainDepthStencil = nullptr;

    DirectionalLight        directionalLight;
    std::vector<PointLight> pointLights;

    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 viewProjectionUnjittered;
    DirectX::XMFLOAT4X4 prevViewProjectionMatrix;
    DirectX::XMFLOAT3   cameraPosition;
    DirectX::XMFLOAT3   cameraDirection;

    float fovY;
    float aspect;
    float nearZ;
    float farZ;
    DirectX::XMFLOAT2 jitterOffset = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 prevJitterOffset = { 0.0f, 0.0f };

    // 影生成クラスへのアクセス（影描画パスで使用）
    const ShadowMap* shadowMap = nullptr;

    // 影の色（環境設定から来る）
    DirectX::XMFLOAT3 shadowColor = { 0.1f, 0.1f, 0.1f };

    ITexture* sceneColorTexture = nullptr;
    ITexture* sceneDepthTexture = nullptr;

    ITexture* reflectionProbeTexture = nullptr;

    ITexture* debugGBuffer0 = nullptr;
    ITexture* debugGBuffer1 = nullptr;
    ITexture* debugGBuffer2 = nullptr;

    RenderEnvironment environment;

    // ポストプロセス用データ
    BloomData       bloomData;      // ★追加
    ColorFilterData colorFilterData; // ★追加
    DepthOfFieldData dofData;       //
    MotionBlurData  motionBlurData;

    // ----------------------------------------------------

    // 既存メンバ
    float time = 0.0f; // アニメーション用タイマーなど

    // 以下、必要な設定データを保持
    UVScrollData uvScrollData;
    MaskData maskData;
    RadialBlurData radialBlurData;
    GaussianFilterData gaussianFilterData;

   
};
