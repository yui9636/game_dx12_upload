//#pragma once
//
//#include "ShaderClass/Shader.h"
//
//class HologramShader : public EffectShader
//{
//public:
//	HologramShader(ID3D11Device* device);
//	~HologramShader() override = default;
//
//	// 開始処理
//	void Begin(const RenderContext& rc) override;
//
//	// 更新処理（時間経過など）
//	void Update(float dt);
//
//	// モデル描画
//	void Draw(const RenderContext& rc, const Model* model) override;
//
//	// HologramShader.h に追加
//	void DrawSnapshot(const RenderContext& rc, const Model* model, const std::vector<DirectX::XMFLOAT4X4>& nodeTransforms);
//
//	// 終了処理
//	void End(const RenderContext& rc) override;
//
//	void OnGUI();
//
//	// ノイズテクスチャ設定
//	void SetNoiseTexture(ID3D11ShaderResourceView* texNoise)
//	{
//		noiseTexture = texNoise;
//	}
//
//	// パラメータ構造体（16バイトアライメント）
//	struct CbHologram
//	{
//		DirectX::XMFLOAT4 baseColor;      // ベースカラー
//		DirectX::XMFLOAT4 rimColor;       // リム発光色
//
//		float             fresnelPower;   // フレネル強度
//		float             scanlineFreq;   // 走査線密度
//		float             scanlineSpeed;  // 走査線速度
//		float             glitchIntensity;// グリッチ強度
//
//		float             time;
//		float             alpha;          // ★これが HLSLの次のスロットに来るべき
//		float             dummy[2];       // パディング調整
//	};
//
//	// パラメータ取得（EffectManagerから操作用）
//	CbHologram& GetParameters() { return cbHologram; }
//
//private:
//	// シーン定数バッファ (b0)
//	struct CbScene
//	{
//		DirectX::XMFLOAT4X4	viewProjection;
//		DirectX::XMFLOAT4	lightDirection; // 今回は未使用だがSlashEffectShaderに合わせて定義
//		DirectX::XMFLOAT4	lightColor;
//		DirectX::XMFLOAT4	cameraPosition;
//		DirectX::XMFLOAT4X4	lightViewProjection;
//	};
//
//	// スケルトン定数バッファ (b6)
//	struct CbSkeleton
//	{
//		DirectX::XMFLOAT4X4	boneTransforms[256];
//	};
//
//	// シェーダーリソース
//	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
//	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
//	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
//
//	// 定数バッファ
//	Microsoft::WRL::ComPtr<ID3D11Buffer>		sceneConstantBuffer;    // b0
//	Microsoft::WRL::ComPtr<ID3D11Buffer>		hologramConstantBuffer; // b1 (CbMeshの代わり)
//	Microsoft::WRL::ComPtr<ID3D11Buffer>		skeletonConstantBuffer; // b6
//
//	// テクスチャ
//	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
//
//	// メンバ変数
//	CbHologram cbHologram{};
//	float      totalTime = 0.0f;
//};