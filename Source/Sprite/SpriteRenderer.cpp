#include "SpriteRenderer.h"

#include "Sprite.h"
#include "Graphics.h"
#include "RHI/IBuffer.h"
#include "RHI/ICommandList.h"
#include "RHI/IPipelineState.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IShader.h"
#include "RHI/ITexture.h"
#include "RHI/PipelineStateDesc.h"
#include "RenderContext/RenderState.h"

#include <cmath>
#include <cstring>

namespace
{
    // 頂点レイアウトは Shader/SpriteVS.hlsl と一致させる。
    //   POSITION は float3、COLOR は float4、TEXCOORD は float2。
    struct SpriteVertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    // Shader/Sprite.hlsli の register(b0) と同じメモリ配置にする。
    struct UIConstants
    {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 glowColor;
        float glowIntensity;
        float padding[3];
    };

    constexpr uint32_t kVertexCountPerSprite = 4;
}

SpriteRenderer& SpriteRenderer::Instance()
{
    static SpriteRenderer instance;
    return instance;
}

void SpriteRenderer::Initialize(IResourceFactory* factory)
{
    if (m_initialized || !factory) {
        return;
    }

    // 仕様 3.5 の Step (a): 既存の事前コンパイル済み .cso を読み込む。
    // これらは Game.vcxproj の FxCompile 設定で SM5.0 として生成済み。
    auto vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/SpriteVS.cso");
    auto ps = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/SpriteUI_PS.cso");
    if (!vs || !ps) {
        // シェーダーファイルが見つからない場合は未初期化のままにし、
        // HUDPass / UIElement が描画をスキップしてクラッシュを避けられるようにする。
        return;
    }
    m_vs.reset(vs.release());
    m_ps.reset(ps.release());

    // SpriteVertex 用の入力レイアウトは POSITION / COLOR / TEXCOORD。
    InputLayoutElement elements[3];
    elements[0] = { "POSITION", 0, TextureFormat::R32G32B32_FLOAT,    0, 0,                                false, 0 };
    elements[1] = { "COLOR",    0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement,            false, 0 };
    elements[2] = { "TEXCOORD", 0, TextureFormat::R32G32_FLOAT,       0, kAppendAlignedElement,            false, 0 };
    InputLayoutDesc ilDesc{ elements, 3 };
    auto il = factory->CreateInputLayout(ilDesc, m_vs.get());
    if (il) {
        m_inputLayout.reset(il.release());
    }

    // PSO を作成する。アルファブレンド、有効な深度なし、triangle strip で
    // スプライト 1 枚を 4 頂点で描く。RTV 形式は Display framebuffer に合わせる。
    // HUDPass が DisplayColor へ書き込むため。
    auto* rs = Graphics::Instance().GetRenderState();
    PipelineStateDesc desc{};
    desc.vertexShader      = m_vs.get();
    desc.pixelShader       = m_ps.get();
    desc.inputLayout       = m_inputLayout.get();
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::NoTestNoWrite);
    desc.rasterizerState   = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    desc.blendState        = rs->GetBlendState(BlendState::Transparency);
    desc.primitiveTopology = PrimitiveTopology::TriangleStrip;
    desc.numRenderTargets  = 1;
    desc.rtvFormats[0]     = TextureFormat::RGBA8_UNORM;
    if (FrameBuffer* display = Graphics::Instance().GetFrameBuffer(FrameBufferId::Display)) {
        if (ITexture* displayColor = display->GetColorTexture(0)) {
            desc.rtvFormats[0] = displayColor->GetFormat();
        }
    }
    desc.dsvFormat = TextureFormat::Unknown;
    auto pso = factory->CreatePipelineState(desc);
    if (pso) {
        m_pso.reset(pso.release());
    }

    // 描画ごとに書き換えるリソース。頂点バッファは 4 頂点だけを保持し、
    // 1 枚の矩形を表す。Begin / End で 1 フレームを区切るため、
    // 各 Draw 呼び出しで同じバッファを再利用して書き換えられる。
    auto vb = factory->CreateBuffer(
        sizeof(SpriteVertex) * kVertexCountPerSprite,
        BufferType::Vertex,
        nullptr);
    if (vb) {
        m_vertexBuffer.reset(vb.release());
    }
    auto cb = factory->CreateBuffer(sizeof(UIConstants), BufferType::Constant, nullptr);
    if (cb) {
        m_constantBuffer.reset(cb.release());
    }

    m_initialized = m_pso != nullptr && m_vertexBuffer != nullptr && m_constantBuffer != nullptr;
}

void SpriteRenderer::Finalize()
{
    m_pso.reset();
    m_inputLayout.reset();
    m_vs.reset();
    m_ps.reset();
    m_vertexBuffer.reset();
    m_constantBuffer.reset();
    m_currentCommandList = nullptr;
    m_initialized = false;
}

