cbuffer CbPostEffect : register(b0)
{
    float luminanceExtractionLowerEdge;
    float luminanceExtractionHigherEdge;
    float gaussianSigma;
    float bloomIntensity;
    
    // -------------------------
    float exposure; // ���ǉ�: ACES �g�[���}�b�s���O�p�̘I�o�l
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