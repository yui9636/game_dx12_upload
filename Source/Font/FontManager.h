#pragma once
#include <map>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Font.h"

struct ImFont;

enum class FontAlign
{
    Left,
    Center,
    Right
};

class FontManager
{
private:
    FontManager() = default;
    ~FontManager() = default;

public:
    static FontManager& Instance()
    {
        static FontManager instance;
        return instance;
    }

    void Clear();
    void Load(ID3D11Device* device, const std::string& key, const char* filename);
    std::shared_ptr<Font> Get(const std::string& key);

    // --------------------------------------------------------------------------
    // --------------------------------------------------------------------------

    void DrawFormat(
        ID3D11DeviceContext* dc,
        const std::string& key,
        float x, float y,
        const DirectX::XMFLOAT4& color,
        float scale,
        FontAlign align,
        const wchar_t* format,
        ...
    );

    void DrawFormat(
        ID3D11DeviceContext* dc,
        const std::string& key,
        float x, float y,
        const wchar_t* format,
        ...
    );

    void DrawFormat3D(
        ID3D11DeviceContext* dc,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const std::string& key,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& rotation,
        float scale,
        const DirectX::XMFLOAT4& color,
        FontAlign align,
        const wchar_t* format,
        ...
    );

    void QueueEditorPreviewFont(const std::string& assetPath, float previewSize = 48.0f);
    ImFont* GetEditorPreviewFont(const std::string& assetPath) const;
    void ProcessEditorPreviewFonts();
    void ClearEditorPreviewFonts();




private:
    std::map<std::string, std::shared_ptr<Font>> fonts;
    std::unordered_map<std::string, ImFont*> m_editorPreviewFonts;
    std::unordered_set<std::string> m_editorPreviewFontFailures;
    std::unordered_set<std::string> m_pendingEditorPreviewFonts;
    float m_editorPreviewFontSize = 48.0f;
};
