#pragma once
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include "Entity/Entity.h"

class RenderPipeline;
class Registry;
class MaterialAsset;
class Model;

class MaterialPreviewStudio {
public:
    static MaterialPreviewStudio& Instance();
    void Initialize(ID3D11Device* device);
    void RenderPreview(MaterialAsset* material, float scaleMult = 1.0f, float rotY = 0.0f);
    ID3D11ShaderResourceView* GetPreviewSRV() const;
    bool IsReady() const;

private:
    MaterialPreviewStudio() = default;
    ~MaterialPreviewStudio() = default;

    std::unique_ptr<RenderPipeline> m_previewPipeline;
    std::unique_ptr<Registry> m_previewRegistry;
    EntityID m_previewCamera = Entity::NULL_ID;

    std::shared_ptr<Model> m_sphereModel;
    bool m_initialized = false;
};