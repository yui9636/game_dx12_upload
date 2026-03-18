//#include"System/Misc.h"
//#include"GpuResourceUtils.h"
//#include "Gizmos.h"
//#include "RHI/ICommandList.h"
//
////�R���X�g���N�^
//Gizmos::Gizmos(ID3D11Device* device)
//{
//	//���̓��C�A�E�g
//	D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
//	{
//		{ "POSITION", 0,
//		DXGI_FORMAT_R32G32B32_FLOAT,   
//		0, D3D11_APPEND_ALIGNED_ELEMENT, 
//		D3D11_INPUT_PER_VERTEX_DATA, 0 },
//	};
//
//	//���_�V�F�[�_�[
//	 GpuResourceUtils::LoadVertexShader(
//		device,
//		"Data/Shader/GizmosVS.cso",
//		inputElementDesc,
//		ARRAYSIZE(inputElementDesc),
//		inputLayout.GetAddressOf(),
//		vertexShader.GetAddressOf());
//	
//	 //�s�N�Z���V�F�[�_�[
//	 GpuResourceUtils::LoadPixelShader(
//		 device,
//		 "Data/Shader/GizmosPS.cso",
//		 pixelShader.GetAddressOf());
//
//	 //�萔�o�b�t�@
//	 GpuResourceUtils::CreateConstantBuffer(
//		 device,
//		 sizeof(CbMesh),
//		 constantBuffer.GetAddressOf());
//
//	//�����b�V������
//	CreateBoxMesh(device, 0.5f, 0.5f, 0.5f);
//
//	//�����b�V������
//	CreateSphereMesh(device, 1.0f, 32);
//
//	//�~�����b�V������
//	CreateCylinderMesh(device, 1.0f, 0.0f, 16);
//
//	//�J�v�Z�����b�V������
//	CreateCapsuleMesh(device, 1.0f, 0.0f, 2);
//
//}
//
//
////���`��
//void Gizmos::DrawBox(
//	const DirectX::XMFLOAT3& position,
//	const DirectX::XMFLOAT3& angle,
//	const DirectX::XMFLOAT3& size, 
//	const DirectX::XMFLOAT4& color)
//{
//	Instance& instance = instances.emplace_back();
//	instance.mesh = &boxMesh;
//	instance.color = color;
//
//	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(size.x, size.y, size.z);
//	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
//	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
//	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * R * T);
//}
//
////���`��
//void Gizmos::DrawSphere(
//	const DirectX::XMFLOAT3& position,
//	float radius, 
//	const DirectX::XMFLOAT4& color)
//{
//	Instance& instance = instances.emplace_back();
//	instance.mesh = &sphereMesh;
//	instance.color = color;
//
//
//	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius,radius);
//	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
//	DirectX::XMStoreFloat4x4(&instance.worldTransform, S *T);
//}
//void Gizmos::DrawCylinder(
//	const DirectX::XMFLOAT3& position,
//	float radius,
//	float height,
//	const DirectX::XMFLOAT4& color)
//{
//	Instance& instance = instances.emplace_back();
//	instance.mesh = &cylinderMesh;
//	instance.color = color;
//
//	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, height, radius);
//	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
//
//
//
//	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * T);
//}
//void Gizmos::DrawCapsule(
//	const DirectX::XMFLOAT3& position,
//	const DirectX::XMFLOAT3& angle,
//	float radius,
//	float height,
//	const DirectX::XMFLOAT4& color)
//{
//	Instance& instance = instances.emplace_back();
//	instance.mesh = &capsuleMesh;
//	instance.color = color;
//
//	const float unitTotalLen = 3.0f;
//	const float targetTotalLen = height + 2.0f * radius;
//
//	using namespace DirectX;
//	XMMATRIX S = XMMatrixScaling(radius, targetTotalLen / unitTotalLen, radius); // XZ=���a�AY=�S����
//	XMMATRIX R = XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
//	XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
//	XMStoreFloat4x4(&instance.worldTransform, S * R * T);
//}
//
////���b�V������
//void Gizmos::CreateMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh)
//{
//	D3D11_BUFFER_DESC desc = {};
//	desc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT3) * vertices.size());
//	desc.Usage = D3D11_USAGE_IMMUTABLE;
//	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
//	desc.CPUAccessFlags = 0;
//	desc.MiscFlags = 0;
//	desc.StructureByteStride = 0;
//	D3D11_SUBRESOURCE_DATA subresourceData = {};
//
//	subresourceData.pSysMem = vertices.data();
//	subresourceData.SysMemPitch = 0;
//	subresourceData.SysMemSlicePitch = 0;
//
//	HRESULT hr = device->CreateBuffer(&desc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
//	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//	mesh.vertexCount = static_cast<UINT>(vertices.size());
//}
////�����b�V���쐬
//void Gizmos::CreateBoxMesh(ID3D11Device* device, float width, float height, float depth)
//{
//	DirectX::XMFLOAT3 position[8] =
//	{
//		//top
//		{-width,height,-depth},
//		{width,height,-depth},
//		{width,height,depth},
//		{-width,height,depth},
//		//bottom
//		{-width,-height,-depth},
//		{width,-height,-depth},
//		{width,-height,depth},
//		{-width,-height,depth},
//	};
//
//	std::vector<DirectX::XMFLOAT3>vertices;
//	vertices.resize(32);
//
//	//top
//	vertices.emplace_back(position[0]);
//	vertices.emplace_back(position[1]);
//	vertices.emplace_back(position[1]);
//	vertices.emplace_back(position[2]);
//	vertices.emplace_back(position[2]);
//	vertices.emplace_back(position[3]);
//	vertices.emplace_back(position[3]);
//	vertices.emplace_back(position[0]);
//	//bottom
//	vertices.emplace_back(position[4]);
//	vertices.emplace_back(position[5]);
//	vertices.emplace_back(position[5]);
//	vertices.emplace_back(position[6]);
//	vertices.emplace_back(position[6]);
//	vertices.emplace_back(position[7]);
//	vertices.emplace_back(position[7]);
//	vertices.emplace_back(position[4]);
//	//side
//	vertices.emplace_back(position[0]);
//	vertices.emplace_back(position[4]);
//	vertices.emplace_back(position[1]);
//	vertices.emplace_back(position[5]);
//	vertices.emplace_back(position[2]);
//	vertices.emplace_back(position[6]);
//	vertices.emplace_back(position[3]);
//	vertices.emplace_back(position[7]);
//
//	//���b�V������
//	CreateMesh(device, vertices, boxMesh);
//}
////�����b�V������
//void Gizmos::CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions)
//{
//	float step = DirectX::XM_2PI / subdivisions;
//
//	std::vector<DirectX::XMFLOAT3> vertices;
//
//	// XZ����
//	for (int i = 0; i < subdivisions; ++i)
//	{
//		for (int j = 0; j < 2; ++j)
//		{
//			float theta = step * ((i + j) % subdivisions);
//
//			DirectX::XMFLOAT3& p = vertices.emplace_back();
//			p.x = sinf(theta) * radius;
//			p.y = 0.0f;
//			p.z = cosf(theta) * radius;
//		}
//	}
//	// XY����
//	for (int i = 0; i < subdivisions; ++i)
//	{
//		for (int j = 0; j < 2; ++j)
//		{
//			float theta = step * ((i + j) % subdivisions);
//
//			DirectX::XMFLOAT3& p = vertices.emplace_back();
//			p.x = sinf(theta) * radius;
//			p.y = cosf(theta) * radius;
//			p.z = 0.0f;
//		}
//	}
//	// YZ����
//	for (int i = 0; i < subdivisions; ++i)
//	{
//		for (int j = 0; j < 2; ++j)
//		{
//			float theta = step * ((i + j) % subdivisions);
//
//			DirectX::XMFLOAT3& p = vertices.emplace_back();
//			p.x = 0.0f;
//			p.y = sinf(theta) * radius;
//			p.z = cosf(theta) * radius;
//		}
//	}
//
//	// ���b�V������
//	CreateMesh(device, vertices, sphereMesh);
//
//}
////�~�����b�V���쐬
//void  Gizmos::CreateCylinderMesh(ID3D11Device* device, float radius, float height, int subdivision)
//{
//	float step = DirectX::XM_2PI / subdivision;
//
//	std::vector<DirectX::XMFLOAT3> vertices;
//	//XZ����
//	for (int i = 0; i < subdivision; ++i)
//	{
//		constexpr int circleNum = 100;
//		for (int j = 0; j < circleNum; ++j)
//		{
//			float interval = 1.0f / circleNum;
//			float height = interval * j;
//
//			for (int k = 0; k < 2; ++k)
//			{
//				float theta = step * ((i + k) % subdivision);
//
//				DirectX::XMFLOAT3& p = vertices.emplace_back();
//				p.x = sinf(theta) * radius;
//				p.y = height * 2;
//				p.z = cosf(theta) * radius;
//			}
//		}
//	}
//
//	//���b�V������
//	CreateMesh(device, vertices, cylinderMesh);
//}
//
//// �J�v�Z��
//void Gizmos::CreateCapsuleMesh(ID3D11Device* device, float /*radius*/, float /*height*/, int subdivision)
//{
//	using namespace DirectX;
//
//	// �~�̊��炩���i�����O�����j�͌Œ�ō��߁F����Łu�܊p�`���v��h��
//	const int ringSides = 64;
//
//	// �c�����̐����x�i�o���{���j�Ɣ����̒i������ subdivision �ɘA��
//	const int meridians = (subdivision > 0 ? subdivision * 4 : 16);
//	const int hemiSteps = (subdivision > 0 ? subdivision + 4 : 8);
//
//	// �K�i���J�v�Z���i���a r=1�A�~���� h=1�j�� ������ DrawCapsule �̃X�P�[���ŗ^����
//	const float r = 1.0f;   // ���a
//	const float h = 1.0f;   // �~�������̒����i=�u�����v�ɑ��������l�j
//	const float yEquatorTop = +h * 0.5f;  // �㔼���̐ԓ�(Y)
//	const float yEquatorBottom = -h * 0.5f;  // �������̐ԓ�(Y)
//
//	const float dPhiMeridian = XM_2PI / (float)meridians;
//	const float dPhiRing = XM_2PI / (float)ringSides;
//
//	std::vector<XMFLOAT3> vertices;
//	vertices.reserve(
//		size_t(meridians) * size_t((2 + hemiSteps * 2) * 2)       // �o���{�����A�[�N
//		+ size_t(ringSides) * size_t(2 /*�ԓ�2�{*/ + 6 /*�Ɋ�胊���O*/));
//
//	auto addLine = [&](const XMFLOAT3& a, const XMFLOAT3& b) {
//		vertices.emplace_back(a);
//		vertices.emplace_back(b);
//		};
//
//	// �����O�iXZ���ʂ̉~�j����Ɋ��炩�ɕ`��
//	auto addSmoothRingXZ = [&](float y, float rad) {
//		XMFLOAT3 prev{ rad * cosf(0.0f), y, rad * sinf(0.0f) };
//		for (int i = 1; i <= ringSides; ++i) {
//			float phi = dPhiRing * (float)i;
//			XMFLOAT3 cur{ rad * cosf(phi), y, rad * sinf(phi) };
//			addLine(prev, cur);
//			prev = cur;
//		}
//		};
//
//	// 1) �~���[�̐ԓ������O�i�����ڂ́g�~�h����Ɉێ��j
//	addSmoothRingXZ(yEquatorTop, r);
//	addSmoothRingXZ(yEquatorBottom, r);
//
//	// 2) �o���i�c���C���j�{�����A�[�N
//	for (int i = 0; i < meridians; ++i)
//	{
//		float a = dPhiMeridian * (float)i;
//		float cx = cosf(a), sz = sinf(a);
//
//		// �~���̏c���i���ԓ�����ԓ��j
//		addLine(
//			XMFLOAT3{ r * cx, yEquatorBottom, r * sz },
//			XMFLOAT3{ r * cx, yEquatorTop,    r * sz }
//		);
//
//		// �� �������F�ԓ�(��=0)����(��=��/2)
//		{
//			XMFLOAT3 prev{ r * cx, yEquatorBottom, r * sz };
//			for (int s = 1; s <= hemiSteps; ++s) {
//				float t = (float)s / (float)hemiSteps;     // 0..1
//				float th = t * (XM_PI * 0.5f);              // 0..��/2
//				float ringR = r * cosf(th);                    // r��0
//				float y = yEquatorBottom - r * sinf(th);   // -h/2 �� -h/2 - r
//				XMFLOAT3 cur{ ringR * cx, y, ringR * sz };
//				addLine(prev, cur);
//				prev = cur;
//			}
//		}
//
//		// �� �㔼���F�ԓ�(��=0)����(��=��/2)
//		{
//			XMFLOAT3 prev{ r * cx, yEquatorTop, r * sz };
//			for (int s = 1; s <= hemiSteps; ++s) {
//				float t = (float)s / (float)hemiSteps;
//				float th = t * (XM_PI * 0.5f);
//				float ringR = r * cosf(th);
//				float y = yEquatorTop + r * sinf(th);      // +h/2 �� +h/2 + r
//				XMFLOAT3 cur{ ringR * cx, y, ringR * sz };
//				addLine(prev, cur);
//				prev = cur;
//			}
//		}
//	}
//
//	// ���b�V���o�^
//	CreateMesh(device, vertices, capsuleMesh);
//}
//
//
//
//
////�`����s
//void Gizmos::Render(const RenderContext& rc)
//{
//	//ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	auto dc = rc.commandList->GetNativeContext();
//
//	//�V�F�[�_�[�ݒ�
//	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
//	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
//	dc->IASetInputLayout(inputLayout.Get());
//
//	//�萔�o�b�t�@�ݒ�
//	dc->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
//
//	//�����_�[�X�e�[�g�ݒ�
//	const float blendFactor[4] = { 1.0f,1.0f,1.0f,1.0f };
//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);
//	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
//	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
//
//	//�r���[�v���W�F�N�V�����s��쐬
//	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
//	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
//	DirectX::XMMATRIX VP = V * P;
//
//	//�v���~�e�B�u�ݒ�
//	UINT stride = sizeof(DirectX::XMFLOAT3);
//	UINT offset = 0;
//	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
//
//	for (const Instance& instance : instances)
//	{
//		//���_�o�b�t�@�ݒ�
//		dc->IASetVertexBuffers(0, 1, instance.mesh->vertexBuffer.GetAddressOf(), &stride, &offset);
//
//		//���[���h�r���[�v���W�F�N�V�����s��쐬
//		DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&instance.worldTransform);
//		DirectX::XMMATRIX WVP = W * VP;
//
//		//�萔�o�b�t�@�X�V
//		CbMesh cbMesh;
//		DirectX::XMStoreFloat4x4(&cbMesh.worldViewProjection, WVP);
//		cbMesh.color = instance.color;
//
//		dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &cbMesh, 0, 0);
//
//		//�`��
//		dc->Draw(instance.mesh->vertexCount, 0);
//	}
//	instances.clear();
//}
//
//
//
//
#include "System/Misc.h"
#include "GpuResourceUtils.h"
#include "Gizmos.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include "RHI/IState.h"

