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
//	void Begin(const RenderContext& rc) override;
//
//	void Update(float dt);
//
//	void Draw(const RenderContext& rc, const Model* model) override;
//
//	void DrawSnapshot(const RenderContext& rc, const Model* model, const std::vector<DirectX::XMFLOAT4X4>& nodeTransforms);
//
//	void End(const RenderContext& rc) override;
//
//	void OnGUI();
//
//	void SetNoiseTexture(ID3D11ShaderResourceView* texNoise)
//	{
//		noiseTexture = texNoise;
//	}
//
//	struct CbHologram
//	{
//
//
//		float             time;
//	};
//
//	CbHologram& GetParameters() { return cbHologram; }
//
//private:
//	struct CbScene
//	{
//		DirectX::XMFLOAT4X4	viewProjection;
//		DirectX::XMFLOAT4	lightColor;
//		DirectX::XMFLOAT4	cameraPosition;
//		DirectX::XMFLOAT4X4	lightViewProjection;
//	};
//
//	struct CbSkeleton
//	{
//		DirectX::XMFLOAT4X4	boneTransforms[256];
//	};
//
//	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
//	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
//	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
//
//	Microsoft::WRL::ComPtr<ID3D11Buffer>		sceneConstantBuffer;    // b0
//	Microsoft::WRL::ComPtr<ID3D11Buffer>		skeletonConstantBuffer; // b6
//
//	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
//
//	CbHologram cbHologram{};
//	float      totalTime = 0.0f;
//};
