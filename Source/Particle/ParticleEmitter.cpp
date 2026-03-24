//#include "ParticleEmitter.h"
//#include "Particle/ParticleUtils.h"
//#include "GpuResourceUtils.h"
//#include "imgui_gradient/imgui_gradient.hpp"
//#include"Model/Model.h"
//#include "Effect/MeshEmitter.h"
//#include "Effect/EffectManager.h"
//
//using namespace DirectX;
//
//// =========================================================
//// =========================================================
//ParticleEmitter::ParticleEmitter()
//{
//   
//
//    name = "Particle Emitter";
//    type = EffectNodeType::Particle;
//
//    settings.count = 10;
//    settings.spawnRate = 10.0f;
//    settings.lifeSeconds = 2.0f;
//    settings.shape = ShapeType::Sphere;
//    settings.radius = 0.5f;
//
//    ID3D11Device* device = Graphics::Instance().GetDevice();
//    particleSystem = std::make_shared<compute_particle_system>(
//        device,
//        1024,
//        nullptr,
//        settings.textureSplitCount
//    );
//
//    LoadTexture("Data/Effect/particle/particle.png");
//
//    m_curlNoiseSRV = EffectManager::Get().GetCommonCurlNoise();
//
//    std::random_device rd;
//    m_randomEngine.seed(rd());
//}
//
//ParticleEmitter::~ParticleEmitter()
//{
//    particleSystem.reset();
//}
//
//// =========================================================
//// =========================================================
//void ParticleEmitter::LoadTexture(const std::string& path)
//{
//    ID3D11Device* device = Graphics::Instance().GetDevice();
//    if (!device) return;
//
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
//    D3D11_TEXTURE2D_DESC desc{};
//    HRESULT hr = GpuResourceUtils::LoadTexture(
//        device,
//        path.c_str(),
//        srv.ReleaseAndGetAddressOf(),
//        &desc
//    );
//
//    if (SUCCEEDED(hr) && srv)
//    {
//        textureSRV = srv;
//        texturePath = path;
//        if (particleSystem) particleSystem->ChangeTexture(textureSRV);
//    }
//}
//
//// =========================================================
//// =========================================================
//
//
//void ParticleEmitter::UpdateWithAge(float age, float lifeTime)
//{
//    EffectNode::UpdateWithAge(age, lifeTime);
//
//    m_currentRootAge = age;
//    m_rootLifeTime = lifeTime;
//}
//
//
//void ParticleEmitter::Update(float dt)
//{
//    EffectNode::Update(dt);
//
//    if (!particleSystem) return;
//
//    if (dt <= 0.0f) return;
//
//    // =================================================================
//    // =================================================================
//    RenderContext rc = {};
//    rc.commandList->GetNativeContext() = Graphics::Instance().GetDeviceContext();
//    rc.renderState = Graphics::Instance().GetRenderState();
//    rc.camera = Graphics::Instance().GetCamera();
//    rc.lightManager = Graphics::Instance().GetLightManager();
//
//    if (!rc.camera) return;
//
//    particleSystem->SetTextureSplitCount(settings.textureSplitCount);
//
//    particleSystem->SetCurlNoiseTexture(m_curlNoiseSRV);
//
//    particleSystem->SetCurlNoiseStrength(renderSettings.curlNoiseStrength);
//    particleSystem->SetCurlNoiseScale(renderSettings.curlNoiseScale);
//    particleSystem->SetCurlMoveSpeed(renderSettings.curlMoveSpeed);
//
//    particleSystem->SetVelocityStretchEnabled(renderSettings.velocityStretchEnabled);
//    particleSystem->SetVelocityStretchScale(renderSettings.velocityStretchScale);
//    particleSystem->SetVelocityStretchMaxAspect(renderSettings.velocityStretchMaxAspect);
//    particleSystem->SetVelocityStretchMinSpeed(renderSettings.velocityStretchMinSpeed);
//
//
//
//
//
//    particleSystem->Begin(rc);
//    particleSystem->Update(rc, dt);
//    particleSystem->End(rc);
//
//    if (m_currentRootAge >= m_rootLifeTime)
//    {
//    }
//
//    // =================================================================
//    // =================================================================
//    m_accumulatedTime += dt;
//
//    if (settings.burst && !m_burstFired)
//    {
//        int burstCount = settings.count * settings.burstFactor;
//        if (burstCount > 0) Emit(burstCount);
//        m_burstFired = true;
//    }
//
//    if (settings.spawnRate > 0.0f)
//    {
//        m_spawnAccumulator += dt;
//        float interval = 1.0f / settings.spawnRate;
//        int times = static_cast<int>(m_spawnAccumulator / interval);
//        if (times > 0)
//        {
//            m_spawnAccumulator -= interval * times;
//            Emit(settings.count * times);
//        }
//    }
//    else
//    {
//        Emit(settings.count);
//    }
//
//    if (settings.loop)
//    {
//        if (m_accumulatedTime >= settings.playSeconds)
//        {
//            m_accumulatedTime = 0.0f;
//            m_burstFired = false;
//            m_spawnAccumulator = 0.0f;
//        }
//    }
//}
//
//
//
//// =========================================================
//// =========================================================
//void ParticleEmitter::Render(RenderContext& rc)
//{
//    if (!particleSystem) return;
//
//    particleSystem->SetGlobalAlpha(m_masterAlpha);
//
//    particleSystem->Begin(rc);
//    particleSystem->Draw(rc);
//    particleSystem->End(rc);
//}
//
//// ParticleEmitter.cpp
//
//bool ParticleEmitter::SampleMeshSurface(DirectX::XMFLOAT3& outPos, DirectX::XMFLOAT3& outNormal)
//{
//
//    if (!parent) return false;
//    auto meshEmitter = dynamic_cast<MeshEmitter*>(parent);
//    if (!meshEmitter) return false;
//    auto model = meshEmitter->GetModel();
//    if (!model) return false;
//    const auto& meshes = model->GetMeshes();
//    if (meshes.empty()) return false;
//
//    std::uniform_int_distribution<int> meshDist(0, (int)meshes.size() - 1);
//    const auto& targetMesh = meshes[meshDist(m_randomEngine)];
//
//    if (targetMesh.indices.empty() || targetMesh.vertices.empty()) return false;
//    int triangleCount = (int)targetMesh.indices.size() / 3;
//    if (triangleCount <= 0) return false;
//
//    std::uniform_int_distribution<int> triDist(0, triangleCount - 1);
//    int triIndex = triDist(m_randomEngine) * 3;
//
//    uint32_t idx0 = targetMesh.indices[triIndex + 0];
//    uint32_t idx1 = targetMesh.indices[triIndex + 1];
//    uint32_t idx2 = targetMesh.indices[triIndex + 2];
//
//    DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&targetMesh.vertices[idx0].position);
//    DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&targetMesh.vertices[idx1].position);
//    DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&targetMesh.vertices[idx2].position);
//
//    DirectX::XMVECTOR n0 = DirectX::XMLoadFloat3(&targetMesh.vertices[idx0].normal);
//    DirectX::XMVECTOR n1 = DirectX::XMLoadFloat3(&targetMesh.vertices[idx1].normal);
//    DirectX::XMVECTOR n2 = DirectX::XMLoadFloat3(&targetMesh.vertices[idx2].normal);
//
//    float r1 = ParticleUtils::Random01(m_randomEngine);
//    float r2 = ParticleUtils::Random01(m_randomEngine);
//    if (r1 + r2 > 1.0f) { r1 = 1.0f - r1; r2 = 1.0f - r2; }
//
//    DirectX::XMVECTOR vPos = v0 + r1 * (v1 - v0) + r2 * (v2 - v0);
//
//    DirectX::XMVECTOR vNormal = n0 + r1 * (n1 - n0) + r2 * (n2 - n0);
//
//    DirectX::XMMATRIX parentWorldMat = DirectX::XMLoadFloat4x4(&meshEmitter->worldMatrix);
//    vPos = DirectX::XMVector3TransformCoord(vPos, parentWorldMat);
//
//    DirectX::XMVECTOR scale, rot, trans;
//    DirectX::XMMatrixDecompose(&scale, &rot, &trans, parentWorldMat);
//    DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationQuaternion(rot);
//
//    vNormal = DirectX::XMVector3TransformNormal(vNormal, rotMat);
//
//    DirectX::XMStoreFloat3(&outPos, vPos);
//    DirectX::XMStoreFloat3(&outNormal, vNormal);
//
//    return true;
//}
//
//void ParticleEmitter::Emit(int count)
//{
//    if (count <= 0) return;
//
//    XMMATRIX worldMat = XMLoadFloat4x4(&worldMatrix);
//    XMVECTOR worldPos, worldRot, worldScale;
//    XMMatrixDecompose(&worldScale, &worldRot, &worldPos, worldMat);
//
//    XMFLOAT3 basePos;
//    XMFLOAT4 baseRot;
//    XMStoreFloat3(&basePos, worldPos);
//    XMStoreFloat4(&baseRot, worldRot);
//
//    const auto& s = settings;
//
//    const Model* parentModel = nullptr;
//    XMMATRIX parentWorldMat = XMMatrixIdentity();
//    if (s.shape == ShapeType::Mesh && parent)
//    {
//        if (auto meshEmitter = dynamic_cast<MeshEmitter*>(parent))
//        {
//            if (meshEmitter->GetModel())
//            {
//                parentModel = meshEmitter->GetModel().get();
//                parentWorldMat = XMLoadFloat4x4(&meshEmitter->worldMatrix);
//            }
//        }
//    }
//
//    for (int i = 0; i < count; ++i)
//    {
//        compute_particle_system::emit_particle_data p{};
//
//        float life = (s.lifeMode == LifeMode::Constant)
//            ? s.lifeSeconds
//            : ParticleUtils::RandomRange(m_randomEngine, s.lifeMin, s.lifeMax);
//        if (life < 0.0f) life = 0.0f;
//
//        p.parameter = XMFLOAT4((float)s.spriteIndex, life, (float)s.spriteFrameCount, life);
//
//   
//     
//        DirectX::XMFLOAT3 meshNormal = { 0,1,0 };
//        bool positionSet = false;
//
//        if (s.shape == ShapeType::Mesh)
//        {
//            if (SampleMeshSurface(finalWorldPos, meshNormal))
//            {
//                positionSet = true;
//                shapeRefPos = finalWorldPos;
//            }
//        }
//
//        if (!positionSet)
//        {
//            shapeRefPos = ParticleUtils::SampleEmissionPosition(s, m_randomEngine);
//
//            meshNormal = ParticleUtils::SampleEmissionDirection(s, shapeRefPos, m_randomEngine);
//
//            XMFLOAT3 localPosWithType = shapeRefPos;
//            localPosWithType.x += s.position.x;
//            localPosWithType.y += s.position.y;
//            localPosWithType.z += s.position.z;
//
//            XMVECTOR P = XMLoadFloat3(&localPosWithType);
//            P = XMVector3Rotate(P, worldRot);
//            P = XMVectorAdd(P, worldPos);
//            XMStoreFloat3(&finalWorldPos, P);
//        }
//
//        p.position = XMFLOAT4(finalWorldPos.x, finalWorldPos.y, finalWorldPos.z, 1.0f);
//
//        // -------------------------------------------------------------
//        // -------------------------------------------------------------
//        XMFLOAT3 localDir = ParticleUtils::SampleEmissionDirection(s, shapeRefPos, m_randomEngine);
//
//        if (positionSet && s.shape == ShapeType::Mesh)
//        {
//            localDir = meshNormal;
//        }
//        else
//        {
//            localDir = ParticleUtils::SampleEmissionDirection(s, shapeRefPos, m_randomEngine);
//        }
//
//
//        XMFLOAT3 velocity = ParticleUtils::ComputeVelocity(s, localDir, baseRot, m_randomEngine);
//        p.velocity = XMFLOAT4(velocity.x, velocity.y, velocity.z, 0.0f);
//
//        XMFLOAT3 accel = s.acceleration;
//        if (s.useGravity)
//        {
//            XMVECTOR G = XMLoadFloat3(&s.gravityDirection);
//            G = XMVector3Normalize(G) * s.gravityPower;
//            XMFLOAT3 gVec;
//            XMStoreFloat3(&gVec, G);
//            accel.x += gVec.x; accel.y += gVec.y; accel.z += gVec.z;
//        }
//        p.acceleration = XMFLOAT4(accel.x, accel.y, accel.z, 0.0f);
//
//        p.rotation = baseRot;
//        float angZ = ParticleUtils::RandomRange(m_randomEngine, s.angularVelocityRangeZ.x, s.angularVelocityRangeZ.y);
//        p.angularVelocity = XMFLOAT4(0, 0, angZ, s.spriteFPS);
//
//        float startS, endS;
//        if (s.scaleMode == ScaleMode::Uniform) {
//            startS = s.scale.x; endS = s.scale.y;
//        }
//        else {
//            startS = ParticleUtils::RandomRange(m_randomEngine, s.scaleBeginRange.x, s.scaleBeginRange.y);
//            endS = ParticleUtils::RandomRange(m_randomEngine, s.scaleEndRange.x, s.scaleEndRange.y);
//        }
//        p.scale_begin = XMFLOAT4(startS, startS, startS, 0);
//        p.scale_end = XMFLOAT4(endS, endS, endS, 0);
//
//        int gCount = s.gradientCount;
//        if (gCount > ParticleSetting::MaxGradientKeys) gCount = ParticleSetting::MaxGradientKeys;
//        p.gradientCount = gCount;
//
//        for (int k = 0; k < ParticleSetting::MaxGradientKeys; ++k) {
//            if (k < gCount) {
//                p.gradientColors[k].time = s.gradientColors[k].time;
//                p.gradientColors[k].color = s.gradientColors[k].color;
//            }
//            else {
//                p.gradientColors[k].time = 1.0e9f;
//                p.gradientColors[k].color = XMFLOAT4(0, 0, 0, 0);
//            }
//        }
//
//        p.fade = XMFLOAT2(s.fadeInRatio, s.fadeOutRatio);
//
//        particleSystem->emit(p);
//    }
//}
//
//
//
//
//
//
//void ParticleEmitter::SyncSettingsToGradient(ImGG::Gradient& outGradient)
//{
//    std::list<ImGG::Mark> marks;
//    for (int i = 0; i < settings.gradientCount; ++i) {
//        const auto& k = settings.gradientColors[i];
//        ImGG::ColorRGBA col = { k.color.x, k.color.y, k.color.z, k.color.w };
//        marks.push_back(ImGG::Mark{ ImGG::RelativePosition{ k.time }, col });
//    }
//    if (marks.empty()) {
//        marks.push_back(ImGG::Mark{ ImGG::RelativePosition{0.0f}, ImGG::ColorRGBA{1,1,1,1} });
//        marks.push_back(ImGG::Mark{ ImGG::RelativePosition{1.0f}, ImGG::ColorRGBA{1,1,1,1} });
//    }
//    outGradient = ImGG::Gradient(marks);
//}
//
//void ParticleEmitter::SyncGradientToSettings(const ImGG::Gradient& inGradient)
//{
//    const auto& marks = inGradient.get_marks();
//    int idx = 0;
//    for (const auto& m : marks) {
//        if (idx >= ParticleSetting::MaxGradientKeys) break;
//        settings.gradientColors[idx].time = m.position.get();
//        settings.gradientColors[idx].color = XMFLOAT4(m.color.x, m.color.y, m.color.z, m.color.w);
//        idx++;
//    }
//    settings.gradientCount = idx;
//}
//
//void ParticleEmitter::Reset()
//{
//    RenderContext rc = {};
//    rc.commandList->GetNativeContext() = Graphics::Instance().GetDeviceContext();
//
//    if (particleSystem) particleSystem->Clear(rc);
//
//    m_accumulatedTime = 0.0f;
//    m_spawnAccumulator = 0.0f;
//    m_burstFired = false;
//
//    m_randomEngine.seed(m_seed);
//
//    EffectNode::Reset();
//}
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleUtils.h"
#include "Graphics.h"
#include "GpuResourceUtils.h"
#include "RenderContext/RenderContext.h"
#include "imgui_gradient/imgui_gradient.hpp"
#include "Model/Model.h"
#include "Effect/MeshEmitter.h"
#include "Effect/EffectManager.h"
#include "RHI/ICommandList.h"

