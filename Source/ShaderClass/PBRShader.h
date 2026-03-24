#pragma once



#include "Shader.h"

#include <memory>

#include <DirectXMath.h>




class IResourceFactory;

class IShader;

class IInputLayout;

class IBuffer;

class ITexture;

class MaterialAsset;

class IPipelineState;



class PBRShader : public Shader

{

public:

	PBRShader(IResourceFactory* factory);

	~PBRShader() override = default;




	void Begin(const RenderContext& rc) override;




	void Update(const RenderContext& rc, const ModelResource::MeshResource& mesh) override;
	void BeginInstanced(const RenderContext& rc) override;
	bool SupportsInstancing(const ModelResource::MeshResource& mesh) const override;




	void End(const RenderContext& rc) override;




	void SetIBLTextures(ITexture* pDiffuseIEM, ITexture* pSpecularPMREM);

	void SetMaterialAssetOverride(const MaterialAsset* material) { m_materialOverride = material; }



	void SetMaterialProperties(const DirectX::XMFLOAT4& baseColor, float metallic, float roughness, float emissive);



private:

	struct CbMesh

	{

		DirectX::XMFLOAT4 materialColor;

		float metallicFactor;

		float roughnessFactor;

		float emissiveFactor;

		float occlusionStrength;

	};




	std::unique_ptr<IShader>       m_vs;

	std::unique_ptr<IShader>       m_ps;
	std::unique_ptr<IShader>       m_instancedVs;

	std::unique_ptr<IInputLayout>  m_inputLayout;
	std::unique_ptr<IInputLayout>  m_instancedInputLayout;

	std::unique_ptr<IBuffer>       m_meshConstantBuffer;




	std::unique_ptr<IPipelineState> m_pso;
	std::unique_ptr<IPipelineState> m_instancedPso;




	std::shared_ptr<ITexture> m_diffuseIem;

	std::shared_ptr<ITexture> m_specularPmrem;

	std::shared_ptr<ITexture> m_lutGgx;



public:

	DirectX::XMFLOAT4 m_matColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float m_matMetallic = 0.0f;

	float m_matRoughness = 1.0f;

	float m_matEmissive = 0.0f;

protected:
	const MaterialAsset* m_materialOverride = nullptr;

};