void SpriteRenderer::Begin(ICommandList* commandList, const DirectX::XMFLOAT2& viewportPx)
{
    if (!m_initialized) return;
    m_currentCommandList = commandList;
    m_currentViewport = viewportPx;
    if (commandList) {
        commandList->SetPipelineState(m_pso.get());
        commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
        commandList->SetInputLayout(m_inputLayout.get());
    }
}

void SpriteRenderer::End()
{
    m_currentCommandList = nullptr;
}

void SpriteRenderer::Draw(const Sprite& sprite,
                          float dx, float dy, float dw, float dh,
                          float angleRad,
                          const DirectX::XMFLOAT4& tintColor)
{
    const float texW = static_cast<float>(sprite.GetTextureWidth());
    const float texH = static_cast<float>(sprite.GetTextureHeight());
    Draw(sprite, dx, dy, dw, dh, 0.0f, 0.0f, texW, texH, angleRad, tintColor);
}

void SpriteRenderer::Draw(const Sprite& sprite,
                          float dx, float dy,
                          float dw, float dh,
                          float sx, float sy, float sw, float sh,
                          float angleRad,
                          const DirectX::XMFLOAT4& tintColor)
{
    DrawInternal(sprite, dx, dy, dw, dh, sx, sy, sw, sh, angleRad, tintColor);
}

void SpriteRenderer::DrawQuad(const Sprite& sprite,
                              const DirectX::XMFLOAT2& p0,
                              const DirectX::XMFLOAT2& p1,
                              const DirectX::XMFLOAT2& p2,
                              const DirectX::XMFLOAT2& p3,
                              const DirectX::XMFLOAT4& tintColor)
{
    DrawQuadInternal(sprite, p0, p1, p2, p3,
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f },
        tintColor);
}

void SpriteRenderer::DrawQuadUV(const Sprite& sprite,
                                const DirectX::XMFLOAT2& p0,
                                const DirectX::XMFLOAT2& p1,
                                const DirectX::XMFLOAT2& p2,
                                const DirectX::XMFLOAT2& p3,
                                const DirectX::XMFLOAT2& uv0,
                                const DirectX::XMFLOAT2& uv1,
                                const DirectX::XMFLOAT2& uv2,
                                const DirectX::XMFLOAT2& uv3,
                                const DirectX::XMFLOAT4& tintColor)
{
    DrawQuadInternal(sprite, p0, p1, p2, p3, uv0, uv1, uv2, uv3, tintColor);
}

void SpriteRenderer::DrawInternal(const Sprite& sprite,
                                  float dx, float dy, float dw, float dh,
                                  float sx, float sy, float sw, float sh,
                                  float angleRad,
                                  const DirectX::XMFLOAT4& tintColor)
{
    if (!m_initialized || !m_currentCommandList) return;
    ITexture* texture = sprite.GetTexture();
    if (!texture) return;
    if (m_currentViewport.x <= 0.0f || m_currentViewport.y <= 0.0f) return;

    m_currentCommandList->SetPipelineState(m_pso.get());
    m_currentCommandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
    m_currentCommandList->SetInputLayout(m_inputLayout.get());

    // 4 隅を画面ピクセル座標で作り、その後 NDC へ変換する。
    // dx,dy は pivot 補正後の矩形左上。呼び出し側が pivot を処理し、
    // シェーダー側の計算を単純に保つ。
    const float cx = dx + dw * 0.5f;
    const float cy = dy + dh * 0.5f;
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);

    auto rotateAround = [&](float px, float py) -> DirectX::XMFLOAT2 {
        const float ox = px - cx;
        const float oy = py - cy;
        return { cx + ox * c - oy * s, cy + ox * s + oy * c };
    };

    const DirectX::XMFLOAT2 cornersPx[4] = {
        rotateAround(dx,      dy),       // TL
        rotateAround(dx + dw, dy),       // TR
        rotateAround(dx,      dy + dh),  // BL
        rotateAround(dx + dw, dy + dh),  // BR
    };

    auto pixelToNdc = [&](const DirectX::XMFLOAT2& p) -> DirectX::XMFLOAT3 {
        const float ndcX = (p.x / m_currentViewport.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (p.y / m_currentViewport.y) * 2.0f;
        return { ndcX, ndcY, 0.0f };
    };

    const float texW = (std::max)(1.0f, static_cast<float>(sprite.GetTextureWidth()));
    const float texH = (std::max)(1.0f, static_cast<float>(sprite.GetTextureHeight()));
    const float u0 = sx / texW;
    const float v0 = sy / texH;
    const float u1 = (sx + sw) / texW;
    const float v1 = (sy + sh) / texH;

    // TriangleStrip の頂点順は左上、右上、左下、右下。
    SpriteVertex vertices[kVertexCountPerSprite];
    vertices[0].position = pixelToNdc(cornersPx[0]);
    vertices[1].position = pixelToNdc(cornersPx[1]);
    vertices[2].position = pixelToNdc(cornersPx[2]);
    vertices[3].position = pixelToNdc(cornersPx[3]);
    vertices[0].texcoord = { u0, v0 };
    vertices[1].texcoord = { u1, v0 };
    vertices[2].texcoord = { u0, v1 };
    vertices[3].texcoord = { u1, v1 };
    for (auto& v : vertices) v.color = tintColor;

    m_currentCommandList->UpdateBuffer(m_vertexBuffer.get(), vertices, sizeof(vertices));

    UIConstants ui{};
    const auto& spriteColor = sprite.GetColor();
    ui.color = { spriteColor.x, spriteColor.y, spriteColor.z, spriteColor.w };
    const auto& glow = sprite.GetGlowColor();
    ui.glowColor = { glow.x, glow.y, glow.z, sprite.GetGlowIntensity() };
    ui.glowIntensity = sprite.GetGlowIntensity();
    m_currentCommandList->UpdateBuffer(m_constantBuffer.get(), &ui, sizeof(ui));

    m_currentCommandList->SetVertexBuffer(0, m_vertexBuffer.get(), sizeof(SpriteVertex), 0);
    m_currentCommandList->VSSetConstantBuffer(0, m_constantBuffer.get());
    m_currentCommandList->PSSetConstantBuffer(0, m_constantBuffer.get());
    m_currentCommandList->PSSetTexture(0, texture);
    m_currentCommandList->Draw(kVertexCountPerSprite, 0);
}

