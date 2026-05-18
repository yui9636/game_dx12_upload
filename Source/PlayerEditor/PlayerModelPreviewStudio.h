#pragma once

#include <memory>
#include <cstdint>
#include <DirectXMath.h>
// ITexture はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class ITexture;
class Model;
class OffscreenRenderer;
class Registry;

class PlayerModelPreviewStudio
{
public:
    static PlayerModelPreviewStudio& Instance();

    void Initialize(OffscreenRenderer* offscreen);
    bool IsReady() const;
    ITexture* GetPreviewTexture() const { return m_previewTexture.get(); }

    // registry/previewEntity は ColliderComponent gizmo を Gizmos 経路で
    // 同じ RT 内に焼き込むために必要。NULL なら gizmo は描かれない。
    // previewEntity は EntityID = uint64_t (truncate 厳禁)
    void RenderPreview(
        const Model* model,
        const DirectX::XMFLOAT3& cameraPosition,
        const DirectX::XMFLOAT3& cameraTarget,
        float aspect,
        float fovY,
        float nearZ,
        float farZ,
        const DirectX::XMFLOAT4& clearColor,
        float previewScale,
        Registry* registry,
        uint64_t previewEntity);

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
