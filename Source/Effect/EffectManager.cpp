#include "EffectManager.h"
#include "EffectNode.h"
#include "MeshEmitter.h"        
#include "ShaderClass/EffectVariantShader.h" 
#include "Graphics.h"
#include "GpuResourceUtils.h"
#include "Model/Model.h"
#include "ShaderClass/ShaderCompiler.h"
#include <filesystem>
#include "EffectLoader.h"
#include <Particle\ParticleEmitter.h>
#include "Noise/CurlNoiseGenerator.h"
#include "System/ResourceManager.h"
#include "RHI/DX11/DX11Texture.h"

void EffectManager::Initialize(ID3D11Device* _device)
{
    device = _device;

    standardShader = std::make_shared<EffectVariantShader>(device);

    shaderCompiler = std::make_unique<ShaderCompiler>();

    CurlNoiseGenerator::Config config;
    config.width = 32; config.height = 32; config.depth = 32;
    CurlNoiseGenerator::CreateCurlNoiseTexture(device, config, commonCurlNoiseSRV);

}

void EffectManager::Finalize()
{
    StopAll();
    psCache.clear();
    standardShader.reset();
    commonCurlNoiseSRV.Reset();
}

std::shared_ptr<EffectInstance> EffectManager::Play(const std::string& effectName, const DirectX::XMFLOAT3& position)
{
    std::shared_ptr<EffectNode> root = nullptr;

    float outLifeTime = 2.0f;
    float outFadeIn = 0.0f;
    float outFadeOut = 0.0f;
    bool outLoop = false;

    if (effectName == "New" || effectName.empty())
    {
        root = std::make_shared<EffectNode>();
        root->name = "Root";
        outLoop = true;
    }
    else
    {
        root = EffectLoader::LoadEffect(effectName, &outLifeTime, &outFadeIn, &outFadeOut, &outLoop);
        if (!root)
        {
            root = std::make_shared<EffectNode>();
            root->name = "EmptyRoot (Load Failed)";
        }
    }

    // 初期位置
    root->localTransform.position = position;

    auto instance = std::make_shared<EffectInstance>(root);
    instance->lifeTime = outLifeTime;
    instance->fadeInTime = outFadeIn;
    instance->fadeOutTime = outFadeOut;
    instance->loop = outLoop;

    // ★重要: 初期状態では Identity を親とする（指定されたpositionに配置される）
    DirectX::XMStoreFloat4x4(&instance->parentMatrix, DirectX::XMMatrixIdentity());

    instances.push_back(instance);
    return instance;
}

void EffectManager::Update(float dt)
{
    for (auto& inst : instances)
    {
        // シーケンサー制御下の場合はここでの更新をスキップ
        if (inst->isSequencerControlled) continue;

        if (inst->prevAge < 0.0f) inst->prevAge = inst->age;
        float oldAge = inst->prevAge;

        inst->age += dt;

        if (inst->loop && inst->age >= inst->lifeTime)
        {
            // 時間を巻き戻す (単純な代入でも良いが、fmodで余りを計算すると滑らか)
            // inst->age = 0.0f; // これでもOK
            inst->age = fmodf(inst->age, inst->lifeTime);

            // ノードの状態もリセットして再シミュレーションが必要
            if (inst->rootNode) {
                inst->rootNode->Reset();
            }
        }
        // ループOFFの場合の終了判定
        else if (!inst->loop && inst->age >= inst->lifeTime)
        {
            inst->isDead = true;
        }






        bool isScrubbing = (dt == 0.0f && fabsf(inst->age - oldAge) > 0.0001f);
        bool isRewind = (inst->age < oldAge);

        

        if (inst->rootNode)
        {
            if (isScrubbing || isRewind)
            {
                inst->rootNode->Reset();
                float simTime = 0.0f;
                const float step = 1.0f / 60.0f;

                while (simTime < inst->age)
                {
                    float currentStep = step;
                    if (simTime + step > inst->age) currentStep = inst->age - simTime;
                    inst->rootNode->UpdateWithAge(simTime + currentStep, inst->lifeTime);
                    inst->rootNode->Update(currentStep);
                    simTime += currentStep;
                }
            }
            else
            {
                inst->rootNode->UpdateWithAge(inst->age, inst->lifeTime);
                inst->rootNode->Update(dt);
            }

            // ★変更: 親行列を使用して更新
            DirectX::XMMATRIX parentMat = DirectX::XMLoadFloat4x4(&inst->parentMatrix);
            inst->rootNode->UpdateTransform(parentMat);
        }

    






        float alphaIn = (inst->fadeInTime > 0.001f) ? (inst->age / inst->fadeInTime) : 1.0f;
        float alphaOut = (inst->fadeOutTime > 0.001f) ? ((inst->lifeTime - inst->age) / inst->fadeOutTime) : 1.0f;
        float currentAlpha = (alphaIn < alphaOut) ? alphaIn : alphaOut;
        if (currentAlpha < 0.0f) currentAlpha = 0.0f;
        if (currentAlpha > 1.0f) currentAlpha = 1.0f;
        inst->masterAlpha = currentAlpha;

        inst->prevAge = inst->age;
    }

    instances.remove_if([this](const std::shared_ptr<EffectInstance>& i) {
        if (i->isSequencerControlled) {
            return i->isDead; // シーケンサー用はフラグが立ったら即削除
        }
        // エディタモード中でなければ、死んでいる通常エフェクトを削除
        return !isEditorMode && i->isDead;
        });


}

