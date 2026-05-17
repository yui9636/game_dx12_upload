#pragma once

#include <memory>
#include <DirectXMath.h>
// ITexture はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

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

    Model* EnsureTrainingStageModel();

    static constexpr uint32_t PREVIEW_SIZE = 512;

    OffscreenRenderer* m_offscreen = nullptr;
    std::shared_ptr<ITexture> m_previewTexture;
    std::unique_ptr<ITexture> m_previewDepth;
    std::shared_ptr<Model> m_trainingStageModel;
    bool m_trainingStageLoadAttempted = false;
};
