//#pragma once
//
//#include <memory>
//#include <vector>
//#include <array>    // ★listからarray/固定サイズvectorに変更
//#include <string>
//#include <d3d11.h>
//#include <wrl/client.h>
//#include <DirectXMath.h>
//#include "Model/Model.h"
//#include "ShaderClass/HologramShader.h" // CbHologram構造体アクセス用
//
//// 前方宣言
//class Actor;
//struct RenderContext;
//
//// ★プールのサイズを設定
//static const int MAX_HOLOGRAM_INSTANCES = 32;
//
//// 残像1つ分のインスタンス
//struct HologramInstance
//{
//    // ★listから変更: activeフラグで管理 (プーリング)
//    bool isActive = false;
//    float timer = 0.0f;
//    float duration = 0.0f;
//
//    // モデルのリソース共有（描画用）
//    std::shared_ptr<Model> model;
//
//    // ポーズ固定用のスナップショット行列（全ノード分）
//    std::vector<DirectX::XMFLOAT4X4> nodeTransforms;
//
//    // ★改善案1: インスタンス固有のシェーダーパラメータ
//    HologramShader::CbHologram params;
//
//    // ★改善案2: 残像全体のワールド位置オフセット
//    DirectX::XMFLOAT3 offsetPosition = { 0.0f, 0.0f, 0.0f };
//};
//
//class HologramManager
//{
//public:
//    // シングルトンインスタンス取得
//    static HologramManager& Instance();
//
//    HologramManager();
//    ~HologramManager();
//
//    // 初期化・終了
//    void Initialize(ID3D11Device* device);
//    void Finalize();
//
//    // 更新・描画
//    void Update(float dt);
//    void Render(const RenderContext& rc);
//
//    HologramShader* GetShaderRaw() const { return shader.get(); }
//
//    // --- 操作 API ---
//
//    // ★Spawn の引数を拡張 (インスタンス別パラメータとオフセット)
//    void Spawn(
//        const std::shared_ptr<Actor>& targetActor,
//        float duration = 0.4f, // デフォルトを短く設定
//        const HologramShader::CbHologram* initialParams = nullptr,
//        const DirectX::XMFLOAT3& offset = { 0.0f, 0.0f, 0.0f }
//    );
//
//    // 全ての残像を消去
//    void Clear();
//
//    // ノイズテクスチャの設定（ファイル名指定）
//    void SetNoiseTexture(const std::string& filename);
//
//    // デバッグ用
//    size_t GetActiveInstanceCount() const;
//
//private:
//    ID3D11Device* device = nullptr;
//    std::unique_ptr<HologramShader> shader;
//
//    // ★list から固定サイズ配列へ変更 (プーリングの核)
//    std::array<HologramInstance, MAX_HOLOGRAM_INSTANCES> instances;
//
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
//};