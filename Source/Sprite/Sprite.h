#pragma once

#include <memory>
#include <string>
#include <DirectXMath.h>

class ITexture;

// Data-only sprite. Holds the texture reference and per-sprite tint /
// glow parameters. The actual GPU draw is issued by SpriteRenderer (see
// HUD_HPBar_Spec_2026-05-05 v3 section 3.2). Sprite no longer owns any
// pipeline state, vertex buffer, or D3D11 / D3D12 native resources.
class Sprite
{
public:
    Sprite();
    explicit Sprite(const std::string& texturePath);
    ~Sprite() = default;

    void SetTexture(std::shared_ptr<ITexture> texture);
    void SetTexture(const std::string& texturePath);
    ITexture* GetTexture() const { return m_texture.get(); }
    const std::shared_ptr<ITexture>& GetTextureShared() const { return m_texture; }

    int GetTextureWidth() const  { return m_textureWidth; }
    int GetTextureHeight() const { return m_textureHeight; }

    void SetColor(const DirectX::XMFLOAT4& c) { m_color = c; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }

    void SetGlow(const DirectX::XMFLOAT3& color, float intensity)
    {
        m_glowColor = color;
        m_glowIntensity = intensity;
    }
    const DirectX::XMFLOAT3& GetGlowColor() const { return m_glowColor; }
    float GetGlowIntensity() const { return m_glowIntensity; }

private:
    std::shared_ptr<ITexture> m_texture;
    int m_textureWidth  = 0;
    int m_textureHeight = 0;

    DirectX::XMFLOAT4 m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_glowColor = { 0.0f, 0.0f, 0.0f };
    float m_glowIntensity = 0.0f;
};
