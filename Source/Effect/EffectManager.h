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

// 前方宣言
class EffectNode;
class EffectVariantShader;
class Model;
struct RenderContext;

class EffectInstance
{
public:
    // このエフェクトのルート（親玉）ノード
    std::shared_ptr<EffectNode> rootNode;

    // 生存フラグ（falseになったらManagerから削除される）
    bool isDead = false;

    float age = 0.0f;       // 現在の年齢 (0.0秒スタート)
    float lifeTime = 2.0f;  // 寿命 (デフォルト2秒で消滅)
    float prevAge = -1.0f; // 1フレーム前の年齢
    bool loop = false;      // ループ再生するかどうか

    // ★追加: フェード制御パラメータ
    float fadeInTime = 0.0f;   // フェードインにかかる時間 (秒)
    float fadeOutTime = 0.0f;  // フェードアウトにかかる時間 (秒)
    float masterAlpha = 1.0f;  // システムが計算した現在の不透明度 (0.0~1.0)


   
    bool isSequencerControlled = false;

    EffectTransform overrideLocalTransform;

    DirectX::XMFLOAT4X4 parentMatrix;

    void Stop(bool immediate = false);
 

    EffectInstance(std::shared_ptr<EffectNode> root) : rootNode(root)
    {
        DirectX::XMStoreFloat4x4(&parentMatrix, DirectX::XMMatrixIdentity());
        // 初期値
        overrideLocalTransform.position = { 0,0,0 };
        overrideLocalTransform.rotation = { 0,0,0 };
        overrideLocalTransform.scale = { 1,1,1 };
    }
};

// =================================================================
// エフェクト全体を統括する監督クラス
// =================================================================
class EffectManager
{
public:
    // シングルトン・アクセス
    static EffectManager& Get() { static EffectManager instance; return instance; }

    // --------------------------------------------------------
    // 1. システム制御
    // --------------------------------------------------------
    void Initialize(ID3D11Device* device);
    void Finalize();

    void Update(float dt);
    void Render( RenderContext& rc);

    // --------------------------------------------------------
    // 2. エフェクト再生
    // --------------------------------------------------------

    // エフェクトを発生させる
    std::shared_ptr<EffectInstance> Play(const std::string& effectName, const DirectX::XMFLOAT3& position);

    // 全停止
    void StopAll();

    void SyncInstanceToTime(std::shared_ptr<EffectInstance> instance, float targetTime);
    // --------------------------------------------------------
    // 3. リソース管理 (キャッシュ機能)
    // --------------------------------------------------------

    // モデルの取得
    std::shared_ptr<::Model> GetModel(const std::string& path);

    // テクスチャの取得
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture(const std::string& path);

    // ★ 戻り値の型を EffectVariantShader に修正
    std::shared_ptr<EffectVariantShader> GetStandardShader() const;

    // シェーダーバリアントの取得
    Microsoft::WRL::ComPtr<ID3D11PixelShader> GetPixelShaderVariant(int flags);


    // ★追加: エディタモード切替 (trueなら寿命が来ても削除しない)
    void SetEditorMode(bool enable) { isEditorMode = enable; }


    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetCommonCurlNoise() { return commonCurlNoiseSRV; }
private:
    // コンストラクタ隠蔽
    EffectManager() = default;
    ~EffectManager() = default;
    EffectManager(const EffectManager&) = delete;
    void operator=(const EffectManager&) = delete;

    // --------------------------------------------------------
    // メンバ変数
    // --------------------------------------------------------
    ID3D11Device* device = nullptr;

    // 実行中のエフェクトリスト
    std::list<std::shared_ptr<EffectInstance>> instances;

    // ピクセルシェーダーのバリアントキャッシュ
    std::map<int, Microsoft::WRL::ComPtr<ID3D11PixelShader>> psCache;
    std::shared_ptr<EffectVariantShader> standardShader;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> commonCurlNoiseSRV;
    bool isEditorMode = false;

    std::unique_ptr<ShaderCompiler> shaderCompiler;
};