void EffectManager::SyncInstanceToTime(std::shared_ptr<EffectInstance> inst, float targetTime)
{
    if (!inst || !inst->rootNode) return;

    if (targetTime < 0.0f) targetTime = 0.0f;
    if (inst->prevAge < 0.0f) inst->prevAge = targetTime;

    float oldAge = inst->age;
    inst->age = targetTime;

    if (!inst->loop && inst->age >= inst->lifeTime) {
        inst->isDead = true;
    }
    else {
        inst->isDead = false;
    }

    bool isRewind = (inst->age < oldAge);
    bool isJump = (fabsf(inst->age - oldAge) > (1.0f / 30.0f));

    if (isRewind || isJump)
    {
        inst->rootNode->Reset();
        float simTime = 0.0f;
        const float step = 1.0f / 60.0f;

        while (simTime < inst->age)
        {
            float currentStep = step;
            if (simTime + step > inst->age) currentStep = inst->age - simTime;
            inst->rootNode->UpdateWithAge(simTime + currentStep, inst->lifeTime);
            inst->rootNode->Update(currentStep);
            simTime += currentStep;
        }
    }
    else
    {
        float dt = inst->age - oldAge;
        if (dt > 0.0f)
        {
            inst->rootNode->UpdateWithAge(inst->age, inst->lifeTime);
            inst->rootNode->Update(dt);
        }
    }

    // ★重要修正: シーケンサー制御下なら、計算後にローカル変形を強制上書きする
    // これにより、Reset()やUpdateWithAge()で座標が初期化されても、
    // GUIで設定したオフセット・回転・スケールが復活します。
    if (inst->isSequencerControlled)
    {
        inst->rootNode->localTransform = inst->overrideLocalTransform;
    }

    // ★変更: parentMatrix (ボーン行列) を適用して行列確定
    DirectX::XMMATRIX parentMat = DirectX::XMLoadFloat4x4(&inst->parentMatrix);
    inst->rootNode->UpdateTransform(parentMat);

    float alphaIn = (inst->fadeInTime > 0.001f) ? (inst->age / inst->fadeInTime) : 1.0f;
    float alphaOut = (inst->fadeOutTime > 0.001f) ? ((inst->lifeTime - inst->age) / inst->fadeOutTime) : 1.0f;
    float currentAlpha = (alphaIn < alphaOut) ? alphaIn : alphaOut;
    if (currentAlpha < 0.0f) currentAlpha = 0.0f;
    if (currentAlpha > 1.0f) currentAlpha = 1.0f;
    inst->masterAlpha = currentAlpha;

    inst->prevAge = inst->age;
}

