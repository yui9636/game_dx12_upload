cbuffer CbPostEffect : register(b0)
{
    float luminanceExtractionLowerEdge;
    float luminanceExtractionHigherEdge;
    float gaussianSigma;
    float bloomIntensity;
    
    // -------------------------
    float exposure; // ★追加: ACES トーンマッピング用の露出値
    float monoBlend;
    float hueShift;
    float flashAmount;
    
    // -------------------------
    float vignetteAmount;
    float time;
    float focusDistance;
    float focusRange;
    
    // -------------------------
    float bokehRadius;

    float motionBlurIntensity;
    float motionBlurSamples;
    float _padding;
};