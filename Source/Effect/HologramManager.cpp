//#include "HologramManager.h"
//#include "ShaderClass/HologramShader.h" // パスは環境に合わせて調整してください
//#include "Actor/Actor.h"
//#include "Model/Model.h" 
//#include "GpuResourceUtils.h"
//#include "Graphics.h" 
//#include "easing.h" // アルファ計算用
//#include <algorithm> // std::find_if, std::min/max用
//
//// using namespace DirectX; // ★禁止のため削除済み
//
//// シングルトンインスタンス
//HologramManager& HologramManager::Instance()
//{
//    static HologramManager instance;
//    return instance;
//}
//
//HologramManager::HologramManager()
//{
//    // ★改善案3: プーリング初期化（配列のため特別な初期化は不要だが、念のため）
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
//    // 初期化時にもプールをクリア
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
//    // GpuResourceUtilsを使ってテクスチャを読み込む
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
//    // ★改善案3: プーリング: メモリ解放せず、すべて非アクティブにする
//    for (auto& instance : instances) {
//        instance.isActive = false;
//        instance.model.reset(); // 参照カウンタを減らす
//    }
//}
//
//// ★修正: Spawn (プーリング、インスタンス別パラメータ、オフセットの適用)
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
//    // 1. ★改善案3: プールから未使用のインスタンスを検索
//    HologramInstance* instance = nullptr;
//    for (auto& inst : instances) {
//        if (!inst.isActive) {
//            instance = &inst;
//            break;
//        }
//    }
//    if (!instance) return; // プールが満杯
//
//    // 2. 初期化
//    instance->isActive = true;
//    instance->timer = 0.0f;
//    instance->duration = duration;
//
//    // Actorからshared_ptr<Model>を取得し、リソースの寿命を延長する (既存コード準拠)
//    instance->model = std::shared_ptr<Model>(targetActor, srcModel);
//
//    // 3. ポーズのコピー
//    const std::vector<Model::Node>& nodes = srcModel->GetNodes();
//    instance->nodeTransforms.resize(nodes.size());
//    for (size_t i = 0; i < nodes.size(); ++i)
//    {
//        instance->nodeTransforms[i] = nodes[i].worldTransform;
//    }
//
//    // 4. ★改善案1: インスタンス別パラメータの設定
//    if (initialParams) {
//        instance->params = *initialParams;
//    }
//    else {
//        // デフォルト設定として、シェーダーの現在のGUI設定をコピーする
//        instance->params = shader->GetParameters();
//    }
//
//    // 5. ★改善案2: オフセットの設定
//    instance->offsetPosition = offset;
//}
//
//// ★修正: Update (プーリングの管理)
//void HologramManager::Update(float dt)
//{
//    // シェーダーの時間更新
//    if (shader)
//    {
//        shader->Update(dt);
//    }
//
//    // インスタンスの寿命管理 (配列を走査)
//    for (auto& instance : instances)
//    {
//        if (instance.isActive)
//        {
//            instance.timer += dt;
//
//            if (instance.timer >= instance.duration)
//            {
//                instance.isActive = false; // ★非アクティブ化してプールに戻す
//                instance.model.reset();    // 参照カウンタを減らす
//            }
//        }
//    }
//}
//
//// ★修正: Render (インスタンス別パラメータとオフセットの適用)
//void HologramManager::Render(const RenderContext& rc)
//{
//    if (!shader || GetActiveInstanceCount() == 0) return;
//
//    shader->Begin(rc);
//
//    // オフセット適用用の一時バッファ
//    std::vector<DirectX::XMFLOAT4X4> tempNodeTransforms;
//
//    for (auto& instance : instances)
//    {
//        if (!instance.isActive || !instance.model) continue;
//
//        // 1. 時間とアルファ計算 (既存ロジック + イージング)
//        float t_normalized = instance.timer / instance.duration;
//        float ease_value = Easing::easeInExpo(t_normalized);
//        float alpha = Clamp(1.0f - ease_value, 0.0f, 1.0f);
//
//        // 2. ★改善案1: シェーダーパラメータの転送と上書き
//        HologramShader::CbHologram& shaderParams = shader->GetParameters();
//        shaderParams = instance.params;      // インスタンス固有のパラメータをコピー
//        shaderParams.alpha = alpha;          // C++で計算したフェード率を適用
//        shaderParams.baseColor.w = alpha;    // HLSL互換のためベースカラーのAも更新
//
//        // 3. ★改善案2: オフセットを適用した行列を作成
//
//        // ノード数を確認し、テンポラリ配列をリサイズ
//        if (tempNodeTransforms.size() != instance.nodeTransforms.size()) {
//            tempNodeTransforms.resize(instance.nodeTransforms.size());
//        }
//
//        // 変換行列の作成 (Translation)
//        DirectX::XMMATRIX T = DirectX::XMMatrixTranslation( // ★DirectX:: を明記
//            instance.offsetPosition.x,
//            instance.offsetPosition.y,
//            instance.offsetPosition.z
//        );
//
//        // 各ノード行列にオフセットを適用
//        for (size_t j = 0; j < instance.nodeTransforms.size(); ++j)
//        {
//            DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&instance.nodeTransforms[j]); // ★DirectX:: を明記
//            DirectX::XMMATRIX OffsetedWorld = World * T; // ★DirectX:: は不要 (演算子オーバーロード)
//            DirectX::XMStoreFloat4x4(&tempNodeTransforms[j], OffsetedWorld); // ★DirectX:: を明記
//        }
//
//        // 4. 描画 (オフセット適用済みの行列を渡す)
//        shader->DrawSnapshot(rc, instance.model.get(), tempNodeTransforms);
//    }
//
//    shader->End(rc);
//}
//
//// デバッグ用
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