#pragma once

#include <memory>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <map>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <functional>
#include "EffectNode.h"
#include "ShaderClass/ShaderCompiler.h"

class EffectNode;
class EffectVariantShader;
class Model;
struct RenderContext;

class EffectInstance
{
public:
    std::shared_ptr<EffectNode> rootNode;

    bool isDead = false;

    float age = 0.0f;
    float lifeTime = 2.0f;
    float prevAge = -1.0f;
    bool loop = false;

    float fadeInTime = 0.0f;
    float fadeOutTime = 0.0f;
    float masterAlpha = 1.0f;


   
    bool isSequencerControlled = false;

    EffectTransform overrideLocalTransform;

    DirectX::XMFLOAT4X4 parentMatrix;

    void Stop(bool immediate = false);
 

    EffectInstance(std::shared_ptr<EffectNode> root) : rootNode(root)
    {
        DirectX::XMStoreFloat4x4(&parentMatrix, DirectX::XMMatrixIdentity());
        overrideLocalTransform.position = { 0,0,0 };
        overrideLocalTransform.rotation = { 0,0,0 };
        overrideLocalTransform.scale = { 1,1,1 };
    }
};

// =================================================================
// =================================================================
class EffectManager
{
public:
    static EffectManager& Get() { static EffectManager instance; return instance; }

    // --------------------------------------------------------
    // --------------------------------------------------------
    void Initialize(ID3D11Device* device);
    void Finalize();

    void Update(float dt);
    void Render( RenderContext& rc);

    // --------------------------------------------------------
    // --------------------------------------------------------

    std::shared_ptr<EffectInstance> Play(const std::string& effectName, const DirectX::XMFLOAT3& position);

    void StopAll();

    void SyncInstanceToTime(std::shared_ptr<EffectInstance> instance, float targetTime);
    // --------------------------------------------------------
    // --------------------------------------------------------

    std::shared_ptr<::Model> GetModel(const std::string& path);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture(const std::string& path);

    std::shared_ptr<EffectVariantShader> GetStandardShader() const;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> GetPixelShaderVariant(int flags);


    void SetEditorMode(bool enable) { isEditorMode = enable; }


    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetCommonCurlNoise() { return commonCurlNoiseSRV; }
private:
    EffectManager() = default;
    ~EffectManager() = default;
    EffectManager(const EffectManager&) = delete;
    void operator=(const EffectManager&) = delete;

    // --------------------------------------------------------
    // --------------------------------------------------------
    ID3D11Device* device = nullptr;

    std::list<std::shared_ptr<EffectInstance>> instances;

    std::map<int, Microsoft::WRL::ComPtr<ID3D11PixelShader>> psCache;
    std::shared_ptr<EffectVariantShader> standardShader;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> commonCurlNoiseSRV;
    bool isEditorMode = false;

    std::unique_ptr<ShaderCompiler> shaderCompiler;
};