// �� �f�X�g���N�^�̎��́iunique_ptr�̕s���S�Ȍ^�G���[����j
Gizmos::~Gizmos() = default;

//�R���X�g���N�^
Gizmos::Gizmos(IResourceFactory* factory)
{
	//���̓��C�A�E�g
	InputLayoutElement layoutElements[] = {
		{ "POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement },
	};

	//���_�V�F�[�_�[�ƃs�N�Z���V�F�[�_�[��RHI��
	vertexShader = factory->CreateShader(ShaderType::Vertex, "Data/Shader/GizmosVS.cso");
	pixelShader = factory->CreateShader(ShaderType::Pixel, "Data/Shader/GizmosPS.cso");

	//�C���v�b�g���C�A�E�g��RHI��
	InputLayoutDesc layoutDesc{ layoutElements, _countof(layoutElements) };
	inputLayout = factory->CreateInputLayout(layoutDesc, vertexShader.get());

	//�萔�o�b�t�@��RHI��
	constantBuffer = factory->CreateBuffer(sizeof(CbMesh), BufferType::Constant);

	//�e�탁�b�V������
	CreateBoxMesh(factory, 0.5f, 0.5f, 0.5f);
	CreateSphereMesh(factory, 1.0f, 32);
	CreateCylinderMesh(factory, 1.0f, 0.0f, 16);
	CreateCapsuleMesh(factory, 1.0f, 0.0f, 2);
}

//���`��
void Gizmos::DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &boxMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(size.x, size.y, size.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * R * T);
}

