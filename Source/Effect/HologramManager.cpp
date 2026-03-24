//#include "HologramManager.h"
//#include "Actor/Actor.h"
//#include "Model/Model.h" 
//#include "GpuResourceUtils.h"
//#include "Graphics.h" 
//
//
//HologramManager& HologramManager::Instance()
//{
//    static HologramManager instance;
//    return instance;
//}
//
//HologramManager::HologramManager()
//{
//    for (auto& instance : instances) {
//        instance.isActive = false;
//    }
//}
//
//HologramManager::~HologramManager()
//{
//}
//
//void HologramManager::Initialize(ID3D11Device* device)
//{
//    this->device = device;
//    shader = std::make_unique<HologramShader>(device);
//    Clear();
//}
//
//void HologramManager::Finalize()
//{
//    Clear();
//    shader.reset();
//    noiseTexture.Reset();
//    device = nullptr;
//}
//
//void HologramManager::SetNoiseTexture(const std::string& filename)
//{
//    if (device == nullptr) return;
//
//    HRESULT hr = GpuResourceUtils::LoadTexture(
//        device,
//        filename.c_str(),
//        noiseTexture.GetAddressOf()
//    );
//
//    if (SUCCEEDED(hr) && shader)
//    {
//        shader->SetNoiseTexture(noiseTexture.Get());
//    }
//}
//
//void HologramManager::Clear()
//{
//    for (auto& instance : instances) {
//        instance.isActive = false;
//    }
//}
//
//void HologramManager::Spawn(
//    const std::shared_ptr<Actor>& targetActor,
//    float duration,
//    const HologramShader::CbHologram* initialParams,
//    const DirectX::XMFLOAT3& offset
//)
//{
//    if (!targetActor) return;
//
//    Model* srcModel = targetActor->GetModelRaw();
//    if (!srcModel) return;
//
//    HologramInstance* instance = nullptr;
//    for (auto& inst : instances) {
//        if (!inst.isActive) {
//            instance = &inst;
//            break;
//        }
//    }
//
//    instance->isActive = true;
//    instance->timer = 0.0f;
//    instance->duration = duration;
//
//    instance->model = std::shared_ptr<Model>(targetActor, srcModel);
//
//    const std::vector<Model::Node>& nodes = srcModel->GetNodes();
//    instance->nodeTransforms.resize(nodes.size());
//    for (size_t i = 0; i < nodes.size(); ++i)
//    {
//        instance->nodeTransforms[i] = nodes[i].worldTransform;
//    }
//
//    if (initialParams) {
//        instance->params = *initialParams;
//    }
//    else {
//        instance->params = shader->GetParameters();
//    }
//
//    instance->offsetPosition = offset;
//}
//
//void HologramManager::Update(float dt)
//{
//    if (shader)
//    {
//        shader->Update(dt);
//    }
//
//    for (auto& instance : instances)
//    {
//        if (instance.isActive)
//        {
//            instance.timer += dt;
//
//            if (instance.timer >= instance.duration)
//            {
//            }
//        }
//    }
//}
//
//void HologramManager::Render(const RenderContext& rc)
//{
//    if (!shader || GetActiveInstanceCount() == 0) return;
//
//    shader->Begin(rc);
//
//    std::vector<DirectX::XMFLOAT4X4> tempNodeTransforms;
//
//    for (auto& instance : instances)
//    {
//        if (!instance.isActive || !instance.model) continue;
//
//        float t_normalized = instance.timer / instance.duration;
//        float ease_value = Easing::easeInExpo(t_normalized);
//        float alpha = Clamp(1.0f - ease_value, 0.0f, 1.0f);
//
//        HologramShader::CbHologram& shaderParams = shader->GetParameters();
//
//
//        if (tempNodeTransforms.size() != instance.nodeTransforms.size()) {
//            tempNodeTransforms.resize(instance.nodeTransforms.size());
//        }
//
//            instance.offsetPosition.x,
//            instance.offsetPosition.y,
//            instance.offsetPosition.z
//        );
//
//        for (size_t j = 0; j < instance.nodeTransforms.size(); ++j)
//        {
//        }
//
//        shader->DrawSnapshot(rc, instance.model.get(), tempNodeTransforms);
//    }
//
//    shader->End(rc);
//}
//
//size_t HologramManager::GetActiveInstanceCount() const
//{
//    size_t count = 0;
//    for (const auto& instance : instances) {
//        if (instance.isActive) {
//            count++;
//        }
//    }
//    return count;
//}
