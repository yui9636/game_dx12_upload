#pragma once
#include <vector>
#include <memory>
#include <string>
#include "FrameGraphTypes.h"

class ITexture;
class IResourceFactory;

class RenderGraphResourcePool {
public:
    RenderGraphResourcePool() = default;
    ~RenderGraphResourcePool() = default;

    // �q�ɂ���e�N�X�`�����؂��i������΍H��ɔ������ĐV�i�����j
    std::unique_ptr<ITexture> AcquireTexture(const std::string& name, const TextureDesc& desc, IResourceFactory* factory, uint64_t currentFrame);

    // �g���I������e�N�X�`����q�ɂɕԋp����
    void ReleaseTexture(const TextureDesc& desc, std::unique_ptr<ITexture> texture, uint64_t currentFrame);

    // �����Ԏg���Ă��Ȃ��e�N�X�`������������i�K�x�[�W�R���N�V�����j
    void Tick(uint64_t currentFrame);

private:
    struct PooledTexture {
        TextureDesc desc;
        std::unique_ptr<ITexture> texture;
        uint64_t lastUsedFrame = 0;
    };

    std::vector<PooledTexture> m_pool;
};