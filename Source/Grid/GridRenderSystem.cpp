#include "GridRenderSystem.h"
#include "Component/GridComponent.h"
#include "Component/TransformComponent.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include "RHI/IState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "System/Query.h"
#include <algorithm>
#include <cmath>

namespace
{
    DirectX::XMFLOAT4 ScaleAlpha(const DirectX::XMFLOAT4& color, float alpha)
    {
        return DirectX::XMFLOAT4(color.x, color.y, color.z, alpha);
    }
}

GridRenderSystem::~GridRenderSystem() = default;

bool GridRenderSystem::EnsureResources()
{
    if (m_vertexShader && m_pixelShader && m_inputLayout && m_constantBuffer) {
        return true;
    }

    IResourceFactory* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        return false;
    }

    InputLayoutElement layoutElements[] = {
        { "POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement },
        { "COLOR", 0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement },
    };

    m_vertexShader = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PrimitiveRendererVS.cso");
    m_pixelShader = factory->CreateShader(ShaderType::Pixel, "Data/Shader/PrimitiveRendererPS.cso");
    if (!m_vertexShader || !m_pixelShader) {
        return false;
    }

    InputLayoutDesc layoutDesc{ layoutElements, _countof(layoutElements) };
    m_inputLayout = factory->CreateInputLayout(layoutDesc, m_vertexShader.get());
    m_constantBuffer = factory->CreateBuffer(sizeof(CbScene), BufferType::Constant);
    return m_inputLayout && m_constantBuffer;
}

void GridRenderSystem::EnsureVertexCapacity(uint32_t requiredVertexCount)
{
    if (requiredVertexCount <= m_vertexCapacity && m_vertexBuffer) {
        return;
    }

    IResourceFactory* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        return;
    }

    m_vertexCapacity = (std::max)(requiredVertexCount, 4096u);
    const uint32_t byteSize = m_vertexCapacity * static_cast<uint32_t>(sizeof(Vertex));
    m_vertexBuffer = factory->CreateBuffer(byteSize, BufferType::Vertex);
}

void GridRenderSystem::AppendLine(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const DirectX::XMFLOAT4& color)
{
    m_vertices.push_back(Vertex{ a, color });
    m_vertices.push_back(Vertex{ b, color });
}

void GridRenderSystem::AppendWorldAlignedGrid(const DirectX::XMFLOAT3& center, int halfLineCount, float cellSize, float y,
    const DirectX::XMFLOAT4& minorColor, const DirectX::XMFLOAT4& axisXColor, const DirectX::XMFLOAT4& axisZColor)
{
    const float snappedCenterX = std::floor(center.x / cellSize) * cellSize;
    const float snappedCenterZ = std::floor(center.z / cellSize) * cellSize;
    const float minX = snappedCenterX - static_cast<float>(halfLineCount) * cellSize;
    const float maxX = snappedCenterX + static_cast<float>(halfLineCount) * cellSize;
    const float minZ = snappedCenterZ - static_cast<float>(halfLineCount) * cellSize;
    const float maxZ = snappedCenterZ + static_cast<float>(halfLineCount) * cellSize;

    for (int i = -halfLineCount; i <= halfLineCount; ++i) {
        const float worldX = snappedCenterX + static_cast<float>(i) * cellSize;
        const DirectX::XMFLOAT4 color = (std::fabs(worldX) < 0.001f) ? axisXColor : minorColor;
        AppendLine({ worldX, y, minZ }, { worldX, y, maxZ }, color);
    }

    for (int i = -halfLineCount; i <= halfLineCount; ++i) {
        const float worldZ = snappedCenterZ + static_cast<float>(i) * cellSize;
        const DirectX::XMFLOAT4 color = (std::fabs(worldZ) < 0.001f) ? axisZColor : minorColor;
        AppendLine({ minX, y, worldZ }, { maxX, y, worldZ }, color);
    }
}

void GridRenderSystem::AppendTransformedGrid(const DirectX::XMFLOAT4X4& transform, int subdivisions, float scale,
    const DirectX::XMFLOAT4& minorColor, const DirectX::XMFLOAT4& axisXColor, const DirectX::XMFLOAT4& axisZColor)
{
    using namespace DirectX;

    const int lineCount = (std::max)(subdivisions, 1);
    const float halfExtent = 0.5f * static_cast<float>(lineCount) * scale;
    const XMMATRIX world = XMLoadFloat4x4(&transform);

    auto transformPoint = [&](float x, float y, float z) {
        XMFLOAT3 result{};
        XMStoreFloat3(&result, XMVector3TransformCoord(XMVectorSet(x, y, z, 1.0f), world));
        return result;
    };

    for (int i = 0; i <= lineCount; ++i) {
        const float local = -halfExtent + static_cast<float>(i) * scale;
        const DirectX::XMFLOAT4 colorX = (std::fabs(local) < 0.001f) ? axisXColor : minorColor;
        AppendLine(
            transformPoint(local, 0.0f, -halfExtent),
            transformPoint(local, 0.0f, halfExtent),
            colorX);

        const DirectX::XMFLOAT4 colorZ = (std::fabs(local) < 0.001f) ? axisZColor : minorColor;
        AppendLine(
            transformPoint(-halfExtent, 0.0f, local),
            transformPoint(halfExtent, 0.0f, local),
            colorZ);
    }
}

