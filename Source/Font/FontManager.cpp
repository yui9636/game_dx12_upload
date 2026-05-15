#include "FontManager.h"

// FontManagerの実装。
// ランタイム文字描画とエディタプレビュー用フォント読み込みを一元管理する。

#include "Graphics.h"
#include "ImGuiRenderer.h"
#include "Console/Logger.h"
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
    // ランタイム文字描画で使用するビューポート幅。0なら画面サイズへフォールバックする。
    float g_runtimeViewportWidth = 0.0f;
    // ランタイム文字描画で使用するビューポート高さ。0なら画面サイズへフォールバックする。
    float g_runtimeViewportHeight = 0.0f;

    // フォントアセットパスを比較しやすい正規化済み文字列へ変換する。
    std::string NormalizeFontAssetPath(const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    }

    // ランタイムTextが直接読み込めるアウトラインフォントかを判定する。
    bool IsRuntimeTrueTypeFontPath(const std::string& key)
    {
        std::string ext = std::filesystem::path(key).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return ext == ".ttf" || ext == ".otf";
    }

    // printf形式のワイド文字列を安全に展開してバッファとして返す。
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

    // 未設定時はGraphicsの画面幅を使って描画用幅を返す。
    float GetFallbackViewportWidth()
    {
        if (g_runtimeViewportWidth > 0.0f) {
            return g_runtimeViewportWidth;
        }
        return (std::max)(1.0f, Graphics::Instance().GetScreenWidth());
    }

    // 未設定時はGraphicsの画面高さを使って描画用高さを返す。
    float GetFallbackViewportHeight()
    {
        if (g_runtimeViewportHeight > 0.0f) {
            return g_runtimeViewportHeight;
        }
        return (std::max)(1.0f, Graphics::Instance().GetScreenHeight());
    }
}

// 読み込み済みランタイムフォントをすべて解放する。
void FontManager::Clear()
{
    fonts.clear();
    m_runtimeFontFailures.clear();
}

// DX11互換呼び出し用。実際の読み込みはRHIファクトリ版へ委譲する。
void FontManager::Load(ID3D11Device*, const std::string& key, const char* filename)
{
    Load(Graphics::Instance().GetResourceFactory(), key, filename);
}

// 指定キーでフォントを読み込み、成功した場合のみキャッシュへ登録する。
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

// 指定キーのフォントを取得する。存在しない場合はnullptrを返す。
std::shared_ptr<Font> FontManager::Get(const std::string& key)
{
    auto it = fonts.find(key);
    return (it != fonts.end()) ? it->second : nullptr;
}

// ランタイム文字描画で使用するビューポートサイズを設定する。
void FontManager::SetRuntimeViewport(float width, float height)
{
    g_runtimeViewportWidth = (std::max)(0.0f, width);
    g_runtimeViewportHeight = (std::max)(0.0f, height);
}

// ランタイム文字描画のビューポート指定を解除する。
void FontManager::ClearRuntimeViewport()
{
    g_runtimeViewportWidth = 0.0f;
    g_runtimeViewportHeight = 0.0f;
}

// 指定キーのフォントを取得し、無ければ指定されたTTF/OTFだけを読み込む。
std::shared_ptr<Font> FontManager::GetOrLoadDefault(const std::string& key)
{
    auto font = Get(key);
    if (font) {
        return font;
    }

    if (key.empty()) {
        if (m_runtimeFontFailures.insert(key).second) {
            LOG_ERROR("[FontManager] Runtime text font path is empty. No fallback font will be loaded.");
        }
        return nullptr;
    }

    if (m_runtimeFontFailures.find(key) != m_runtimeFontFailures.end()) {
        return nullptr;
    }

    if (!IsRuntimeTrueTypeFontPath(key)) {
        m_runtimeFontFailures.insert(key);
        LOG_ERROR("[FontManager] Runtime text font must be .ttf or .otf: %s", key.c_str());
        return nullptr;
    }

    Load(Graphics::Instance().GetResourceFactory(), key, key.c_str());
    font = Get(key);
    if (font) {
        return font;
    }

    m_runtimeFontFailures.insert(key);
    LOG_ERROR("[FontManager] Failed to load runtime text font: %s", key.c_str());
    return nullptr;
}

// 指定キーのランタイムフォントの行高さを返す。
float FontManager::GetLineHeight(const std::string& key)
{
    auto font = GetOrLoadDefault(key);
    return font ? font->GetLineHeight() : 32.0f;
}

