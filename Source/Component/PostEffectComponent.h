#pragma once


struct PostEffectComponent {
    float luminanceLowerEdge = 0.0f;
    float luminanceHigherEdge = 0.0f;
    float bloomIntensity = 0.0f;
    float gaussianSigma = 0.0f;

    float exposure = -0.25f;
    float monoBlend = 0.0f;
    float hueShift = 0.0f;
    float flashAmount = 0.0f;
    float vignetteAmount = 0.0f;

    bool  enableDoF = false;
    float focusDistance = 0.0f;
    float focusRange = 0.0f;
    float bokehRadius = 0.0f;

    float motionBlurIntensity = 1.0f;
    int motionBlurSamples = 8;
};
