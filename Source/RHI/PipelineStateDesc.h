#pragma once
#include <cstdint>
#include "RHI/IShader.h"
#include "RHI/IState.h"
#include "RHI/ITexture.h"
#include "RHI/ICommandList.h"

// ==========================================
// ==========================================
struct PipelineStateDesc {
    IShader* vertexShader = nullptr;
    IShader* pixelShader = nullptr;
    IShader* geometryShader = nullptr;
    IShader* computeShader = nullptr;

    IInputLayout* inputLayout = nullptr;
    IBlendState* blendState = nullptr;
    IRasterizerState* rasterizerState = nullptr;
    IDepthStencilState* depthStencilState = nullptr;

    PrimitiveTopology primitiveTopology = PrimitiveTopology::TriangleList;

    uint32_t      numRenderTargets = 1;
    TextureFormat rtvFormats[8] = { TextureFormat::Unknown };
    TextureFormat dsvFormat = TextureFormat::Unknown;

    uint32_t sampleCount = 1;
    uint32_t sampleMask = 0xFFFFFFFF;
};