// RHIコマンドリストを使って、色・倍率・揃え指定ありの2D文字列を描画する。
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

    // 揃え指定に応じて描画開始X座標を補正する。
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

    // Fontに描画設定を渡し、Begin/Draw/Endの順で即時描画する。
    font->SetColor(color);
    font->SetScale(scale, scale);
    font->Begin(commandList, GetFallbackViewportWidth(), GetFallbackViewportHeight());
    font->Draw(drawX, y, buffer.data());
    font->End(commandList);
}

// DX11コンテキストをRHIコマンドリストへ包んで2D文字列を描画する。
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

// RHIコマンドリストを使って、既定色・既定倍率の2D文字列を描画する。
void FontManager::DrawFormat(ICommandList* commandList, const std::string& key, float x, float y, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    std::vector<wchar_t> buffer = FormatWideString(format, args);
    va_end(args);

    DrawFormat(commandList, key, x, y, { 1, 1, 1, 1 }, 1.0f, FontAlign::Left, L"%s", buffer.data());
}

// DX11コンテキストを使って、既定色・既定倍率の2D文字列を描画する。
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

// RHIコマンドリストを使って、3D空間上に文字列を描画する。
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

    // 揃え指定に応じてローカル空間上のXオフセットを求める。
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

    // 文字列のローカル変換を作り、指定位置へ配置する。
    const XMMATRIX offsetMatrix = XMMatrixTranslation(offsetX, 0.0f, 0.0f);
    const float worldScale = scale * 0.02f;
    const XMMATRIX scaleMatrix = XMMatrixScaling(worldScale, worldScale, 1.0f);
    const XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z));
    const XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);
    const XMMATRIX world = offsetMatrix * scaleMatrix * rotationMatrix * translationMatrix;

    // Fontに描画設定を渡し、Begin/Draw/Endの順で即時描画する。
    font->SetColor(color);
    font->SetScale(1.0f, 1.0f);
    font->Begin(commandList, GetFallbackViewportWidth(), GetFallbackViewportHeight());
    font->Draw3D(world, view, projection, buffer.data());
    font->End(commandList);
}

// DX11コンテキストをRHIコマンドリストへ包んで3D文字列を描画する。
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

// エディタプレビュー用フォントの読み込みを予約する。
void FontManager::QueueEditorPreviewFont(const std::string& assetPath, float previewSize)
{
    if (assetPath.empty()) {
        return;
    }
    // 同じフォントを重複して読み込まないよう、正規化したパスで管理する。
    const std::string normalized = NormalizeFontAssetPath(assetPath);
    if (m_editorPreviewFonts.find(normalized) != m_editorPreviewFonts.end() ||
        m_editorPreviewFontFailures.find(normalized) != m_editorPreviewFontFailures.end()) {
        return;
    }
    m_editorPreviewFontSize = previewSize;
    m_pendingEditorPreviewFonts.insert(normalized);
}

// 読み込み済みのエディタプレビュー用ImFontを取得する。
ImFont* FontManager::GetEditorPreviewFont(const std::string& assetPath) const
{
    if (assetPath.empty()) {
        return nullptr;
    }
    // 同じフォントを重複して読み込まないよう、正規化したパスで管理する。
    const std::string normalized = NormalizeFontAssetPath(assetPath);
    auto it = m_editorPreviewFonts.find(normalized);
    return it != m_editorPreviewFonts.end() ? it->second : nullptr;
}

// 予約されたエディタプレビュー用フォントをImGuiのフォントアトラスへ追加する。
void FontManager::ProcessEditorPreviewFonts()
{
    if (m_pendingEditorPreviewFonts.empty()) {
        return;
    }

    // 予約集合を一度ローカルへ移し、処理中の追加と衝突しないようにする。
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

        // 日本語グリフを含めてTTFをImGuiフォントとして追加する。
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

    // フォントが増えた場合はImGui側のフォントアトラスを再構築する。
    if (rebuildRequired) {
        ImGuiRenderer::RebuildFontAtlas();
    }
}

// エディタプレビュー用フォントのキャッシュと失敗履歴をクリアする。
void FontManager::ClearEditorPreviewFonts()
{
    m_editorPreviewFonts.clear();
    m_editorPreviewFontFailures.clear();
    m_pendingEditorPreviewFonts.clear();
}
