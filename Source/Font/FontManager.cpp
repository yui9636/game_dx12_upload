#include "FontManager.h"

#include "Graphics.h"
#include "ImGuiRenderer.h"
#include "RHI/DX11/DX11CommandList.h"
#include "RHI/ICommandList.h"
#include "RHI/IResourceFactory.h"

#include <algorithm>
#include <cstdarg>
#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>

using namespace DirectX;

namespace
{
    constexpr const char* kDefaultBitmapFontPath = "Data/Font/Unnamed-1.fnt";

    std::string NormalizeFontAssetPath(const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    }

    std::vector<wchar_t> FormatWideString(const wchar_t* format, va_list args)
    {
        if (!format) {
            return { L'\0' };
        }

        va_list countArgs;
        va_copy(countArgs, args);
        const int len = _vscwprintf(format, countArgs);
        va_end(countArgs);

        if (len <= 0) {
            return { L'\0' };
        }

        std::vector<wchar_t> buffer(static_cast<size_t>(len) + 1u);
        vswprintf_s(buffer.data(), buffer.size(), format, args);
        return buffer;
    }

    float GetFallbackViewportWidth()
    {
        return (std::max)(1.0f, Graphics::Instance().GetScreenWidth());
    }

    float GetFallbackViewportHeight()
    {
        return (std::max)(1.0f, Graphics::Instance().GetScreenHeight());
    }
}

void FontManager::Clear()
{
    fonts.clear();
}

void FontManager::Load(ID3D11Device* /*device*/, const std::string& key, const char* filename)
{
    Load(Graphics::Instance().GetResourceFactory(), key, filename);
}

void FontManager::Load(IResourceFactory* factory, const std::string& key, const char* filename)
{
    if (key.empty() || fonts.find(key) != fonts.end()) {
        return;
    }

    auto font = std::make_shared<Font>(factory, filename);
    if (!font->IsValid()) {
        return;
    }

    fonts[key] = font;
}

std::shared_ptr<Font> FontManager::Get(const std::string& key)
{
    auto it = fonts.find(key);
    return (it != fonts.end()) ? it->second : nullptr;
}

std::shared_ptr<Font> FontManager::GetOrLoadDefault(const std::string& key)
{
    auto font = Get(key);
    if (font) {
        return font;
    }

    Load(Graphics::Instance().GetResourceFactory(), key, kDefaultBitmapFontPath);
    return Get(key);
}

void FontManager::DrawFormat(
    ICommandList* commandList,
    const std::string& key,
    float x, float y,
    const DirectX::XMFLOAT4& color,
    float scale,
    FontAlign align,
    const wchar_t* format,
    ...)
{
    if (!commandList) {
        return;
    }

    auto font = GetOrLoadDefault(key);
    if (!font) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vector<wchar_t> buffer = FormatWideString(format, args);
    va_end(args);

    float drawX = x;
    if (align != FontAlign::Left)
    {
        const float width = font->GetTextWidth(buffer.data()) * scale;
        if (align == FontAlign::Center) {
            drawX -= width * 0.5f;
        } else if (align == FontAlign::Right) {
            drawX -= width;
        }
    }

    font->SetColor(color);
    font->SetScale(scale, scale);
    font->Begin(commandList, GetFallbackViewportWidth(), GetFallbackViewportHeight());
    font->Draw(drawX, y, buffer.data());
    font->End(commandList);
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
    if (!dc) {
        return;
    }

    DX11CommandList commandList(dc);

    va_list args;
    va_start(args, format);
    std::vector<wchar_t> buffer = FormatWideString(format, args);
    va_end(args);

    DrawFormat(&commandList, key, x, y, color, scale, align, L"%s", buffer.data());
}

void FontManager::DrawFormat(ICommandList* commandList, const std::string& key, float x, float y, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    std::vector<wchar_t> buffer = FormatWideString(format, args);
    va_end(args);

    DrawFormat(commandList, key, x, y, { 1, 1, 1, 1 }, 1.0f, FontAlign::Left, L"%s", buffer.data());
}

void FontManager::DrawFormat(ID3D11DeviceContext* dc, const std::string& key, float x, float y, const wchar_t* format, ...)
{
    if (!dc) {
        return;
    }

    DX11CommandList commandList(dc);

    va_list args;
    va_start(args, format);
    std::vector<wchar_t> buffer = FormatWideString(format, args);
    va_end(args);

    DrawFormat(&commandList, key, x, y, { 1, 1, 1, 1 }, 1.0f, FontAlign::Left, L"%s", buffer.data());
}

void FontManager::DrawFormat3D(
    ICommandList* commandList,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& projection,
    const std::string& key,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& rotation,
    float scale,
    const DirectX::XMFLOAT4& color,
    FontAlign align,
    const wchar_t* format,
    ...)
{
    if (!commandList) {
        return;
    }

    auto font = GetOrLoadDefault(key);
    if (!font) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vector<wchar_t> buffer = FormatWideString(format, args);
    va_end(args);

    float offsetX = 0.0f;
    if (align != FontAlign::Left)
    {
        const float width = font->GetTextWidth(buffer.data());
        if (align == FontAlign::Center) {
            offsetX = -width * 0.5f;
        } else if (align == FontAlign::Right) {
            offsetX = -width;
        }
    }

    const XMMATRIX offsetMatrix = XMMatrixTranslation(offsetX, 0.0f, 0.0f);
    const float worldScale = scale * 0.02f;
    const XMMATRIX scaleMatrix = XMMatrixScaling(worldScale, worldScale, 1.0f);
    const XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z));
    const XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);
    const XMMATRIX world = offsetMatrix * scaleMatrix * rotationMatrix * translationMatrix;

    font->SetColor(color);
    font->SetScale(1.0f, 1.0f);
    font->Begin(commandList, GetFallbackViewportWidth(), GetFallbackViewportHeight());
    font->Draw3D(world, view, projection, buffer.data());
    font->End(commandList);
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
    ...)
{
    if (!dc) {
        return;
    }

    DX11CommandList commandList(dc);

    va_list args;
    va_start(args, format);
    std::vector<wchar_t> buffer = FormatWideString(format, args);
    va_end(args);

    DrawFormat3D(&commandList, view, projection, key, position, rotation, scale, color, align, L"%s", buffer.data());
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
