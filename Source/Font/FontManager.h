#pragma once
#include <map>
#include <string>
#include <memory>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Font.h"


// 整列設定
enum class FontAlign
{
    Left,   // 左揃え (通常)
    Center, // 中央揃え (座標を中心に描画)
    Right   // 右揃え (座標を右端として描画)
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
    // 高機能描画メソッド
    // --------------------------------------------------------------------------

    // 書式指定 + アライメント + 色 + スケール
    // 例: DrawFormat(dc, "Main", x, y, {1,0,0,1}, 1.5f, FontAlign::Center, L"%d DMG", damage);
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

    // シンプル版 (色は白、スケール1.0、左揃え)
    void DrawFormat(
        ID3D11DeviceContext* dc,
        const std::string& key,
        float x, float y,
        const wchar_t* format,
        ...
    );

    void DrawFormat3D(
        ID3D11DeviceContext* dc,            // RenderContext ではなく DC を直接受ける
        const DirectX::XMMATRIX& view,       // ★追加
        const DirectX::XMMATRIX& projection, // ★追加
        const std::string& key,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& rotation,   // 回転を指定
        float scale,
        const DirectX::XMFLOAT4& color,
        FontAlign align,
        const wchar_t* format,
        ...
    );




private:
    std::map<std::string, std::shared_ptr<Font>> fonts;
};