#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"
#include <DirectXMath.h>
#include <memory>
#include <vector>

class IShader;
class IBuffer;
class IInputLayout;

// ワイヤーグリッドを描画するシステム。
// GridComponent を持つ entity 用のグリッド描画と、
// editor 専用のワールド固定グリッド描画の両方を担当する。
class GridRenderSystem {
public:
    // Editor 用ワールドグリッドの描画設定。
    struct EditorGridSettings
    {
        // 1 マスの大きさ。
        float cellSize = 20.0f;

        // 原点から片側に何本線を引くか。
        int halfLineCount = 32;

        // グリッドを描く高さ。
        float height = 0.01f;
    };

    // デフォルト構築。
    GridRenderSystem() = default;

    // 内部 GPU リソースを保持するためデストラクタを定義している。
    ~GridRenderSystem();

    // Registry 内の GridComponent を走査し、それぞれの transform に応じたグリッドを描画する。
    void Render(Registry& registry, RenderContext& rc);

    // Editor 用のワールド固定グリッドを描画する。
    void RenderEditorGrid(RenderContext& rc, const EditorGridSettings& settings);

private:
    // ライン描画用の頂点。
    struct Vertex
    {
        // 頂点位置。
        DirectX::XMFLOAT3 position;

        // 頂点色。
        DirectX::XMFLOAT4 color;
    };

    // シェーダへ渡すシーン定数。
    struct CbScene
    {
        // ViewProjection 行列。
        DirectX::XMFLOAT4X4 viewProjection;
    };

    // 描画に必要なシェーダ・入力レイアウト・定数バッファを作成する。
    bool EnsureResources();

    // 必要頂点数を満たすように頂点バッファ容量を拡張する。
    void EnsureVertexCapacity(uint32_t requiredVertexCount);

    // 線分 1 本ぶんの頂点を CPU 側配列へ追加する。
    void AppendLine(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const DirectX::XMFLOAT4& color);

    // ワールド座標に揃った XZ 平面グリッドを追加する。
    void AppendWorldAlignedGrid(const DirectX::XMFLOAT3& center, int halfLineCount, float cellSize, float y,
        const DirectX::XMFLOAT4& minorColor, const DirectX::XMFLOAT4& axisXColor, const DirectX::XMFLOAT4& axisZColor);

    // 任意 transform を持つローカルグリッドを追加する。
    void AppendTransformedGrid(const DirectX::XMFLOAT4X4& transform, int subdivisions, float scale,
        const DirectX::XMFLOAT4& minorColor, const DirectX::XMFLOAT4& axisXColor, const DirectX::XMFLOAT4& axisZColor);

    // 現在たまっている頂点を GPU へ送り、実際に描画する。
    void Flush(RenderContext& rc);

    // 頂点シェーダ。
    std::unique_ptr<IShader> m_vertexShader;

    // ピクセルシェーダ。
    std::unique_ptr<IShader> m_pixelShader;

    // 入力レイアウト。
    std::unique_ptr<IInputLayout> m_inputLayout;

    // ライン頂点を送るための頂点バッファ。
    std::unique_ptr<IBuffer> m_vertexBuffer;

    // ViewProjection 行列用の定数バッファ。
    std::unique_ptr<IBuffer> m_constantBuffer;

    // CPU 側で一時的に保持する頂点列。
    std::vector<Vertex> m_vertices;

    // 現在確保済みの頂点バッファ容量。
    uint32_t m_vertexCapacity = 0;
};