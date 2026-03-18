//#include "HologramShader.h"
//#include "GpuResourceUtils.h"
//#include <imgui.h> 
//#include "Model.h"
//#include "ShadowMap.h"
//#include "RHI/ICommandList.h"
//
//HologramShader::HologramShader(ID3D11Device* device)
//{
//	// 入力レイアウト
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
//	// 頂点シェーダー
//	GpuResourceUtils::LoadVertexShader(
//		device,
//		"Data/Shader/HologramVS.cso",
//		inputElementDesc,
//		_countof(inputElementDesc),
//		inputLayout.GetAddressOf(),
//		vertexShader.GetAddressOf());
//
//	// ピクセルシェーダー
//	GpuResourceUtils::LoadPixelShader(
//		device,
//		"Data/Shader/HologramPS.cso",
//		pixelShader.GetAddressOf());
//
//	// シーン用定数バッファ (b0)
//	GpuResourceUtils::CreateConstantBuffer(
//		device,
//		sizeof(CbScene),
//		sceneConstantBuffer.GetAddressOf());
//
//	// ホログラム用定数バッファ (b1)
//	GpuResourceUtils::CreateConstantBuffer(
//		device,
//		sizeof(CbHologram),
//		hologramConstantBuffer.GetAddressOf());
//
//	// スケルトン用定数バッファ (b6)
//	GpuResourceUtils::CreateConstantBuffer(
//		device,
//		sizeof(CbSkeleton),
//		skeletonConstantBuffer.GetAddressOf());
//
//	// デフォルトパラメータ設定
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
//	// シェーダー設定
//	dc->IASetInputLayout(inputLayout.Get());
//	rc.commandList->GetNativeContext()->VSSetShader(vertexShader.Get(), nullptr, 0);
//	rc.commandList->GetNativeContext()->PSSetShader(pixelShader.Get(), nullptr, 0);
//
//	// 定数バッファ設定（既存の作りを徹底再現）
//	// 配列を使って一括設定し、Skeletonを b6 に配置します。
//	ID3D11Buffer* constantBuffers[] =
//	{
//		sceneConstantBuffer.Get(),      // b0: Scene
//		hologramConstantBuffer.Get(),   // b1: Hologram (Meshの代わり)
//		nullptr,                        // b2: Anim (未使用)
//		nullptr,                        // b3: Transform (未使用)
//		nullptr,                        // b4: Shadow (未使用)
//		nullptr,                        // b5: (未使用)
//		skeletonConstantBuffer.Get(),   // b6: Skeleton ★ここです
//	};
//	rc.commandList->GetNativeContext()->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
//	rc.commandList->GetNativeContext()->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
//
//
//	// サンプラーステート設定
//	ID3D11SamplerState* samplerStates[] =
//	{
//		rc.renderState->GetSamplerState(SamplerState::LinearWrap),
//	};
//	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);
//
//	// レンダーステート設定 (加算合成・Z書込なし・カリングなし)
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
//	// ホログラム用定数バッファ更新
//	// Update() で time は更新済み
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
//		// 頂点バッファ設定
//		UINT stride = sizeof(Model::Vertex);
//		UINT offset = 0;
//		dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
//		dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
//		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//		// スケルトン用定数バッファ更新
//		// EffectManager 側で事前にボーン行列を計算・書き換えている前提であれば、
//		// ここでは mesh.bones からそのまま転送します。
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
//		// 既に Begin で b6 にバインドされているので、UpdateSubresource だけで反映されます
//		dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);
//
//		// テクスチャ設定 (t0)
//		if (noiseTexture)
//		{
//			ID3D11ShaderResourceView* srvs[] = { noiseTexture.Get() };
//			dc->PSSetShaderResources(0, 1, srvs);
//		}
//
//		// 描画
//		dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
//	}
//}
//
//// HologramShader.cpp の末尾（または適切な場所）に追加
//
//// ★追加実装: スナップショット行列を使って描画する
//void HologramShader::DrawSnapshot(
//	const RenderContext& rc,
//	const Model* model,
//	const std::vector<DirectX::XMFLOAT4X4>& nodeTransforms
//)
//{
//	if (!model) return;
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	// パラメータ更新 (Alpha反映)
//	// CbHologram::alpha が更新されていることを期待
//	dc->UpdateSubresource(hologramConstantBuffer.Get(), 0, 0, &cbHologram, 0, 0);
//
//	for (const Model::Mesh& mesh : model->GetMeshes())
//	{
//		// 頂点バッファ設定
//		UINT stride = sizeof(Model::Vertex);
//		UINT offset = 0;
//		dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
//		dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
//		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//		// スケルトン用定数バッファ更新
//		// CbSkeleton cbSkeleton の WorldTransform 部分を、スナップショット行列から読み込む
//		CbSkeleton cbSkeleton{};
//
//		if (mesh.bones.size() > 0)
//		{
//			for (size_t i = 0; i < mesh.bones.size(); ++i)
//			{
//				const Model::Bone& bone = mesh.bones.at(i);
//
//				// ★ここが通常のDrawと異なる点: node->worldTransform ではなく、引数の配列から取得
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
//			// ボーンが無いメッシュ（メッシュノード）の場合
//			if (mesh.nodeIndex < (int)nodeTransforms.size()) {
//				cbSkeleton.boneTransforms[0] = nodeTransforms[mesh.nodeIndex];
//			}
//		}
//
//		// スケルトンCB (b6) に更新
//		// HologramShader::Begin() で既にバインドされているので、更新だけでOK
//		dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);
//
//		// テクスチャ設定 (t0)
//		if (noiseTexture) {
//			ID3D11ShaderResourceView* srvs[] = { noiseTexture.Get() };
//			dc->PSSetShaderResources(0, 1, srvs);
//		}
//
//		// 描画
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
//	// シェーダー設定解除
//	dc->VSSetShader(nullptr, nullptr, 0);
//	dc->PSSetShader(nullptr, nullptr, 0);
//	dc->IASetInputLayout(nullptr);
//
//	// リソース解除
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
//		// ベースカラーとアルファ
//		// baseColor.w は C++側のフェード処理で使用していますが、初期値として編集可能にします。
//		ImGui::ColorEdit4("Base Color (RGB & Initial Alpha)", (float*)&cbHologram.baseColor);
//
//		// リムライトカラー
//		ImGui::ColorEdit3("Rim Color", (float*)&cbHologram.rimColor);
//
//		ImGui::Separator();
//
//		// フレネル (輪郭線の強さ)
//		ImGui::SliderFloat("Fresnel Power", &cbHologram.fresnelPower, 0.1f, 10.0f);
//
//		// 走査線（スキャンライン）
//		ImGui::SliderFloat("Scanline Freq", &cbHologram.scanlineFreq, 1.0f, 100.0f);
//		ImGui::SliderFloat("Scanline Speed ", &cbHologram.scanlineSpeed, 0.0f, 20.0f);
//
//		ImGui::Separator();
//
//		// グリッチノイズ
//		ImGui::SliderFloat("Glitch Intensity", &cbHologram.glitchIntensity, 0.0f, 0.5f);
//
//		// (デバッグ用: 時間と現在のフェード率)
//		ImGui::Text("Shader Time: %.2f", cbHologram.time);
//		ImGui::Text("Current C++ Alpha: %.2f", cbHologram.alpha);
//	}
//}
