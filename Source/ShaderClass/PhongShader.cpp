#include "PhongShader.h"
#include "Graphics.h"
#include "ShadowMap.h"

// RHI �֘A
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IBuffer.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"

PhongShader::PhongShader(IResourceFactory* factory)
{
	// 1. �V�F�[�_�[�̃��[�h (RHI��)
	m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PhongVS.cso");
	m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/PhongPS.cso");

	// 2. ���̓��C�A�E�g�̒�`�Ɛ��� (RHI��)
	InputLayoutElement layoutElements[] = {
		{"POSITION",     0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement},
		{"BONE_WEIGHTS", 0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement},
		{"BONE_INDICES", 0, TextureFormat::R32G32B32A32_UINT,  0, kAppendAlignedElement},
		{"TEXCOORD",     0, TextureFormat::R32G32_FLOAT,       0, kAppendAlignedElement},
		{"NORMAL",       0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement},
		{"TANGENT",      0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement},
	};
	InputLayoutDesc layoutDesc = { layoutElements, _countof(layoutElements) };
	m_inputLayout = factory->CreateInputLayout(layoutDesc, m_vs.get());

	// 3. �萔�o�b�t�@�̐��� (RHI��)
	m_meshConstantBuffer = factory->CreateBuffer(sizeof(CbMesh), BufferType::Constant);

	// 4. PSO (Pipeline State Object) �̍\�z
	PipelineStateDesc desc{};
	desc.vertexShader = m_vs.get();
	desc.pixelShader = m_ps.get();
	desc.inputLayout = m_inputLayout.get();

	// �p�C�v���C���ݒ� (ModelRenderer������n�����X�e�[�g�ɏ���)
	desc.primitiveTopology = PrimitiveTopology::TriangleList;

	// �����_�[�^�[�Q�b�g�ݒ� (Scene�o�b�t�@�̃t�H�[�}�b�g: R16G16B16A16_FLOAT)
	desc.numRenderTargets = 1;
	desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
	desc.dsvFormat = TextureFormat::D32_FLOAT;

	m_pso = factory->CreatePipelineState(desc);
}

// �J�n����
void PhongShader::Begin(const RenderContext& rc)
{
	// PSO ���ꊇ�o�C���h (Shader, InputLayout, Topology, States����x�ɐݒ�)
	rc.commandList->SetPipelineState(m_pso.get());

	// �萔�o�b�t�@�ݒ� (�X���b�g1)
	rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
}

// �X�V����
void PhongShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
	// 1. ���b�V���p�萔�o�b�t�@�X�V
	CbMesh cbMesh{};
	cbMesh.materialColor = mesh.material->color;
	rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));

	// 2. �V���h�E�}�b�v�̎擾 (ITexture�o�R)
	ITexture* shadowTex = nullptr;
	if (rc.shadowMap) {
		shadowTex = rc.shadowMap->GetTexture();
	}

	// 3. �e�N�X�`���̃o�C���h (RHI��)
	// t0: Diffuse, t1: Normal, t2: Shadow
	ITexture* srvs[] =
	{
		mesh.material->diffuseMap.get(), // �� �}�e���A������ ITexture ������Ă���O��
		mesh.material->normalMap.get(),
		shadowTex
	};
	rc.commandList->PSSetTextures(0, _countof(srvs), srvs);
}

// �`��I��
void PhongShader::End(const RenderContext& rc)
{
	// �e�N�X�`���ݒ����
	ITexture* nullTextures[] = { nullptr, nullptr, nullptr };
	rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
}