using namespace DirectX;

// =========================================================
// =========================================================
ParticleEmitter::ParticleEmitter()
{
    name = "Particle Emitter";
    type = EffectNodeType::Particle;

    settings.count = 10;
    settings.spawnRate = 10.0f;
    settings.lifeSeconds = 2.0f;
    settings.shape = ShapeType::Sphere;
    settings.radius = 0.5f;

    ID3D11Device* device = Graphics::Instance().GetDevice();
    particleSystem = std::make_shared<compute_particle_system>(
        device,
        1024,
        nullptr,
        settings.textureSplitCount
    );

    LoadTexture("Data/Effect/particle/particle.png");

    m_curlNoiseSRV = EffectManager::Get().GetCommonCurlNoise();

    std::random_device rd;
    m_seed = rd();
    m_randomEngine.seed(rd());
}

ParticleEmitter::~ParticleEmitter()
{
    particleSystem.reset();
}

// =========================================================
// =========================================================
void ParticleEmitter::LoadTexture(const std::string& path)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    if (!device) return;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_TEXTURE2D_DESC desc{};
    HRESULT hr = GpuResourceUtils::LoadTexture(
        device,
        path.c_str(),
        srv.ReleaseAndGetAddressOf(),
        &desc
    );

    if (SUCCEEDED(hr) && srv)
    {
        textureSRV = srv;
        texturePath = path;
        if (particleSystem) particleSystem->ChangeTexture(textureSRV);
    }
}

