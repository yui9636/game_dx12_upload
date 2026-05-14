#include "System/Misc.h"
#include "GpuResourceUtils.h"
#include "Gizmos.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include "RHI/IState.h"
#include "RHI/DX12/DX12CommandList.h"

Gizmos::~Gizmos() = default;

Gizmos::Gizmos(IResourceFactory* factory)
{
	InputLayoutElement layoutElements[] = {
		{ "POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement },
	};

	vertexShader = factory->CreateShader(ShaderType::Vertex, "Data/Shader/GizmosVS.cso");
	pixelShader = factory->CreateShader(ShaderType::Pixel, "Data/Shader/GizmosPS.cso");

	InputLayoutDesc layoutDesc{ layoutElements, _countof(layoutElements) };
	inputLayout = factory->CreateInputLayout(layoutDesc, vertexShader.get());

	constantBuffer = factory->CreateBuffer(sizeof(CbMesh), BufferType::Constant);

	CreateBoxMesh(factory, 0.5f, 0.5f, 0.5f);
	CreateSphereMesh(factory, 1.0f, 32);
	CreateCylinderMesh(factory, 1.0f, 0.0f, 16);
	CreateCapsuleMesh(factory, 1.0f, 0.0f, 2);
}

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

void Gizmos::DrawSphere(const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &sphereMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius, radius);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * T);
}

void Gizmos::DrawCylinder(const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.mesh = &cylinderMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, height, radius);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * T);
}

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

void Gizmos::CreateMesh(IResourceFactory* factory, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh)
{
	uint32_t byteSize = static_cast<uint32_t>(sizeof(DirectX::XMFLOAT3) * vertices.size());
	mesh.vertexBuffer = factory->CreateBuffer(byteSize, BufferType::Vertex, vertices.data());
	mesh.vertexCount = static_cast<UINT>(vertices.size());
}

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

void Gizmos::CreateCapsuleMesh(IResourceFactory* factory, float, float, int subdivision)
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

	rc.commandList->VSSetShader(vertexShader.get());
	rc.commandList->PSSetShader(pixelShader.get());
	rc.commandList->SetInputLayout(inputLayout.get());

    const float blendFactor[4] = { 1.0f,1.0f,1.0f,1.0f };
    // Gizmo はメインシーン後に、読み取り専用の深度ターゲットで描画する。
    // ここで深度を書き込むと、デバッグ形状の表示時にオーバーレイが不安定になる。
	rc.commandList->SetBlendState(rc.renderState->GetBlendState(BlendState::Alpha), blendFactor, 0xFFFFFFFF);
	rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
	DirectX::XMMATRIX VP = V * P;

	UINT stride = sizeof(DirectX::XMFLOAT3);
	UINT offset = 0;
	rc.commandList->SetPrimitiveTopology(PrimitiveTopology::LineList);

	for (const Instance& instance : instances)
	{
		rc.commandList->SetVertexBuffer(0, instance.mesh->vertexBuffer.get(), stride, offset);

		DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&instance.worldTransform);
		DirectX::XMMATRIX WVP = W * VP;

		CbMesh cbMesh;
		DirectX::XMStoreFloat4x4(&cbMesh.worldViewProjection, WVP);
		cbMesh.color = instance.color;

        if (auto* dx12Cmd = dynamic_cast<DX12CommandList*>(rc.commandList)) {
            dx12Cmd->VSSetDynamicConstantBuffer(0, &cbMesh, sizeof(cbMesh));
        } else {
            rc.commandList->VSSetConstantBuffer(0, constantBuffer.get());
            rc.commandList->UpdateBuffer(constantBuffer.get(), &cbMesh, sizeof(cbMesh));
        }


        rc.commandList->Draw(instance.mesh->vertexCount, 0);
	}
	instances.clear();
}
