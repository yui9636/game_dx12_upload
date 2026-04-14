#pragma once

#include <memory>
#include <DirectXMath.h>

class ITexture;
class Model;
class OffscreenRenderer;

class PlayerModelPreviewStudio
{
public:
    static PlayerModelPreviewStudio& Instance();

    void Initialize(OffscreenRenderer* offscreen);
    bool IsReady() const;
    ITexture* GetPreviewTexture() const { return m_previewTexture.get(); }

    void RenderPreview(
        const Model* model,
        const DirectX::XMFLOAT3& cameraPosition,
        const DirectX::XMFLOAT3& cameraTarget,
        float aspect,
        float fovY,
        float nearZ,
        float farZ,
        const DirectX::XMFLOAT4& clearColor,
        float previewScale);

private:
    PlayerModelPreviewStudio() = default;
    ~PlayerModelPreviewStudio() = default;

    static constexpr uint32_t PREVIEW_SIZE = 512;

    OffscreenRenderer* m_offscreen = nullptr;
    std::shared_ptr<ITexture> m_previewTexture;
    std::unique_ptr<ITexture> m_previewDepth;
};