// =========================================================
// =========================================================
Model* ParticleEmitter::GetParentModel() const
{
    if (parent)
    {
        auto meshNode = dynamic_cast<MeshEmitter*>(parent);
        if (meshNode && meshNode->GetModel())
        {
            return meshNode->GetModel().get();
        }
    }
    return nullptr;
}

// =========================================================
// =========================================================
void ParticleEmitter::LoadMeshShader()
{
    if (m_meshVS) return;

    auto device = Graphics::Instance().GetDevice();
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };


 
    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/Shader/ParticleMeshVS.cso",
        inputElementDesc,
        _countof(inputElementDesc),
        m_meshInputLayout.GetAddressOf(),
        m_meshVS.GetAddressOf()
    );
}

// =========================================================
// =========================================================
void ParticleEmitter::UpdateWithAge(float age, float lifeTime)
{
    EffectNode::UpdateWithAge(age, lifeTime);
    m_currentRootAge = age;
    m_rootLifeTime = lifeTime;
}

void ParticleEmitter::Update(float dt)
{
    EffectNode::Update(dt);


     if (settings.renderMode == RenderMode::Mesh)
    {
        Model* model = GetParentModel();
        if (model && !model->GetMeshes().empty())
        {
            UINT indexCount = static_cast<UINT>(model->GetMeshes()[0].indices.size());
            particleSystem->SetIndirectDrawIndexCount(Graphics::Instance().GetDeviceContext(), indexCount);
            LoadMeshShader();
        }
    }



    if (!particleSystem) return;
    if (dt <= 0.0f) return;

    RenderContext rc = {};
    //rc.commandList->GetNativeContext() = Graphics::Instance().GetDeviceContext();
    rc.renderState = Graphics::Instance().GetRenderState();
    //rc.camera = Graphics::Instance().GetCamera();
    //rc.lightManager = Graphics::Instance().GetLightManager();

    //if (!rc.camera) return;

    particleSystem->SetTextureSplitCount(settings.textureSplitCount);
    particleSystem->SetCurlNoiseTexture(m_curlNoiseSRV);
    particleSystem->SetCurlNoiseStrength(renderSettings.curlNoiseStrength);
    particleSystem->SetCurlNoiseScale(renderSettings.curlNoiseScale);
    particleSystem->SetCurlMoveSpeed(renderSettings.curlMoveSpeed);
    particleSystem->SetVelocityStretchEnabled(renderSettings.velocityStretchEnabled);
    particleSystem->SetVelocityStretchScale(renderSettings.velocityStretchScale);
    particleSystem->SetVelocityStretchMaxAspect(renderSettings.velocityStretchMaxAspect);
    particleSystem->SetVelocityStretchMinSpeed(renderSettings.velocityStretchMinSpeed);

    particleSystem->Begin(rc);
    particleSystem->Update(rc, dt);
    particleSystem->End(rc);

    if (m_currentRootAge >= m_rootLifeTime) return;

    m_accumulatedTime += dt;

    if (settings.burst && !m_burstFired)
    {
        int burstCount = settings.count * settings.burstFactor;
        if (burstCount > 0) Emit(burstCount);
        m_burstFired = true;
    }

    if (settings.spawnRate > 0.0f)
    {
        m_spawnAccumulator += dt;
        float interval = 1.0f / settings.spawnRate;
        int times = static_cast<int>(m_spawnAccumulator / interval);
        if (times > 0)
        {
            m_spawnAccumulator -= interval * times;
            Emit(settings.count * times);
        }
    }
    else
    {
        Emit(settings.count);
    }

    if (settings.loop)
    {
        if (m_accumulatedTime >= settings.playSeconds)
        {
            m_accumulatedTime = 0.0f;
            m_burstFired = false;
            m_spawnAccumulator = 0.0f;
        }
    }

}

