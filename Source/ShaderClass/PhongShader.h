#pragma once

#include "Shader.h"

#include <memory>

#include <DirectXMath.h>



// RHI �O���錾

class IResourceFactory;

class IShader;

class IInputLayout;

class IBuffer;

class IPipelineState;



class PhongShader : public Shader

{

public:

	PhongShader(IResourceFactory* factory);

	~PhongShader() override = default;



	// �J�n�����iPSO�̃o�C���h���j

	void Begin(const RenderContext& rc) override;



	// �X�V�����i���b�V�����Ƃ̃��\�[�X�X�V�j

	void Update(const RenderContext& rc, const ModelResource::MeshResource& mesh) override;
	void BeginInstanced(const RenderContext& rc) override;
	bool SupportsInstancing(const ModelResource::MeshResource& mesh) const override;



	// �I������

	void End(const RenderContext& rc) override;



private:

	struct CbMesh

	{

		DirectX::XMFLOAT4 materialColor;

	};



	// RHI ���\�[�X

	std::unique_ptr<IShader>       m_vs;

	std::unique_ptr<IShader>       m_ps;
	std::unique_ptr<IShader>       m_instancedVs;

	std::unique_ptr<IInputLayout>  m_inputLayout;
	std::unique_ptr<IInputLayout>  m_instancedInputLayout;

	std::unique_ptr<IBuffer>       m_meshConstantBuffer;



	// �p�C�v���C���X�e�[�g�I�u�W�F�N�g

	std::unique_ptr<IPipelineState> m_pso;
	std::unique_ptr<IPipelineState> m_instancedPso;

};