//���`��
void Gizmos::DrawSphere(const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &sphereMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius, radius);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * T);
}

//�~���`��
void Gizmos::DrawCylinder(const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &cylinderMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, height, radius);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * T);
}

//�J�v�Z���`��
void Gizmos::DrawCapsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, float radius, float height, const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &capsuleMesh;
	instance.color = color;

	const float unitTotalLen = 3.0f;
	const float targetTotalLen = height + 2.0f * radius;

	using namespace DirectX;
	XMMATRIX S = XMMatrixScaling(radius, targetTotalLen / unitTotalLen, radius);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
	XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
	XMStoreFloat4x4(&instance.worldTransform, S * R * T);
}

//���b�V������
void Gizmos::CreateMesh(IResourceFactory* factory, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh)
{
	uint32_t byteSize = static_cast<uint32_t>(sizeof(DirectX::XMFLOAT3) * vertices.size());
	mesh.vertexBuffer = factory->CreateBuffer(byteSize, BufferType::Vertex, vertices.data());
	mesh.vertexCount = static_cast<UINT>(vertices.size());
}

//�����b�V���쐬
void Gizmos::CreateBoxMesh(IResourceFactory* factory, float width, float height, float depth)
{
	DirectX::XMFLOAT3 position[8] =
	{
		{-width,height,-depth}, {width,height,-depth}, {width,height,depth}, {-width,height,depth},
		{-width,-height,-depth}, {width,-height,-depth}, {width,-height,depth}, {-width,-height,depth},
	};

	std::vector<DirectX::XMFLOAT3>vertices;
	vertices.reserve(32);

	vertices.emplace_back(position[0]); vertices.emplace_back(position[1]);
	vertices.emplace_back(position[1]); vertices.emplace_back(position[2]);
	vertices.emplace_back(position[2]); vertices.emplace_back(position[3]);
	vertices.emplace_back(position[3]); vertices.emplace_back(position[0]);
	vertices.emplace_back(position[4]); vertices.emplace_back(position[5]);
	vertices.emplace_back(position[5]); vertices.emplace_back(position[6]);
	vertices.emplace_back(position[6]); vertices.emplace_back(position[7]);
	vertices.emplace_back(position[7]); vertices.emplace_back(position[4]);
	vertices.emplace_back(position[0]); vertices.emplace_back(position[4]);
	vertices.emplace_back(position[1]); vertices.emplace_back(position[5]);
	vertices.emplace_back(position[2]); vertices.emplace_back(position[6]);
	vertices.emplace_back(position[3]); vertices.emplace_back(position[7]);

	CreateMesh(factory, vertices, boxMesh);
}

