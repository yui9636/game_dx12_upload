//#include "HologramShader.h"
//#include "GpuResourceUtils.h"
//#include <imgui.h> 
//#include "Model.h"
//#include "ShadowMap.h"
//#include "RHI/ICommandList.h"
//
//HologramShader::HologramShader(ID3D11Device* device)
//{
//	D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
//	{
//		{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//	};
//
//	GpuResourceUtils::LoadVertexShader(
//		device,
//		"Data/Shader/HologramVS.cso",
//		inputElementDesc,
//		_countof(inputElementDesc),
//		inputLayout.GetAddressOf(),
//		vertexShader.GetAddressOf());
//
//	GpuResourceUtils::LoadPixelShader(
//		device,
//		"Data/Shader/HologramPS.cso",
//		pixelShader.GetAddressOf());
//
//	GpuResourceUtils::CreateConstantBuffer(
//		device,
//		sizeof(CbScene),
//		sceneConstantBuffer.GetAddressOf());
//
//	GpuResourceUtils::CreateConstantBuffer(
//		device,
//		sizeof(CbHologram),
//		hologramConstantBuffer.GetAddressOf());
//
//	GpuResourceUtils::CreateConstantBuffer(
//		device,
//		sizeof(CbSkeleton),
//		skeletonConstantBuffer.GetAddressOf());
//
//	cbHologram.baseColor = { 0.0f, 0.5f, 1.0f, 0.5f };
//	cbHologram.rimColor = { 10.8f, 0.9f, 1.0f, 1.0f };
//	cbHologram.fresnelPower = 3.0f;
//	cbHologram.scanlineFreq = 50.0f;
//	cbHologram.scanlineSpeed = 5.0f;
//	cbHologram.glitchIntensity = 0.0f;
//	cbHologram.time = 0.0f;
//}
//
//void HologramShader::Update(float dt)
//{
//	totalTime += dt;
//	cbHologram.time = totalTime;
//}
//
//void HologramShader::Begin(const RenderContext& rc)
//{
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	dc->IASetInputLayout(inputLayout.Get());
//	rc.commandList->GetNativeContext()->VSSetShader(vertexShader.Get(), nullptr, 0);
//	rc.commandList->GetNativeContext()->PSSetShader(pixelShader.Get(), nullptr, 0);
//
//	ID3D11Buffer* constantBuffers[] =
//	{
//		sceneConstantBuffer.Get(),      // b0: Scene
//	};
//	rc.commandList->GetNativeContext()->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
//	rc.commandList->GetNativeContext()->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
//
//
//	ID3D11SamplerState* samplerStates[] =
//	{
//		rc.renderState->GetSamplerState(SamplerState::LinearWrap),
//	};
//	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);
//
//	const float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Additive), blend_factor, 0xFFFFFFFF);
//	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
//	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
//
//	CbScene cbScene{};
//	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
//	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
//	DirectX::XMStoreFloat4x4(&cbScene.viewProjection, V * P);
//	const DirectionalLight& directionalLight = rc.directionalLight;
//	cbScene.lightDirection.x = directionalLight.direction.x;
//	cbScene.lightDirection.y = directionalLight.direction.y;
//	cbScene.lightDirection.z = directionalLight.direction.z;
//	cbScene.lightColor.x = directionalLight.color.x;
//	cbScene.lightColor.y = directionalLight.color.y;
//	cbScene.lightColor.z = directionalLight.color.z;
//	cbScene.cameraPosition = { rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z, 1.0f };
//	cbScene.lightViewProjection = rc.shadowMap->GetLightViewProjection(0);
//
//	dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);
//
//	dc->UpdateSubresource(hologramConstantBuffer.Get(), 0, 0, &cbHologram, 0, 0);
//}
//
//void HologramShader::Draw(const RenderContext& rc, const Model* model)
//{
//	if (model == nullptr) return;
//
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	for (const Model::Mesh& mesh : model->GetMeshes())
//	{
//		UINT stride = sizeof(Model::Vertex);
//		UINT offset = 0;
//		dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
//		dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
//		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//		CbSkeleton cbSkeleton{};
//		if (mesh.bones.size() > 0)
//		{
//			for (size_t i = 0; i < mesh.bones.size(); ++i)
//			{
//				const Model::Bone& bone = mesh.bones.at(i);
//				DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
//				DirectX::XMMATRIX OffsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
//				DirectX::XMMATRIX BoneTransform = OffsetTransform * WorldTransform;
//				DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], BoneTransform);
//			}
//		}
//		else
//		{
//			cbSkeleton.boneTransforms[0] = mesh.node->worldTransform;
//		}
//		dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);
//
//		if (noiseTexture)
//		{
//			ID3D11ShaderResourceView* srvs[] = { noiseTexture.Get() };
//			dc->PSSetShaderResources(0, 1, srvs);
//		}
//
//		dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
//	}
//}
//
//
//void HologramShader::DrawSnapshot(
//	const RenderContext& rc,
//	const Model* model,
//	const std::vector<DirectX::XMFLOAT4X4>& nodeTransforms
//)
//{
//	if (!model) return;
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	dc->UpdateSubresource(hologramConstantBuffer.Get(), 0, 0, &cbHologram, 0, 0);
//
//	for (const Model::Mesh& mesh : model->GetMeshes())
//	{
//		UINT stride = sizeof(Model::Vertex);
//		UINT offset = 0;
//		dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
//		dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
//		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//		CbSkeleton cbSkeleton{};
//
//		if (mesh.bones.size() > 0)
//		{
//			for (size_t i = 0; i < mesh.bones.size(); ++i)
//			{
//				const Model::Bone& bone = mesh.bones.at(i);
//
//				if (bone.nodeIndex < (int)nodeTransforms.size())
//				{
//					DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&nodeTransforms[bone.nodeIndex]);
//					DirectX::XMMATRIX OffsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
//					DirectX::XMMATRIX BoneTransform = OffsetTransform * WorldTransform;
//					DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], BoneTransform);
//				}
//			}
//		}
//		else
//		{
//			if (mesh.nodeIndex < (int)nodeTransforms.size()) {
//				cbSkeleton.boneTransforms[0] = nodeTransforms[mesh.nodeIndex];
//			}
//		}
//
//		dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);
//
//		if (noiseTexture) {
//			ID3D11ShaderResourceView* srvs[] = { noiseTexture.Get() };
//			dc->PSSetShaderResources(0, 1, srvs);
//		}
//
//		dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
//	}
//}
//
//
//
//void HologramShader::End(const RenderContext& rc)
//{
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	dc->VSSetShader(nullptr, nullptr, 0);
//	dc->PSSetShader(nullptr, nullptr, 0);
//	dc->IASetInputLayout(nullptr);
//
//	ID3D11ShaderResourceView* srvs[] = { nullptr };
//	dc->PSSetShaderResources(0, _countof(srvs), srvs);
//}
//
//
//
//
//
//void HologramShader::OnGUI()
//{
//	if (ImGui::CollapsingHeader("Hologram Shader Settings", ImGuiTreeNodeFlags_DefaultOpen))
//	{
//		ImGui::ColorEdit4("Base Color (RGB & Initial Alpha)", (float*)&cbHologram.baseColor);
//
//		ImGui::ColorEdit3("Rim Color", (float*)&cbHologram.rimColor);
//
//		ImGui::Separator();
//
//		ImGui::SliderFloat("Fresnel Power", &cbHologram.fresnelPower, 0.1f, 10.0f);
//
//		ImGui::SliderFloat("Scanline Freq", &cbHologram.scanlineFreq, 1.0f, 100.0f);
//		ImGui::SliderFloat("Scanline Speed ", &cbHologram.scanlineSpeed, 0.0f, 20.0f);
//
//		ImGui::Separator();
//
//		ImGui::SliderFloat("Glitch Intensity", &cbHologram.glitchIntensity, 0.0f, 0.5f);
//
//		ImGui::Text("Shader Time: %.2f", cbHologram.time);
//		ImGui::Text("Current C++ Alpha: %.2f", cbHologram.alpha);
//	}
//}
