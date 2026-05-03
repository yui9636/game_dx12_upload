#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "RHI/IBuffer.h"
#include "RHI/IShader.h"
#include "RHI/IState.h"

class ICommandList;
class IResourceFactory;
class ITexture;

class Font
{
public:
    Font(IResourceFactory* factory, const char* filename, int maxSpriteCount = 2048);
    virtual ~Font() = default;

    bool IsValid() const { return m_isValid; }

    void Begin(ICommandList* commandList, float viewportWidth, float viewportHeight);
    void Draw(float x, float y, const wchar_t* string);
    void Draw3D(DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, const wchar_t* string);
    void End(ICommandList* commandList);

    float GetTextWidth(const wchar_t* string) const;

    void SetScale(float x, float y) { m_scaleX = x; m_scaleY = y; }
    void SetColor(const DirectX::XMFLOAT4& color) { m_fontColor = color; }
    void SetSDFParams(float threshold = 0.5f, float softness = 0.5f);

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    struct SDFData
    {
        DirectX::XMFLOAT4 Color;
        float Threshold;
        float Softness;
        float Padding[2];
    };

    struct CBMatrix
    {
        DirectX::XMFLOAT4X4 World;
        DirectX::XMFLOAT4X4 View;
        DirectX::XMFLOAT4X4 Projection;
    };

    struct CharacterInfo
    {
        static const uint16_t NonCode = 0;
        static const uint16_t EndCode = 0xFFFF;
        static const uint16_t ReturnCode = 0xFFFE;
        static const uint16_t TabCode = 0xFFFD;
        static const uint16_t SpaceCode = 0xFFFC;

        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        float xoffset = 0.0f;
        float yoffset = 0.0f;
        float xadvance = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        int page = 0;
        bool ascii = false;
    };

    struct Subset
    {
        ITexture* texture = nullptr;
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
    };

    bool LoadFontData(IResourceFactory* factory, const char* filename);
    void AddGlyphQuad(float x, float y, const CharacterInfo& info, bool ndc2D);
    void PushSubsetIfNeeded(int page);

    std::unique_ptr<IShader> m_vertexShader;
    std::unique_ptr<IShader> m_pixelShader;
    std::unique_ptr<IInputLayout> m_inputLayout;
    std::unique_ptr<IBuffer> m_vertexBuffer;
    std::unique_ptr<IBuffer> m_indexBuffer;
    std::unique_ptr<IBuffer> m_sdfConstantBuffer;
    std::unique_ptr<IBuffer> m_matrixBuffer;

    std::vector<std::shared_ptr<ITexture>> m_textures;
    std::vector<CharacterInfo> m_characterInfos;
    std::vector<uint16_t> m_characterIndices;
    std::vector<Subset> m_subsets;
    std::vector<Vertex> m_vertices;

    Vertex* m_currentVertex = nullptr;
    uint32_t m_currentIndexCount = 0;
    int m_currentPage = -1;
    int m_maxSpriteCount = 0;

    float m_fontWidth = 0.0f;
    float m_fontHeight = 0.0f;
    int m_textureCount = 0;
    int m_characterCount = 0;

    float m_screenWidth = 0.0f;
    float m_screenHeight = 0.0f;
    float m_scaleX = 1.0f;
    float m_scaleY = 1.0f;
    DirectX::XMFLOAT4 m_fontColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    float m_sdfThreshold = 0.5f;
    float m_sdfSoftness = 0.5f;

    bool m_is3DMode = false;
    bool m_isValid = false;

    DirectX::XMFLOAT4X4 m_currentWorld;
    DirectX::XMFLOAT4X4 m_currentView;
    DirectX::XMFLOAT4X4 m_currentProj;
};
