#pragma once


struct PostEffectComponent {
    // 1. 輝度抽出 & ブルーム
    float luminanceLowerEdge = 0.0f;
    float luminanceHigherEdge = 0.0f;
    float bloomIntensity = 0.0f;
    float gaussianSigma = 0.0f;

    // 2. カラーフィルター & レンズ効果
    float exposure = -0.25f;
    float monoBlend = 0.0f;
    float hueShift = 0.0f;
    float flashAmount = 0.0f;
    float vignetteAmount = 0.0f;

    // 3. 被写界深度 (DoF)
    bool  enableDoF = false;
    float focusDistance = 0.0f;
    float focusRange = 0.0f;
    float bokehRadius = 0.0f;

    //4. オブジェクト・モーションブラー
    float motionBlurIntensity = 1.0f; // ブラーの強さ（0.0で無効）
    int motionBlurSamples = 8;        // サンプル数（多いほど滑らかだが重い）
};