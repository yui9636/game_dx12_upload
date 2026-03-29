#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"
#include <DirectXMath.h>
#include <memory>
#include <vector>

class IShader;
class IBuffer;
class IInputLayout;

class GridRenderSystem {
public:
    struct EditorGridSettings
    {
        float cellSize = 20.0f;
        int halfLineCount = 32;
        float height = 0.01f;
    };

    GridRenderSystem() = default;
    ~GridRenderSystem();

    void Render(Registry& registry, RenderContext& rc);
    void RenderEditorGrid(RenderContext& rc, const EditorGridSettings& settings);

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;
    };

    bool EnsureResources();
    void EnsureVertexCapacity(uint32_t requiredVertexCount);
    void AppendLine(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const DirectX::XMFLOAT4& color);
    void AppendWorldAlignedGrid(const DirectX::XMFLOAT3& center, int halfLineCount, float cellSize, float y,
        const DirectX::XMFLOAT4& minorColor, const DirectX::XMFLOAT4& axisXColor, const DirectX::XMFLOAT4& axisZColor);
    void AppendTransformedGrid(const DirectX::XMFLOAT4X4& transform, int subdivisions, float scale,
        const DirectX::XMFLOAT4& minorColor, const DirectX::XMFLOAT4& axisXColor, const DirectX::XMFLOAT4& axisZColor);
    void Flush(RenderContext& rc);

    std::unique_ptr<IShader> m_vertexShader;
    std::unique_ptr<IShader> m_pixelShader;
    std::unique_ptr<IInputLayout> m_inputLayout;
    std::unique_ptr<IBuffer> m_vertexBuffer;
    std::unique_ptr<IBuffer> m_constantBuffer;
    std::vector<Vertex> m_vertices;
    uint32_t m_vertexCapacity = 0;
};
