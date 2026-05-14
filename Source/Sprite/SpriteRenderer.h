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

// エンジン側で所有する 2D スプライト描画基盤。PSO、シェーダー、動的
// 頂点バッファ、描画ごとの定数バッファを保持する。描画は常に
// HUDPass 実行中の Begin -> Draw* -> End を通る。
// 座標規約は HUD_HPBar_Spec_2026-05-05 v3 の 3.3 節に合わせる。
//   dx, dy : 画面ピクセル座標、左上原点。
//   dw, dh : 画面ピクセル単位のサイズ。
//   sx, sy, sw, sh : テクスチャ内のピクセル座標。
//   angleRad : ラジアン単位で、矩形中央を軸に回転する。
class SpriteRenderer
{
public:
    static SpriteRenderer& Instance();

    // PSO、シェーダー、バッファを構築する。ResourceFactory が使える
    // エンジン起動後に一度だけ呼ぶ。DX12 専用で、DX11 経路は
    // 仕様 3.7 節に従い HUD では no-op とする。
    void Initialize(IResourceFactory* factory);
    void Finalize();

    // Begin / End は SpriteRenderer のコマンドリスト範囲を区切る。
    // ここでコマンドリストと viewport ピクセルサイズを保持し、
    // Draw() が毎回問い合わせずに NDC を計算できるようにする。
    void Begin(ICommandList* commandList, const DirectX::XMFLOAT2& viewportPx);
    void End();

    // テクスチャ全体を描画するバリアント。
    void Draw(const Sprite& sprite,
              float dx, float dy,
              float dw, float dh,
              float angleRad,
              const DirectX::XMFLOAT4& tintColor);

    // バーの塗り幅や atlas glyph 用に、テクスチャの一部だけを描画するバリアント。
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
    std::shared_ptr<IBuffer>        m_vertexBuffer;     // 4 頂点分の動的バッファ。
    std::shared_ptr<IBuffer>        m_constantBuffer;   // 256 バイト境界にそろえた UIConstants。

    ICommandList* m_currentCommandList = nullptr;
    DirectX::XMFLOAT2 m_currentViewport{ 0.0f, 0.0f };

    bool m_initialized = false;
};
