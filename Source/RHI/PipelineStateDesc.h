#pragma once
#include <cstdint>
#include "RHI/IShader.h"
#include "RHI/IState.h"
#include "RHI/ITexture.h"
#include "RHI/ICommandList.h" // PrimitiveTopology のため

// ==========================================
// PSO生成用の記述子（DX12のD3D12_GRAPHICS_PIPELINE_STATE_DESCに相当）
// ==========================================
struct PipelineStateDesc {
    // 1. シェーダー
    IShader* vertexShader = nullptr;
    IShader* pixelShader = nullptr;
    IShader* geometryShader = nullptr;
    IShader* computeShader = nullptr;

    // 2. 固定機能ステート
    IInputLayout* inputLayout = nullptr;
    IBlendState* blendState = nullptr;
    IRasterizerState* rasterizerState = nullptr;
    IDepthStencilState* depthStencilState = nullptr;

    // 3. トポロジー
    PrimitiveTopology primitiveTopology = PrimitiveTopology::TriangleList;

    // 4. レンダーターゲット構成
    uint32_t      numRenderTargets = 1;
    TextureFormat rtvFormats[8] = { TextureFormat::Unknown };
    TextureFormat dsvFormat = TextureFormat::Unknown;

    // 5. サンプリング設定（MSAA用）
    uint32_t sampleCount = 1;
    uint32_t sampleMask = 0xFFFFFFFF;
};