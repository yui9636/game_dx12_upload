#pragma once
#include <memory>

class ITexture;
class Model;
class MaterialAsset;
class OffscreenRenderer;

class MaterialPreviewStudio {
public:
    static MaterialPreviewStudio& Instance();
    void Initialize(OffscreenRenderer* offscreen);

    void RequestPreview(MaterialAsset* material);
    void PumpPreview();
    ITexture* GetPreviewTexture() const { return m_previewTexture.get(); }
    bool IsReady() const;
    bool IsDirty() const { return m_dirty; }

private:
    MaterialPreviewStudio() = default;
    ~MaterialPreviewStudio();

    void ExecuteRender();

    static constexpr int PREVIEW_SIZE = 256;

    OffscreenRenderer* m_offscreen = nullptr;
    std::shared_ptr<ITexture> m_previewTexture;  // RT+SRV, RGBA8_UNORM
    std::unique_ptr<ITexture> m_previewDepth;     // D24_UNORM_S8_UINT
    std::shared_ptr<Model> m_sphereModel;

    MaterialAsset* m_pendingMaterial = nullptr;
    bool m_dirty = false;
};