// =========================================================
// =========================================================
void ParticleEmitter::Render(RenderContext& rc)
{
    if (!particleSystem) return;

    if (settings.renderMode == RenderMode::Mesh)
    {
        // =========================================================
        // =========================================================
        Model* model = GetParentModel();
        if (!model || !m_meshVS) return;

        ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
        auto standardShader = EffectManager::Get().GetStandardShader();
        standardShader->Begin(rc);

        dc->VSSetShader(m_meshVS.Get(), nullptr, 0);
        dc->IASetInputLayout(m_meshInputLayout.Get());

        if (auto meshParent = dynamic_cast<MeshEmitter*>(parent))
        {
            if (meshParent->material)
            {
                if (meshParent->pixelShaderVariant)
                    dc->PSSetShader(meshParent->pixelShaderVariant.Get(), nullptr, 0);

                meshParent->material->Apply(rc);
                particleSystem->SetGlobalAlpha(meshParent->material->GetConstants().visibility);

                // =========================================================
                // =========================================================
                auto blendMode = meshParent->material->GetBlendMode();
                auto renderState = Graphics::Instance().GetRenderState();

                if (blendMode == EffectBlendMode::Opaque)
                {
                    //dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
                }
                else
                {
                    //dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestOnly), 0);
                }
                // =========================================================
            }
        }



        ID3D11Buffer* renderCB = particleSystem->GetRenderConstantBuffer();
        if (renderCB) dc->VSSetConstantBuffers(2, 1, &renderCB);

        particleSystem->BindParticleDataToVS(rc, 0);
        //model->BindBuffers(dc);

        dc->DrawIndexedInstancedIndirect(particleSystem->GetIndirectBuffer(), 0);






        ID3D11ShaderResourceView* nullSRV = nullptr;
        dc->VSSetShaderResources(0, 1, &nullSRV);
        ID3D11Buffer* nullCB = nullptr;
        dc->VSSetConstantBuffers(2, 1, &nullCB);
        standardShader->End(rc);
    }
    else
    {
        // =========================================================
        // =========================================================
        particleSystem->SetGlobalAlpha(m_masterAlpha);
        particleSystem->Begin(rc);
        particleSystem->Draw(rc);
        particleSystem->End(rc);
    }
}

