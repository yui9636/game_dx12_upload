#pragma once

#include "Shader.h"

#include <memory>

#include <DirectXMath.h>




class IResourceFactory;

class IShader;

class IInputLayout;

class IBuffer;

class IPipelineState;
class ITexture;
class MaterialAsset;

class PhongShader : public Shader

{

public:

	PhongShader(IResourceFactory* factory);

	~PhongShader() override = default;




	void Begin(const RenderContext& rc) override;




	void Update(const RenderContext& rc, const ModelResource::MeshResource& mesh) override;
	void BeginInstanced(const RenderContext& rc) override;
	bool SupportsInstancing(const ModelResource::MeshResource& mesh) const override;




	void End(const RenderContext& rc) override;

	void SetMaterialAssetOverride(const MaterialAsset* material) { m_materialOverride = material; }



private:

	struct CbMesh

	{

		DirectX::XMFLOAT4 materialColor;

	};



	std::shared_ptr<ITexture>      m_whiteTexture; // 1x1 white fallback

	std::unique_ptr<IShader>       m_vs;

	std::unique_ptr<IShader>       m_ps;
	std::unique_ptr<IShader>       m_instancedVs;

	std::unique_ptr<IInputLayout>  m_inputLayout;
	std::unique_ptr<IInputLayout>  m_instancedInputLayout;

	std::unique_ptr<IBuffer>       m_meshConstantBuffer;




	std::unique_ptr<IPipelineState> m_pso;
	std::unique_ptr<IPipelineState> m_instancedPso;
	const MaterialAsset* m_materialOverride = nullptr;

};
