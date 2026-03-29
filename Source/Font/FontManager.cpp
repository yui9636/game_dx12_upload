#include "FontManager.h"
#include "ImGuiRenderer.h"
#include <cstdarg>
#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <vector>


using namespace DirectX;

namespace
{
    std::string NormalizeFontAssetPath(const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    }
}

void FontManager::Clear()
{
    fonts.clear();
}

void FontManager::Load(ID3D11Device* device, const std::string& key, const char* filename)
{
    if (fonts.find(key) != fonts.end()) return;
    auto font = std::make_shared<Font>(device, filename);
    fonts[key] = font;
}

std::shared_ptr<Font> FontManager::Get(const std::string& key)
{
    auto it = fonts.find(key);
    return (it != fonts.end()) ? it->second : nullptr;
}

void FontManager::DrawFormat(
    ID3D11DeviceContext* dc,
    const std::string& key,
    float x, float y,
    const DirectX::XMFLOAT4& color,
    float scale,
    FontAlign align,
    const wchar_t* format,
    ...)
{
    auto font = Get(key);
    if (!font) return;

    va_list args;
    va_start(args, format);
    int len = _vscwprintf(format, args) + 1;
    std::vector<wchar_t> buffer(len);
    vswprintf_s(buffer.data(), len, format, args);
    va_end(args);

    float drawX = x;
    if (align != FontAlign::Left)
    {
        float width = font->GetTextWidth(buffer.data()) * scale;

        if (align == FontAlign::Center)
            drawX -= width * 0.5f;
        else if (align == FontAlign::Right)
            drawX -= width;
    }

    font->SetColor(color);
    font->SetScale(scale, scale);

    font->Begin(dc);
    font->Draw(drawX, y, buffer.data());
    font->End(dc);
}

void FontManager::DrawFormat(ID3D11DeviceContext* dc, const std::string& key, float x, float y, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    int len = _vscwprintf(format, args) + 1;
    std::vector<wchar_t> buffer(len);
    vswprintf_s(buffer.data(), len, format, args);
    va_end(args);

    DrawFormat(dc, key, x, y, { 1,1,1,1 }, 1.0f, FontAlign::Left, L"%s", buffer.data());
}

void FontManager::DrawFormat3D(
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
)
{
    auto font = Get(key);
    if (!font) return;

    va_list args;
    va_start(args, format);
    int len = _vscwprintf(format, args) + 1;
    std::vector<wchar_t> buffer(len);
    vswprintf_s(buffer.data(), len, format, args);
    va_end(args);

    float offsetX = 0.0f;
    if (align != FontAlign::Left)
    {
        float width = font->GetTextWidth(buffer.data());
        if (align == FontAlign::Center) offsetX = -width * 0.5f;
        else if (align == FontAlign::Right) offsetX = -width;
    }

    XMMATRIX MOffset = XMMatrixTranslation(offsetX, 0.0f, 0.0f);

    float worldScale = scale * 0.02f;
    XMMATRIX MScale = XMMatrixScaling(worldScale, worldScale, 1.0f);

    XMMATRIX MRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    XMMATRIX MTrans = XMMatrixTranslation(position.x, position.y, position.z);

    XMMATRIX World = MOffset * MScale * MRot * MTrans;

    font->SetColor(color);

    font->SetScale(1.0f, 1.0f);

    font->Begin(dc);
    font->Draw3D(World, view, projection, buffer.data());
    font->End(dc);
}

void FontManager::QueueEditorPreviewFont(const std::string& assetPath, float previewSize)
{
    if (assetPath.empty()) {
        return;
    }
    const std::string normalized = NormalizeFontAssetPath(assetPath);
    if (m_editorPreviewFonts.find(normalized) != m_editorPreviewFonts.end() ||
        m_editorPreviewFontFailures.find(normalized) != m_editorPreviewFontFailures.end()) {
        return;
    }
    m_editorPreviewFontSize = previewSize;
    m_pendingEditorPreviewFonts.insert(normalized);
}

ImFont* FontManager::GetEditorPreviewFont(const std::string& assetPath) const
{
    if (assetPath.empty()) {
        return nullptr;
    }
    const std::string normalized = NormalizeFontAssetPath(assetPath);
    auto it = m_editorPreviewFonts.find(normalized);
    return it != m_editorPreviewFonts.end() ? it->second : nullptr;
}

void FontManager::ProcessEditorPreviewFonts()
{
    if (m_pendingEditorPreviewFonts.empty()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    bool rebuildRequired = false;
    std::vector<std::string> pending(m_pendingEditorPreviewFonts.begin(), m_pendingEditorPreviewFonts.end());
    m_pendingEditorPreviewFonts.clear();

    for (const std::string& path : pending) {
        if (m_editorPreviewFonts.find(path) != m_editorPreviewFonts.end() ||
            m_editorPreviewFontFailures.find(path) != m_editorPreviewFontFailures.end()) {
            continue;
        }

        if (!std::filesystem::exists(path)) {
            m_editorPreviewFontFailures.insert(path);
            continue;
        }

        ImFontConfig config{};
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = false;
        ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), m_editorPreviewFontSize, &config, io.Fonts->GetGlyphRangesJapanese());
        if (font) {
            m_editorPreviewFonts.emplace(path, font);
            rebuildRequired = true;
        } else {
            m_editorPreviewFontFailures.insert(path);
        }
    }

    if (rebuildRequired) {
        ImGuiRenderer::RebuildFontAtlas();
    }
}

void FontManager::ClearEditorPreviewFonts()
{
    m_editorPreviewFonts.clear();
    m_editorPreviewFontFailures.clear();
    m_pendingEditorPreviewFonts.clear();
}