// =========================================================
// =========================================================
bool ParticleEmitter::SampleMeshSurface(const Model* model, DirectX::XMFLOAT3& outPos, DirectX::XMFLOAT3& outNormal)
{
    if (!model || model->GetMeshes().empty()) return false;
    const auto& mesh = model->GetMeshes()[0];
    if (mesh.indices.empty()) return false;

    std::uniform_int_distribution<size_t> triDist(0, (mesh.indices.size() / 3) - 1);
    size_t triIdx = triDist(m_randomEngine) * 3;

    uint32_t i0 = mesh.indices[triIdx];
    uint32_t i1 = mesh.indices[triIdx + 1];
    uint32_t i2 = mesh.indices[triIdx + 2];

    const auto& v0 = mesh.vertices[i0];
    const auto& v1 = mesh.vertices[i1];
    const auto& v2 = mesh.vertices[i2];

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r1 = dist(m_randomEngine);
    float r2 = dist(m_randomEngine);

    if (r1 + r2 > 1.0f) { r1 = 1.0f - r1; r2 = 1.0f - r2; }
    float r3 = 1.0f - r1 - r2;

    XMVECTOR p0 = XMLoadFloat3(&v0.position);
    XMVECTOR p1 = XMLoadFloat3(&v1.position);
    XMVECTOR p2 = XMLoadFloat3(&v2.position);
    XMVECTOR pos = r1 * p0 + r2 * p1 + r3 * p2;

    XMVECTOR n0 = XMLoadFloat3(&v0.normal);
    XMVECTOR n1 = XMLoadFloat3(&v1.normal);
    XMVECTOR n2 = XMLoadFloat3(&v2.normal);
    XMVECTOR norm = XMVector3Normalize(r1 * n0 + r2 * n1 + r3 * n2);

    XMStoreFloat3(&outPos, pos);
    XMStoreFloat3(&outNormal, norm);

    return true;
}

