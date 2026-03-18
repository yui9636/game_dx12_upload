#pragma once

#include "Shader.h"
#include <memory>
#include <DirectXMath.h>

// RHI �O���錾
class IResourceFactory;
class IShader;
class IInputLayout;
class IBuffer;
class ITexture;
class IPipelineState;

class PBRShader : public Shader
{
public:
	PBRShader(IResourceFactory* factory);
	~PBRShader() override = default;

	// �J�n����
	void Begin(const RenderContext& rc) override;

	// �X�V����
	void Update(const RenderContext& rc, const Model::Mesh& mesh) override;

	// �I������
	void End(const RenderContext& rc) override;

	// IBL�̍X�V
	void SetIBLTextures(ITexture* pDiffuseIEM, ITexture* pSpecularPMREM);

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

	// RHI ���\�[�X
	std::unique_ptr<IShader>       m_vs;
	std::unique_ptr<IShader>       m_ps;
	std::unique_ptr<IInputLayout>  m_inputLayout;
	std::unique_ptr<IBuffer>       m_meshConstantBuffer;

	// �p�C�v���C���X�e�[�g
	std::unique_ptr<IPipelineState> m_pso;

	// IBL ���\�[�X (�O������X�V�����\�������邽�� shared_ptr)
	std::shared_ptr<ITexture> m_diffuseIem;
	std::shared_ptr<ITexture> m_specularPmrem;
	std::shared_ptr<ITexture> m_lutGgx;

public:
	DirectX::XMFLOAT4 m_matColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float m_matMetallic = 0.0f;
	float m_matRoughness = 1.0f;
	float m_matEmissive = 0.0f;
};