void GridRenderSystem::Flush(RenderContext& rc)
{
    if (m_vertices.empty() || !EnsureResources()) {
        m_vertices.clear();
        return;
    }

    EnsureVertexCapacity(static_cast<uint32_t>(m_vertices.size()));
    if (!m_vertexBuffer) {
        m_vertices.clear();
        return;
    }

    rc.commandList->VSSetShader(m_vertexShader.get());
    rc.commandList->PSSetShader(m_pixelShader.get());
    rc.commandList->SetInputLayout(m_inputLayout.get());
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::LineList);
    rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Alpha));
    rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
    rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
    rc.commandList->SetViewport(rc.mainViewport);

    using namespace DirectX;
    const XMMATRIX view = XMLoadFloat4x4(&rc.viewMatrix);
    const XMMATRIX projection = XMLoadFloat4x4(&rc.projectionMatrix);
    CbScene cbScene{};
    XMStoreFloat4x4(&cbScene.viewProjection, view * projection);

    if (auto* dx12Cmd = dynamic_cast<DX12CommandList*>(rc.commandList)) {
        dx12Cmd->VSSetDynamicConstantBuffer(0, &cbScene, sizeof(cbScene));
    } else {
        rc.commandList->VSSetConstantBuffer(0, m_constantBuffer.get());
        rc.commandList->UpdateBuffer(m_constantBuffer.get(), &cbScene, sizeof(cbScene));
    }

    constexpr uint32_t kChunkVertexCount = 8192;
    const uint32_t stride = sizeof(Vertex);
    const uint32_t offset = 0;
    rc.commandList->SetVertexBuffer(0, m_vertexBuffer.get(), stride, offset);

    const uint32_t totalVertexCount = static_cast<uint32_t>(m_vertices.size());
    uint32_t startVertex = 0;
    while (startVertex < totalVertexCount) {
        const uint32_t drawVertexCount = (std::min)(kChunkVertexCount, totalVertexCount - startVertex);
        rc.commandList->UpdateBuffer(
            m_vertexBuffer.get(),
            m_vertices.data() + startVertex,
            drawVertexCount * static_cast<uint32_t>(sizeof(Vertex)));
        rc.commandList->Draw(drawVertexCount, 0);
        startVertex += drawVertexCount;
    }

    m_vertices.clear();
}

void GridRenderSystem::Render(Registry& registry, RenderContext& rc)
{
    Query<GridComponent, TransformComponent> query(registry);
    query.ForEach([&](GridComponent& grid, TransformComponent& trans) {
        if (!grid.enabled) {
            return;
        }

        const DirectX::XMFLOAT4 minorColor = ScaleAlpha(grid.color, grid.color.w > 0.0f ? grid.color.w : 0.28f);
        const DirectX::XMFLOAT4 axisXColor = { 0.85f, 0.35f, 0.35f, 0.85f };
        const DirectX::XMFLOAT4 axisZColor = { 0.35f, 0.60f, 0.90f, 0.85f };
        AppendTransformedGrid(trans.worldMatrix, grid.subdivisions, grid.scale, minorColor, axisXColor, axisZColor);
    });

    Flush(rc);
}

void GridRenderSystem::RenderEditorGrid(RenderContext& rc, const EditorGridSettings& settings)
{
    const float cellSize = (std::max)(settings.cellSize, 1.0f);
    const int halfLineCount = (std::max)(settings.halfLineCount, 1);
    const DirectX::XMFLOAT4 minorColor = { 0.36f, 0.39f, 0.44f, 0.34f };
    const DirectX::XMFLOAT4 axisXColor = { 0.82f, 0.32f, 0.32f, 0.85f };
    const DirectX::XMFLOAT4 axisZColor = { 0.32f, 0.58f, 0.86f, 0.85f };
    AppendWorldAlignedGrid({ 0.0f, 0.0f, 0.0f }, halfLineCount, cellSize, settings.height,
        minorColor, axisXColor, axisZColor);
    Flush(rc);
}
