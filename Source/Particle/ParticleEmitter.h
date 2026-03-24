#pragma once
#include "Effect/EffectNode.h"           
#include "Particle/ParticleSetting.h"      
#include "Particle/compute_particle_system.h" 
#include <random>

namespace ImGG { class Gradient; }
class RenderContext;

class ParticleEmitter : public EffectNode
{
public:
    ParticleEmitter();
    ~ParticleEmitter() override;

    void Update(float dt) override;
    void UpdateWithAge(float age, float lifeTime) override;

    void Render(RenderContext& rc) override;

    bool SampleMeshSurface(const Model* model, DirectX::XMFLOAT3& outPos, DirectX::XMFLOAT3& outNormal);

    void Reset() override;

    // --- Editor Linkage ---
    void SyncSettingsToGradient(ImGG::Gradient& outGradient);
    void SyncGradientToSettings(const ImGG::Gradient& inGradient);
    void LoadTexture(const std::string& path);

    void SetMasterAlpha(float alpha) { m_masterAlpha = alpha; }
public:
    ParticleSetting settings;
    ParticleRendererSettings renderSettings;

    Model* GetParentModel() const;

    std::shared_ptr<compute_particle_system> particleSystem;
    std::string texturePath;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_curlNoiseSRV;
private:
    bool SampleMeshSurface(DirectX::XMFLOAT3& outPos, DirectX::XMFLOAT3& outNormal);
    void Emit(int count);


    float m_accumulatedTime = 0.0f;
    float m_spawnAccumulator = 0.0f;
    bool  m_burstFired = false;
    std::mt19937 m_randomEngine;

    float m_currentRootAge = 0.0f;
    float m_rootLifeTime = 0.0f;

    unsigned int m_seed = 0;

    float m_masterAlpha = 1.0f;


    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_meshVS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_meshInputLayout;

    void LoadMeshShader();

};
