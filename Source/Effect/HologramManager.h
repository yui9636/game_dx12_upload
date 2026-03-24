//#pragma once
//
//#include <memory>
//#include <vector>
//#include <string>
//#include <d3d11.h>
//#include <wrl/client.h>
//#include <DirectXMath.h>
//#include "Model/Model.h"
//
//class Actor;
//struct RenderContext;
//
//static const int MAX_HOLOGRAM_INSTANCES = 32;
//
//struct HologramInstance
//{
//    bool isActive = false;
//    float timer = 0.0f;
//    float duration = 0.0f;
//
//    std::shared_ptr<Model> model;
//
//    std::vector<DirectX::XMFLOAT4X4> nodeTransforms;
//
//    HologramShader::CbHologram params;
//
//    DirectX::XMFLOAT3 offsetPosition = { 0.0f, 0.0f, 0.0f };
//};
//
//class HologramManager
//{
//public:
//    static HologramManager& Instance();
//
//    HologramManager();
//    ~HologramManager();
//
//    void Initialize(ID3D11Device* device);
//    void Finalize();
//
//    void Update(float dt);
//    void Render(const RenderContext& rc);
//
//    HologramShader* GetShaderRaw() const { return shader.get(); }
//
//
//    void Spawn(
//        const std::shared_ptr<Actor>& targetActor,
//        const HologramShader::CbHologram* initialParams = nullptr,
//        const DirectX::XMFLOAT3& offset = { 0.0f, 0.0f, 0.0f }
//    );
//
//    void Clear();
//
//    void SetNoiseTexture(const std::string& filename);
//
//    size_t GetActiveInstanceCount() const;
//
//private:
//    ID3D11Device* device = nullptr;
//    std::unique_ptr<HologramShader> shader;
//
//    std::array<HologramInstance, MAX_HOLOGRAM_INSTANCES> instances;
//
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
//};