void Gizmos::CreateSphereMesh(IResourceFactory* factory, float radius, int subdivisions)
{
	float step = DirectX::XM_2PI / subdivisions;
	std::vector<DirectX::XMFLOAT3> vertices;

	for (int i = 0; i < subdivisions; ++i) {
		for (int j = 0; j < 2; ++j) {
			float theta = step * ((i + j) % subdivisions);
			vertices.push_back({ sinf(theta) * radius, 0.0f, cosf(theta) * radius });
		}
	}
	for (int i = 0; i < subdivisions; ++i) {
		for (int j = 0; j < 2; ++j) {
			float theta = step * ((i + j) % subdivisions);
			vertices.push_back({ sinf(theta) * radius, cosf(theta) * radius, 0.0f });
		}
	}
	for (int i = 0; i < subdivisions; ++i) {
		for (int j = 0; j < 2; ++j) {
			float theta = step * ((i + j) % subdivisions);
			vertices.push_back({ 0.0f, sinf(theta) * radius, cosf(theta) * radius });
		}
	}

	CreateMesh(factory, vertices, sphereMesh);
}

//�~�����b�V���쐬
void Gizmos::CreateCylinderMesh(IResourceFactory* factory, float radius, float height, int subdivision)
{
	float step = DirectX::XM_2PI / subdivision;
	std::vector<DirectX::XMFLOAT3> vertices;

	for (int i = 0; i < subdivision; ++i) {
		constexpr int circleNum = 100;
		for (int j = 0; j < circleNum; ++j) {
			float interval = 1.0f / circleNum;
			float h = interval * j;

			for (int k = 0; k < 2; ++k) {
				float theta = step * ((i + k) % subdivision);
				vertices.push_back({ sinf(theta) * radius, h * 2, cosf(theta) * radius });
			}
		}
	}

	CreateMesh(factory, vertices, cylinderMesh);
}

