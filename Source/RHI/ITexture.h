#pragma once
#include <cstdint>

// RHI�ň����e�N�X�`���t�H�[�}�b�g
enum class TextureFormat {
    Unknown,
    RGBA8_UNORM,
    R16G16B16A16_FLOAT,
    R32G32B32A32_FLOAT,
    R32G32B32A32_UINT,
    R32G32B32_FLOAT,
    R32G32_FLOAT,
    R16G16_FLOAT,        // �� �ǉ��F���[�V�����x�N�g���iVelocity�j�p
    R8_UNORM,
    D32_FLOAT,
    D24_UNORM_S8_UINT,
    R32_TYPELESS // DoF�̐[�x�ǂݍ��݂Ȃǂ̓���p�r
};


enum class ResourceState {
    Common,            // �ėp�iDX12�̏�����ԂȂǁj
    RenderTarget,      // �����_�[�^�[�Q�b�g�Ƃ��ď������ݒ�
    DepthWrite,        // �[�x�o�b�t�@�Ƃ��ď������ݒ�
    DepthRead,         // �[�x�o�b�t�@�Ƃ��ēǂݎ�蒆
    ShaderResource,    // �V�F�[�_�[����ǂݎ�蒆�iSRV�j
    UnorderedAccess,   // UAV�Ƃ��ēǂݏ�����
    Present            // ��ʕ\���p
};


// �e�N�X�`���̎g�����i�r�b�g�t���O�j
enum class TextureBindFlags : uint32_t {
    None = 0,
    ShaderResource = 1 << 0,
    RenderTarget = 1 << 1,
    DepthStencil = 1 << 2,
    UnorderedAccess = 1 << 3,
};
inline TextureBindFlags operator|(TextureBindFlags a, TextureBindFlags b) {
    return static_cast<TextureBindFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(TextureBindFlags a, TextureBindFlags b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// =========================================================
// �����ȃe�N�X�`���C���^�[�t�F�[�X
// =========================================================
class ITexture {
public:
    virtual ~ITexture() = default;

    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual TextureFormat GetFormat() const = 0;

    virtual ResourceState GetCurrentState() const = 0;
    virtual void SetCurrentState(ResourceState state) = 0;
};