void SpriteRenderer::DrawQuadInternal(const Sprite& sprite,
                                      const DirectX::XMFLOAT2& p0,
                                      const DirectX::XMFLOAT2& p1,
                                      const DirectX::XMFLOAT2& p2,
                                      const DirectX::XMFLOAT2& p3,
                                      const DirectX::XMFLOAT2& uv0,
                                      const DirectX::XMFLOAT2& uv1,
                                      const DirectX::XMFLOAT2& uv2,
                                      const DirectX::XMFLOAT2& uv3,
                                      const DirectX::XMFLOAT4& tintColor)
{
    if (!m_initialized || !m_currentCommandList) return;
    ITexture* texture = sprite.GetTexture();
    if (!texture) return;
    if (m_currentViewport.x <= 0.0f || m_currentViewport.y <= 0.0f) return;

    m_currentCommandList->SetPipelineState(m_pso.get());
    m_currentCommandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
    m_currentCommandList->SetInputLayout(m_inputLayout.get());

    auto pixelToNdc = [&](const DirectX::XMFLOAT2& p) -> DirectX::XMFLOAT3 {
        const float ndcX = (p.x / m_currentViewport.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (p.y / m_currentViewport.y) * 2.0f;
        return { ndcX, ndcY, 0.0f };
    };

    SpriteVertex vertices[kVertexCountPerSprite];
    vertices[0].position = pixelToNdc(p0);
    vertices[1].position = pixelToNdc(p1);
    vertices[2].position = pixelToNdc(p3);
    vertices[3].position = pixelToNdc(p2);
    vertices[0].texcoord = uv0;
    vertices[1].texcoord = uv1;
    vertices[2].texcoord = uv3;
    vertices[3].texcoord = uv2;
    for (auto& v : vertices) v.color = tintColor;

    m_currentCommandList->UpdateBuffer(m_vertexBuffer.get(), vertices, sizeof(vertices));

    UIConstants ui{};
    const auto& spriteColor = sprite.GetColor();
    ui.color = { spriteColor.x, spriteColor.y, spriteColor.z, spriteColor.w };
    const auto& glow = sprite.GetGlowColor();
    ui.glowColor = { glow.x, glow.y, glow.z, sprite.GetGlowIntensity() };
    ui.glowIntensity = sprite.GetGlowIntensity();
    m_currentCommandList->UpdateBuffer(m_constantBuffer.get(), &ui, sizeof(ui));

    m_currentCommandList->SetVertexBuffer(0, m_vertexBuffer.get(), sizeof(SpriteVertex), 0);
    m_currentCommandList->VSSetConstantBuffer(0, m_constantBuffer.get());
    m_currentCommandList->PSSetConstantBuffer(0, m_constantBuffer.get());
    m_currentCommandList->PSSetTexture(0, texture);
    m_currentCommandList->Draw(kVertexCountPerSprite, 0);
}
