#pragma once

#include <memory>
#include <DirectXMath.h>

class IBuffer;
class ICommandList;
class IInputLayout;
class IPipelineState;
class IResourceFactory;
class IShader;
class Sprite;

// Engine-owned 2D sprite render base. Holds the PSO / shaders / dynamic
// vertex buffer / per-draw constant buffer. Drawing always goes through
// Begin -> Draw* -> End during the HUDPass execution.
//
// Coordinate convention (per HUD_HPBar_Spec_2026-05-05 v3 section 3.3):
//   dx, dy : screen pixel, top-left origin
//   dw, dh : screen pixel size
//   sx, sy, sw, sh : texture pixel coordinates
//   angleRad : radians, rotates around the quad centre
class SpriteRenderer
{
public:
    static SpriteRenderer& Instance();

    // Build PSO / shaders / buffers. Called once at engine bring-up after
    // the resource factory is available (DX12 only; the DX11 path is a
    // no-op for HUD purposes per spec section 3.7).
    void Initialize(IResourceFactory* factory);
    void Finalize();

    // Begin / End frame the SpriteRenderer's command list scope. The
    // command list and viewport pixel size are captured here so Draw()
    // can compute NDC without re-querying every call.
    void Begin(ICommandList* commandList, const DirectX::XMFLOAT2& viewportPx);
    void End();

    // Full-rect texture variant.
    void Draw(const Sprite& sprite,
              float dx, float dy,
              float dw, float dh,
              float angleRad,
              const DirectX::XMFLOAT4& tintColor);

    // Source sub-rect variant (for filled bars / atlas glyphs).
    void Draw(const Sprite& sprite,
              float dx, float dy,
              float dw, float dh,
              float sx, float sy, float sw, float sh,
              float angleRad,
              const DirectX::XMFLOAT4& tintColor);

    void DrawQuad(const Sprite& sprite,
                  const DirectX::XMFLOAT2& p0,
                  const DirectX::XMFLOAT2& p1,
                  const DirectX::XMFLOAT2& p2,
                  const DirectX::XMFLOAT2& p3,
                  const DirectX::XMFLOAT4& tintColor);

    void DrawQuadUV(const Sprite& sprite,
                    const DirectX::XMFLOAT2& p0,
                    const DirectX::XMFLOAT2& p1,
                    const DirectX::XMFLOAT2& p2,
                    const DirectX::XMFLOAT2& p3,
                    const DirectX::XMFLOAT2& uv0,
                    const DirectX::XMFLOAT2& uv1,
                    const DirectX::XMFLOAT2& uv2,
                    const DirectX::XMFLOAT2& uv3,
                    const DirectX::XMFLOAT4& tintColor);

    bool IsActive() const { return m_currentCommandList != nullptr; }

private:
    SpriteRenderer() = default;
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    void DrawInternal(const Sprite& sprite,
                      float dx, float dy, float dw, float dh,
                      float sx, float sy, float sw, float sh,
                      float angleRad,
                      const DirectX::XMFLOAT4& tintColor);

    void DrawQuadInternal(const Sprite& sprite,
                          const DirectX::XMFLOAT2& p0,
                          const DirectX::XMFLOAT2& p1,
                          const DirectX::XMFLOAT2& p2,
                          const DirectX::XMFLOAT2& p3,
                          const DirectX::XMFLOAT2& uv0,
                          const DirectX::XMFLOAT2& uv1,
                          const DirectX::XMFLOAT2& uv2,
                          const DirectX::XMFLOAT2& uv3,
                          const DirectX::XMFLOAT4& tintColor);

    std::shared_ptr<IShader>        m_vs;
    std::shared_ptr<IShader>        m_ps;
    std::shared_ptr<IInputLayout>   m_inputLayout;
    std::shared_ptr<IPipelineState> m_pso;
    std::shared_ptr<IBuffer>        m_vertexBuffer;     // 4 vertices, dynamic
    std::shared_ptr<IBuffer>        m_constantBuffer;   // UIConstants, aligned to 256

    ICommandList* m_currentCommandList = nullptr;
    DirectX::XMFLOAT2 m_currentViewport{ 0.0f, 0.0f };

    bool m_initialized = false;
};
