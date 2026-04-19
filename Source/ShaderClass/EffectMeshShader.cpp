#include "EffectMeshShader.h"

#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/IShader.h"
#include "RHI/IPipelineState.h"
#include "RHI/ITexture.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/DX12/DX12CommandList.h"

EffectMeshShader::~EffectMeshShader() = default;

EffectMeshShader::EffectMeshShader(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/EffectMeshUberVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/EffectMeshUberPS.cso");

    // Match PBRShader input layout so existing mesh VBs flow through unchanged.
    InputLayoutElement elems[] = {
        {"POSITION",     0, TextureFormat::R32G32B32_FLOAT,     0, kAppendAlignedElement},
        {"BONE_WEIGHTS", 0, TextureFormat::R32G32B32A32_FLOAT,  0, kAppendAlignedElement},
        {"BONE_INDICES", 0, TextureFormat::R32G32B32A32_UINT,   0, kAppendAlignedElement},
        {"TEXCOORD",     0, TextureFormat::R32G32_FLOAT,        0, kAppendAlignedElement},
        {"NORMAL",       0, TextureFormat::R32G32B32_FLOAT,     0, kAppendAlignedElement},
        {"TANGENT",      0, TextureFormat::R32G32B32_FLOAT,     0, kAppendAlignedElement},
    };
    InputLayoutDesc ilDesc = { elems, _countof(elems) };
    m_inputLayout = factory->CreateInputLayout(ilDesc, m_vs.get());

    PipelineStateDesc desc{};
    desc.vertexShader      = m_vs.get();
    desc.pixelShader       = m_ps.get();
    desc.inputLayout       = m_inputLayout.get();
    desc.primitiveTopology = PrimitiveTopology::TriangleList;
    desc.numRenderTargets  = 1;
    desc.rtvFormats[0]     = TextureFormat::R16G16B16A16_FLOAT;
    desc.dsvFormat         = TextureFormat::D32_FLOAT;
    m_pso = factory->CreatePipelineState(desc);
}

void EffectMeshShader::UploadConstants(ICommandList* cmd, const CbMeshEffect& cb) const
{
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        static_cast<DX12CommandList*>(cmd)->PSSetDynamicConstantBuffer(3, &cb, sizeof(cb));
    }
    // DX11 path is not needed — EffectMesh is DX12-only in Phase B.
}

void EffectMeshShader::BindTextures(ICommandList* cmd,
    ITexture* base, ITexture* mask, ITexture* normal,
    ITexture* flow, ITexture* sub,  ITexture* emission) const
{
    ITexture* slots[6] = { base, mask, normal, flow, sub, emission };
    cmd->PSSetTextures(0, 6, slots);
}

void EffectMeshShader::UnbindTextures(ICommandList* cmd) const
{
    ITexture* nulls[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    cmd->PSSetTextures(0, 6, nulls);
}