void Gizmos::CreateCapsuleMesh(IResourceFactory* factory, float /*radius*/, float /*height*/, int subdivision)
{
	using namespace DirectX;
	const int ringSides = 64;
	const int meridians = (subdivision > 0 ? subdivision * 4 : 16);
	const int hemiSteps = (subdivision > 0 ? subdivision + 4 : 8);

	const float r = 1.0f;
	const float h = 1.0f;
	const float yEquatorTop = +h * 0.5f;
	const float yEquatorBottom = -h * 0.5f;

	const float dPhiMeridian = XM_2PI / (float)meridians;
	const float dPhiRing = XM_2PI / (float)ringSides;

	std::vector<XMFLOAT3> vertices;
	vertices.reserve(size_t(meridians) * size_t((2 + hemiSteps * 2) * 2) + size_t(ringSides) * size_t(2 + 6));

	auto addLine = [&](const XMFLOAT3& a, const XMFLOAT3& b) {
		vertices.emplace_back(a);
		vertices.emplace_back(b);
		};

	auto addSmoothRingXZ = [&](float y, float rad) {
		XMFLOAT3 prev{ rad * cosf(0.0f), y, rad * sinf(0.0f) };
		for (int i = 1; i <= ringSides; ++i) {
			float phi = dPhiRing * (float)i;
			XMFLOAT3 cur{ rad * cosf(phi), y, rad * sinf(phi) };
			addLine(prev, cur);
			prev = cur;
		}
		};

	addSmoothRingXZ(yEquatorTop, r);
	addSmoothRingXZ(yEquatorBottom, r);

	for (int i = 0; i < meridians; ++i) {
		float a = dPhiMeridian * (float)i;
		float cx = cosf(a), sz = sinf(a);

		addLine(XMFLOAT3{ r * cx, yEquatorBottom, r * sz }, XMFLOAT3{ r * cx, yEquatorTop, r * sz });

		{
			XMFLOAT3 prev{ r * cx, yEquatorBottom, r * sz };
			for (int s = 1; s <= hemiSteps; ++s) {
				float t = (float)s / (float)hemiSteps;
				float th = t * (XM_PI * 0.5f);
				float ringR = r * cosf(th);
				float y = yEquatorBottom - r * sinf(th);
				XMFLOAT3 cur{ ringR * cx, y, ringR * sz };
				addLine(prev, cur);
				prev = cur;
			}
		}

		{
			XMFLOAT3 prev{ r * cx, yEquatorTop, r * sz };
			for (int s = 1; s <= hemiSteps; ++s) {
				float t = (float)s / (float)hemiSteps;
				float th = t * (XM_PI * 0.5f);
				float ringR = r * cosf(th);
				float y = yEquatorTop + r * sinf(th);
				XMFLOAT3 cur{ ringR * cx, y, ringR * sz };
				addLine(prev, cur);
				prev = cur;
			}
		}
	}

	CreateMesh(factory, vertices, capsuleMesh);
}