// =========================================================
// =========================================================
void ParticleEmitter::Emit(int count)
{
    if (count <= 0) return;

    XMMATRIX worldMat = XMLoadFloat4x4(&worldMatrix);
    XMVECTOR worldPos, worldRot, worldScale;
    XMMatrixDecompose(&worldScale, &worldRot, &worldPos, worldMat);

    XMFLOAT3 basePos;
    XMFLOAT4 baseRot;
    XMStoreFloat3(&basePos, worldPos);
    XMStoreFloat4(&baseRot, worldRot);

    const auto& s = settings;

    Model* targetModel = nullptr;
    XMMATRIX parentWorld = XMMatrixIdentity();
    if (s.shape == ShapeType::Mesh)
    {
        targetModel = GetParentModel();
        if (parent) parentWorld = XMLoadFloat4x4(&parent->worldMatrix);
    }

    for (int i = 0; i < count; ++i)
    {
        compute_particle_system::emit_particle_data p{};

        float life = (s.lifeMode == LifeMode::Constant)
            ? s.lifeSeconds
            : ParticleUtils::RandomRange(m_randomEngine, s.lifeMin, s.lifeMax);
        if (life < 0.0f) life = 0.0f;

        p.parameter = XMFLOAT4((float)s.spriteIndex, life, (float)s.spriteFrameCount, life);
        p.rotation = baseRot;

        if (s.shape == ShapeType::Mesh && targetModel)
        {
            XMFLOAT3 localPos, localNorm;
            if (SampleMeshSurface(targetModel, localPos, localNorm))
            {
                p.position = XMFLOAT4(localPos.x, localPos.y, localPos.z, 1.0f);

                XMFLOAT3 v = ParticleUtils::ComputeVelocity(s, localNorm, baseRot, m_randomEngine);
                p.velocity = XMFLOAT4(v.x, v.y, v.z, 0.0f);

                XMFLOAT3 accel = s.acceleration;
                if (s.useGravity) {
                    XMVECTOR G = XMLoadFloat3(&s.gravityDirection);
                    G = XMVector3Normalize(G) * s.gravityPower;
                    XMFLOAT3 gVec; XMStoreFloat3(&gVec, G);
                    accel.x += gVec.x; accel.y += gVec.y; accel.z += gVec.z;
                }
                p.acceleration = XMFLOAT4(accel.x, accel.y, accel.z, 0.0f);

                float angZ = ParticleUtils::RandomRange(m_randomEngine, s.angularVelocityRangeZ.x, s.angularVelocityRangeZ.y);
                p.angularVelocity = XMFLOAT4(0, 0, angZ, s.spriteFPS);

                float startS, endS;
                if (s.scaleMode == ScaleMode::Uniform) {
                    startS = s.scale.x; endS = s.scale.y;
                }
                else {
                    startS = ParticleUtils::RandomRange(m_randomEngine, s.scaleBeginRange.x, s.scaleBeginRange.y);
                    endS = ParticleUtils::RandomRange(m_randomEngine, s.scaleEndRange.x, s.scaleEndRange.y);
                }
                p.scale_begin = XMFLOAT4(startS, startS, startS, 0);
                p.scale_end = XMFLOAT4(endS, endS, endS, 0);
                p.scale = p.scale_begin;

                // =========================================================
                // =========================================================
                if (s.renderMode == RenderMode::Mesh)
                {
                    p.gradientCount = 1;
                    p.gradientColors[0].time = 0.0f;
                    p.gradientColors[0].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                    for (int k = 1; k < ParticleSetting::MaxGradientKeys; ++k) {
                        p.gradientColors[k].time = 1.0e9f;
                        p.gradientColors[k].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                    }
                }
                else
                {
                    int gCount = s.gradientCount;
                    if (gCount > ParticleSetting::MaxGradientKeys) gCount = ParticleSetting::MaxGradientKeys;
                    p.gradientCount = gCount;
                    for (int k = 0; k < ParticleSetting::MaxGradientKeys; ++k) {
                        if (k < gCount) {
                            p.gradientColors[k].time = s.gradientColors[k].time;
                            p.gradientColors[k].color = s.gradientColors[k].color;
                        }
                        else {
                            p.gradientColors[k].time = 1.0e9f;
                            p.gradientColors[k].color = XMFLOAT4(0, 0, 0, 0);
                        }
                    }
                }
                p.fade = XMFLOAT2(s.fadeInRatio, s.fadeOutRatio);

                particleSystem->emit(p);
                continue;
            }
        }

        // =========================================================
        // =========================================================

        XMFLOAT3 shapeRefPos = ParticleUtils::SampleEmissionPosition(s, m_randomEngine);
        XMFLOAT3 meshNormal = ParticleUtils::SampleEmissionDirection(s, shapeRefPos, m_randomEngine);

        XMFLOAT3 localPosWithType = shapeRefPos;
        localPosWithType.x += s.position.x;
        localPosWithType.y += s.position.y;
        localPosWithType.z += s.position.z;

        XMVECTOR P = XMLoadFloat3(&localPosWithType);
        P = XMVector3Rotate(P, worldRot);
        P = XMVectorAdd(P, worldPos);

        XMFLOAT3 finalWorldPos;
        XMStoreFloat3(&finalWorldPos, P);

        p.position = XMFLOAT4(finalWorldPos.x, finalWorldPos.y, finalWorldPos.z, 1.0f);

        XMFLOAT3 localDir = ParticleUtils::SampleEmissionDirection(s, shapeRefPos, m_randomEngine);
        XMFLOAT3 velocity = ParticleUtils::ComputeVelocity(s, localDir, baseRot, m_randomEngine);
        p.velocity = XMFLOAT4(velocity.x, velocity.y, velocity.z, 0.0f);

        XMFLOAT3 accel = s.acceleration;
        if (s.useGravity)
        {
            XMVECTOR G = XMLoadFloat3(&s.gravityDirection);
            G = XMVector3Normalize(G) * s.gravityPower;
            XMFLOAT3 gVec;
            XMStoreFloat3(&gVec, G);
            accel.x += gVec.x; accel.y += gVec.y; accel.z += gVec.z;
        }
        p.acceleration = XMFLOAT4(accel.x, accel.y, accel.z, 0.0f);

        p.rotation = baseRot;
        float angZ = ParticleUtils::RandomRange(m_randomEngine, s.angularVelocityRangeZ.x, s.angularVelocityRangeZ.y);
        p.angularVelocity = XMFLOAT4(0, 0, angZ, s.spriteFPS);

        float startS, endS;
        if (s.scaleMode == ScaleMode::Uniform) {
            startS = s.scale.x; endS = s.scale.y;
        }
        else {
            startS = ParticleUtils::RandomRange(m_randomEngine, s.scaleBeginRange.x, s.scaleBeginRange.y);
            endS = ParticleUtils::RandomRange(m_randomEngine, s.scaleEndRange.x, s.scaleEndRange.y);
        }
        p.scale_begin = XMFLOAT4(startS, startS, startS, 0);
        p.scale_end = XMFLOAT4(endS, endS, endS, 0);
        p.scale = p.scale_begin;

        // =========================================================
        // =========================================================
        if (s.renderMode == RenderMode::Mesh)
        {
            p.gradientCount = 1;
            p.gradientColors[0].time = 0.0f;
            p.gradientColors[0].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            for (int k = 1; k < ParticleSetting::MaxGradientKeys; ++k) {
                p.gradientColors[k].time = 1.0e9f;
                p.gradientColors[k].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
        else
        {
            int gCount = s.gradientCount;
            if (gCount > ParticleSetting::MaxGradientKeys) gCount = ParticleSetting::MaxGradientKeys;
            p.gradientCount = gCount;

            for (int k = 0; k < ParticleSetting::MaxGradientKeys; ++k) {
                if (k < gCount) {
                    p.gradientColors[k].time = s.gradientColors[k].time;
                    p.gradientColors[k].color = s.gradientColors[k].color;
                }
                else {
                    p.gradientColors[k].time = 1.0e9f;
                    p.gradientColors[k].color = XMFLOAT4(0, 0, 0, 0);
                }
            }
        }

        p.fade = XMFLOAT2(s.fadeInRatio, s.fadeOutRatio);

        particleSystem->emit(p);
    }
}

// =========================================================
// =========================================================
void ParticleEmitter::SyncSettingsToGradient(ImGG::Gradient& outGradient)
{
    std::list<ImGG::Mark> marks;
    for (int i = 0; i < settings.gradientCount; ++i) {
        const auto& k = settings.gradientColors[i];
        ImGG::ColorRGBA col = { k.color.x, k.color.y, k.color.z, k.color.w };
        marks.push_back(ImGG::Mark{ ImGG::RelativePosition{ k.time }, col });
    }
    if (marks.empty()) {
        marks.push_back(ImGG::Mark{ ImGG::RelativePosition{0.0f}, ImGG::ColorRGBA{1,1,1,1} });
        marks.push_back(ImGG::Mark{ ImGG::RelativePosition{1.0f}, ImGG::ColorRGBA{1,1,1,1} });
    }
    outGradient = ImGG::Gradient(marks);
}

void ParticleEmitter::SyncGradientToSettings(const ImGG::Gradient& inGradient)
{
    const auto& marks = inGradient.get_marks();
    int idx = 0;
    for (const auto& m : marks) {
        if (idx >= ParticleSetting::MaxGradientKeys) break;
        settings.gradientColors[idx].time = m.position.get();
        settings.gradientColors[idx].color = XMFLOAT4(m.color.x, m.color.y, m.color.z, m.color.w);
        idx++;
    }
    settings.gradientCount = idx;
}

void ParticleEmitter::Reset()
{
    RenderContext rc = {};
    //rc.commandList->GetNativeContext() = Graphics::Instance().GetDeviceContext();

    if (particleSystem) particleSystem->Clear(rc);

    m_accumulatedTime = 0.0f;
    m_spawnAccumulator = 0.0f;
    m_burstFired = false;
    m_randomEngine.seed(m_seed);

    EffectNode::Reset();
}
