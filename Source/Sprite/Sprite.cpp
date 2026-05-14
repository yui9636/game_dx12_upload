#include "Sprite.h"

#include "RHI/ITexture.h"
#include "System/ResourceManager.h"

Sprite::Sprite() = default;
// Sprite::Sprite はこのモジュールの実行時処理を構成する補助処理を行う。

Sprite::Sprite(const std::string& texturePath)
{
    SetTexture(texturePath);
}

void Sprite::SetTexture(std::shared_ptr<ITexture> texture)
{
    m_texture = std::move(texture);
    if (m_texture) {
        m_textureWidth  = static_cast<int>(m_texture->GetWidth());
        m_textureHeight = static_cast<int>(m_texture->GetHeight());
    } else {
        m_textureWidth  = 0;
        m_textureHeight = 0;
    }
}

void Sprite::SetTexture(const std::string& texturePath)
{
    if (texturePath.empty()) {
        SetTexture(std::shared_ptr<ITexture>{});
        return;
    }
    SetTexture(ResourceManager::Instance().GetTexture(texturePath));
}
