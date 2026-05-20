#pragma once
#include <memory>
#include <wrl.h>
#include <d3d11.h>
#include "ShaderCommon.h"
#include "RenderContext/RenderState.h"
#include "Render/ShadowMap.h"
// ICommandList はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class ICommandList;
class IBuffer;
class ITexture;
class DX12Device;

class GlobalRootSignature {
public:
    static GlobalRootSignature& Instance();

    void Initialize(ID3D11Device* device);
    void Initialize(DX12Device* device);

    ~GlobalRootSignature();

    void BindAll(ICommandList* commandList, const RenderState* renderState, const ShadowMap* shadowMap,
        IBuffer* sceneBufferOverride = nullptr, IBuffer* shadowBufferOverride = nullptr);

    IBuffer* GetSceneBuffer() const;
    IBuffer* GetShadowBuffer() const;

    void SetIBL(ITexture* diff, ITexture* spec) {
        m_diffIBL = diff;
        m_specIBL = spec;
    }


private:
    std::unique_ptr<IBuffer> m_cbScene;
    std::unique_ptr<IBuffer> m_cbShadow;

    ITexture* m_diffIBL = nullptr;
    ITexture* m_specIBL = nullptr;

    bool m_isDX12 = false;

    GlobalRootSignature() = default;
};