void Gizmos::Render(const RenderContext& rc)
{
	// UpdateSubresource は RHI の UpdateBuffer に移行済み（dc は不要）

	//�V�F�[�_�[�ݒ�
	rc.commandList->VSSetShader(vertexShader.get());
	rc.commandList->PSSetShader(pixelShader.get());
	rc.commandList->SetInputLayout(inputLayout.get());

	//�萔�o�b�t�@�ݒ�
	rc.commandList->VSSetConstantBuffer(0, constantBuffer.get());

	//�����_�[�X�e�[�g�ݒ�
	const float blendFactor[4] = { 1.0f,1.0f,1.0f,1.0f };
	rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);
	rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	//�r���[�v���W�F�N�V�����s��쐬
	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
	DirectX::XMMATRIX VP = V * P;

	//�v���~�e�B�u�ݒ�
	UINT stride = sizeof(DirectX::XMFLOAT3);
	UINT offset = 0;
	rc.commandList->SetPrimitiveTopology(PrimitiveTopology::LineList);

	for (const Instance& instance : instances)
	{
		//���_�o�b�t�@�ݒ�
		rc.commandList->SetVertexBuffer(0, instance.mesh->vertexBuffer.get(), stride, offset);

		//���[���h�r���[�v���W�F�N�V�����s��쐬
		DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&instance.worldTransform);
		DirectX::XMMATRIX WVP = W * VP;

		//�萔�o�b�t�@�X�V
		CbMesh cbMesh;
		DirectX::XMStoreFloat4x4(&cbMesh.worldViewProjection, WVP);
		cbMesh.color = instance.color;

		// �������͐����\�[�X�����o���čX�V
		//ID3D11Resource* cbRes = static_cast<ID3D11Resource*>(constantBuffer->GetNativeBuffer());
		//dc->UpdateSubresource(cbRes, 0, 0, &cbMesh, 0, 0);
		rc.commandList->UpdateBuffer(constantBuffer.get(), &cbMesh, sizeof(cbMesh));


		//�`��
		rc.commandList->Draw(instance.mesh->vertexCount, 0);
	}
	instances.clear();
}
