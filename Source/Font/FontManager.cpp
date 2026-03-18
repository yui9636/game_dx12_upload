#include "FontManager.h"
#include <cstdarg>
#include <vector>


using namespace DirectX;

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

// フル機能版 DrawFormat
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

    // 1. 文字列整形
    va_list args;
    va_start(args, format);
    int len = _vscwprintf(format, args) + 1;
    std::vector<wchar_t> buffer(len);
    vswprintf_s(buffer.data(), len, format, args);
    va_end(args);

    // 2. アライメント計算 (座標補正)
    float drawX = x;
    if (align != FontAlign::Left)
    {
        // 文字列の幅を取得してオフセット計算
        float width = font->GetTextWidth(buffer.data()) * scale;

        if (align == FontAlign::Center)
            drawX -= width * 0.5f; // 半分左へずらす
        else if (align == FontAlign::Right)
            drawX -= width;        // 全部左へずらす
    }

    // 3. 設定適用 & 描画
    // ステートを保存・復元する仕組みはないため、毎回設定します
    font->SetColor(color);
    font->SetScale(scale, scale);

    font->Begin(dc);
    font->Draw(drawX, y, buffer.data());
    font->End(dc);
}

// シンプル版 (オーバーロード)
void FontManager::DrawFormat(ID3D11DeviceContext* dc, const std::string& key, float x, float y, const wchar_t* format, ...)
{
    // 引数転送のため、ここでもva_list処理が必要
    va_list args;
    va_start(args, format);
    int len = _vscwprintf(format, args) + 1;
    std::vector<wchar_t> buffer(len);
    vswprintf_s(buffer.data(), len, format, args);
    va_end(args);

    // デフォルト設定で呼び出し
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

    // 1. 文字列整形
    va_list args;
    va_start(args, format);
    int len = _vscwprintf(format, args) + 1;
    std::vector<wchar_t> buffer(len);
    vswprintf_s(buffer.data(), len, format, args);
    va_end(args);

    // 2. アライメント計算
    float offsetX = 0.0f;
    if (align != FontAlign::Left)
    {
        float width = font->GetTextWidth(buffer.data());
        if (align == FontAlign::Center) offsetX = -width * 0.5f;
        else if (align == FontAlign::Right) offsetX = -width;
    }

    // 3. 行列計算
    XMMATRIX MOffset = XMMatrixTranslation(offsetX, 0.0f, 0.0f);

    // スケール補正 (0.02f)
    float worldScale = scale * 0.02f;
    XMMATRIX MScale = XMMatrixScaling(worldScale, worldScale, 1.0f);

    // 回転
    XMMATRIX MRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    // 平行移動
    XMMATRIX MTrans = XMMatrixTranslation(position.x, position.y, position.z);

    // ワールド行列
    XMMATRIX World = MOffset * MScale * MRot * MTrans;

    // 4. 描画
    font->SetColor(color);

    font->SetScale(1.0f, 1.0f);

    font->Begin(dc);
    // 引数で受け取った view, projection をそのまま渡す
    font->Draw3D(World, view, projection, buffer.data());
    font->End(dc);
}
