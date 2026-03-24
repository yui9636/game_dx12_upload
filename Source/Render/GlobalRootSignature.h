//#pragma once
//#include <wrl.h>
//#include <d3d11.h>
//#include "ShaderCommon.h"
//#include "RenderContext/RenderState.h"
//#include "ShadowMap.h"
//
//class GlobalRootSignature {
//public:
//    static GlobalRootSignature& Instance() {
//        static GlobalRootSignature instance;
//        return instance;
//    }
//
//    void Initialize(ID3D11Device* device);
//
//    void BindAll(ID3D11DeviceContext* dc, const RenderState* renderState, const ShadowMap* shadowMap);
//
//    ID3D11Buffer* GetSceneBuffer() const { return m_cbScene.Get(); }
//    ID3D11Buffer* GetShadowBuffer() const { return m_cbShadow.Get(); }
//
//    void SetIBL(ID3D11ShaderResourceView* diff, ID3D11ShaderResourceView* spec) {
//        m_diffIBL = diff;
//        m_specIBL = spec;
//    }
//
//private:
//    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbScene;
//    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbShadow;
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_diffIBL;
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_specIBL;
//
//    GlobalRootSignature() = default;
//};
#pragma once
#include <memory>
#include <wrl.h>
#include <d3d11.h>
#include "ShaderCommon.h"
#include "RenderContext/RenderState.h"
#include "ShadowMap.h"

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

    void BindAll(ICommandList* commandList, const RenderState* renderState, const ShadowMap* shadowMap);

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
