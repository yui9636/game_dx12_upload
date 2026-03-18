#include "PBRShader.h"

#include "Graphics.h"

#include "ShadowMap.h"

#include "System/ResourceManager.h"



// RHI �֘A

#include "RHI/IResourceFactory.h"

#include "RHI/ICommandList.h"

#include "RHI/ITexture.h"

#include "RHI/IBuffer.h"

#include "RHI/PipelineStateDesc.h"

#include "RHI/IPipelineState.h"



PBRShader::PBRShader(IResourceFactory* factory)

{

	// 1. �V�F�[�_�[�̃��[�h

	m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PBRVS.cso");

	m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/PBRPS.cso");



	// 2. ���̓��C�A�E�g��`

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



	// 3. �萔�o�b�t�@����

	m_meshConstantBuffer = factory->CreateBuffer(sizeof(CbMesh), BufferType::Constant);



	// 4. �f�t�H���g IBL �e�N�X�`���ǂݍ��� (ITexture��)

	auto& rm = ResourceManager::Instance();

	m_diffuseIem = rm.GetTexture("Data/Texture/IBL/diffuse_iem.dds");

	m_specularPmrem = rm.GetTexture("Data/Texture/IBL/specular_pmrem.dds");

	m_lutGgx = rm.GetTexture("Data/Texture/IBL/lut_ggx.dds");



	// 5. PSO �̐���

	PipelineStateDesc desc{};

	desc.vertexShader = m_vs.get();

	desc.pixelShader = m_ps.get();

	desc.inputLayout = m_inputLayout.get();

	desc.primitiveTopology = PrimitiveTopology::TriangleList;



	// �V�[���`��p�t�H�[�}�b�g (R16G16B16A16_FLOAT) �ɍ��킹��

	desc.numRenderTargets = 1;

	desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;

	desc.dsvFormat = TextureFormat::D32_FLOAT;



	m_pso = factory->CreatePipelineState(desc);

}



void PBRShader::Begin(const RenderContext& rc)

{

	// PSO �o�C���h

	rc.commandList->SetPipelineState(m_pso.get());



	// �萔�o�b�t�@�ݒ� (�X���b�g1)

	rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());

}



void PBRShader::SetMaterialProperties(const DirectX::XMFLOAT4& baseColor, float metallic, float roughness, float emissive)

{

	m_matColor = baseColor;

	m_matMetallic = metallic;

	m_matRoughness = roughness;

	m_matEmissive = emissive;

}



void PBRShader::Update(const RenderContext& rc, const ModelResource::MeshResource& mesh)

{

	// 1. �萔�o�b�t�@�X�V

	CbMesh cbMesh{};

	const auto meshColor = mesh.material.color;

	cbMesh.materialColor = {

		meshColor.x * m_matColor.x,

		meshColor.y * m_matColor.y,

		meshColor.z * m_matColor.z,

		meshColor.w * m_matColor.w

	};

	cbMesh.metallicFactor = mesh.material.metallicFactor * m_matMetallic;

	cbMesh.roughnessFactor = mesh.material.roughnessFactor * m_matRoughness;

	cbMesh.emissiveFactor = m_matEmissive;

	cbMesh.occlusionStrength = mesh.material.occlusionStrength;



	rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));



	// 2. �V���h�E�}�b�v�擾

	ITexture* shadowTex = rc.shadowMap ? rc.shadowMap->GetTexture() : nullptr;



	// 3. ??????????????? (t0 - t5)

	ITexture* materialTextures[] = {

		mesh.material.albedoMap.get(),

		mesh.material.normalMap.get(),

		mesh.material.metallicMap.get(),

		mesh.material.roughnessMap.get(),

		mesh.material.occlusionMap.get(),

		shadowTex

	};

	rc.commandList->PSSetTextures(0, _countof(materialTextures), materialTextures);



	// 4. IBL �e�N�X�`���̃o�C���h (t33 - t35)

	rc.commandList->PSSetTexture(33, m_diffuseIem.get());

	rc.commandList->PSSetTexture(34, m_specularPmrem.get());

	rc.commandList->PSSetTexture(35, m_lutGgx.get());

}



void PBRShader::End(const RenderContext& rc)

{

	// �e�N�X�`���ݒ���� (��v�X���b�g)

	ITexture* nullTextures[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);



	// IBL ����

	rc.commandList->PSSetTexture(33, nullptr);

	rc.commandList->PSSetTexture(34, nullptr);

	rc.commandList->PSSetTexture(35, nullptr);

}



void PBRShader::SetIBLTextures(ITexture* pDiffuseIEM, ITexture* pSpecularPMREM)

{

	// ITexture �|�C���^��ێ��i�K�v�Ȃ狤�L���p�����邽�߂� shared_ptr �����邱�Ƃ������j

	// �����ł͒P���ȃ|�C���^�R�s�[�ł͂Ȃ��A���\�[�X�̎����ɍ��킹�ēK�؂ɊǗ�

	if (pDiffuseIEM) m_diffuseIem = std::shared_ptr<ITexture>(pDiffuseIEM, [](ITexture*) {}); // �O���Ǘ��p

	if (pSpecularPMREM) m_specularPmrem = std::shared_ptr<ITexture>(pSpecularPMREM, [](ITexture*) {});

}