void EffectManager::Render( RenderContext& rc)
{
    // 再帰描画用のヘルパーラムダ
    // ★修正: 引数に currentAlpha を追加
    std::function<void(const std::shared_ptr<EffectNode>&, float, float)> recursiveRender =
        [&](const std::shared_ptr<EffectNode>& node, float currentAge, float currentAlpha)
        {
            // MeshEmitter なら描画
            if (auto meshNode = std::dynamic_pointer_cast<MeshEmitter>(node))
            {
                if (meshNode->material) {
                    auto& c = meshNode->material->GetConstants();

                    // 時間をセット
                    c.currentTime = currentAge;

                    // ★追加: 自動フェードの適用
                    // 現在のマテリアル設定(Inspector等で設定した値)をバックアップ
                    float backupVisibility = c.visibility;

                    // システム計算値を乗算 (Base * Alpha)
                    c.visibility *= currentAlpha;

                    // 描画実行 (Apply -> Draw)
                    meshNode->Render(rc);

                    // ★重要: 設定を汚染しないように元に戻す
                    c.visibility = backupVisibility;
                }
                else {
                    meshNode->Render(rc);
                }
            }

            else if (auto particleNode = std::dynamic_pointer_cast<ParticleEmitter>(node))
            {
                // パーティクルにも自動フェードを適用したければ、ここで settings を一時的にいじる等の処理が必要ですが、
                // まずは表示させることを優先して Render を呼びます。
                particleNode->SetMasterAlpha(currentAlpha);
                particleNode->Render(rc);
            }

            // 子ノードへ
            for (auto& child : node->children) {
                recursiveRender(child, currentAge, currentAlpha);
            }
        };

    // 全インスタンスを描画
    for (auto& inst : instances)
    {
        if (inst->rootNode) {
            // ★修正: インスタンスごとの masterAlpha を渡す
            recursiveRender(inst->rootNode, inst->age, inst->masterAlpha);
        }
    }
}

void EffectManager::StopAll()
{
    instances.clear();
}

// --------------------------------------------------------
// リソース管理
// --------------------------------------------------------
std::shared_ptr<Model> EffectManager::GetModel(const std::string& path)
{
    return ResourceManager::Instance().GetModel(path);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> EffectManager::GetTexture(const std::string& path)
{
    // ★ 修正：ITexture として取得し、DX11Texture にダウンキャストして生SRVを返す
    auto tex = ResourceManager::Instance().GetTexture(path);
    return tex ? static_cast<DX11Texture*>(tex.get())->GetNativeSRV() : nullptr;
}

std::shared_ptr<EffectVariantShader> EffectManager::GetStandardShader() const
{
    return standardShader;
}

// --------------------------------------------------------
// ★追加: シェーダーバリアント取得の実装
// --------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11PixelShader> EffectManager::GetPixelShaderVariant(int flags)
{
    // 1. キャッシュにあればそれを返す (高速)
    auto it = psCache.find(flags);
    if (it != psCache.end())
    {
        return it->second;
    }

    // 2. なければ新規コンパイル (低速だが初回のみ)
    Microsoft::WRL::ComPtr<ID3D11PixelShader> newPS;

    // HLSLのパスはプロジェクト構成に合わせて調整してください
    // ※実行時にコンパイルするため、hlslファイル自体が実行ファイルから見える場所(Data/Shader/等)にある必要があります
    HRESULT hr = ShaderCompiler::CompilePixelShader(
        device,
        L"Shader/EffectPS.hlsl",
        flags,
        newPS.GetAddressOf()
    );

    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to compile EffectPS variant!\n");
        // 失敗時はnullptrを返すが、フォールバックとしてデフォルトシェーダーを返しても良い
        return nullptr;
    }

    // 3. キャッシュに登録して返す
    psCache[flags] = newPS;
    return newPS;
}

void EffectInstance::Stop(bool immediate)
{
    if (immediate) {
        isDead = true;
    }
    else {
        loop = false;
    }
}
