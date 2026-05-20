#pragma once
#include <memory>
#include <wrl.h>
#include <d3d11.h>
#include "ShaderCommon.h"
#include "RenderContext/RenderState.h"
#include "Render/ShadowMap.h"

class ICommandList;
class IBuffer;
class ITexture;
class DX12Device;

class GlobalRootSignature {
public:
    // scene / shadow 共通定数バッファを持つ singleton。
    static GlobalRootSignature& Instance();

    // DX11 用の共通 constant buffer を初期化する。
    void Initialize(ID3D11Device* device);
    // DX12 用の共通 constant buffer を初期化する。
    void Initialize(DX12Device* device);

    ~GlobalRootSignature();

    // RenderState と ShadowMap から CbScene / CbShadowMap を更新し、shader slot へ bind する。
    void BindAll(ICommandList* commandList, const RenderState* renderState, const ShadowMap* shadowMap,
        IBuffer* sceneBufferOverride = nullptr, IBuffer* shadowBufferOverride = nullptr);

    IBuffer* GetSceneBuffer() const;
    IBuffer* GetShadowBuffer() const;

    // image based lighting 用の diffuse / specular cubemap を設定する。texture は外部所有。
    void SetIBL(ITexture* diff, ITexture* spec) {
        m_diffIBL = diff;
        m_specIBL = spec;
    }


private:
    std::unique_ptr<IBuffer> m_cbScene;  // CbScene 用 constant buffer。
    std::unique_ptr<IBuffer> m_cbShadow; // CbShadowMap 用 constant buffer。

    ITexture* m_diffIBL = nullptr; // diffuse IBL texture。非所有。
    ITexture* m_specIBL = nullptr; // specular IBL texture。非所有。

    bool m_isDX12 = false; // DX12 backend で初期化されたか。

    GlobalRootSignature() = default;
};
