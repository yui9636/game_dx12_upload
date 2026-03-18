#include "compute_particle_system.h"

compute_particle_system::compute_particle_system(ID3D11Device* device, UINT particles_count, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, DirectX::XMUINT2 split_count)
{
	HRESULT hr;

	//	バイトニックソートの仕様上、2の累乗にしておく必要があるためパーティクル数を補正
	float	f_exponent = log2f(static_cast<float>(particles_count));
	int		exponent = static_cast<int>(ceilf(f_exponent) + 0.5f);
	particles_count = static_cast<UINT>(pow(2, exponent) + 0.5f);
	particles_count = max(min(particles_count, 1 << 27), 1 << 7);

	//	パーティクル数をスレッド数に合わせて制限
	num_particles = ((particles_count + (NumParticleThread - 1)) / NumParticleThread) * NumParticleThread;
	num_emit_particles = min(num_particles, 10000);	//	1Fでの生成制限数はテキトーに決めているのでここは要調整
	texture_split_count = split_count;
	one_shot_initialize = false;

	////シーン用定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
	device,
	sizeof(CbScene),
	sceneConstantBuffer.GetAddressOf());


	GpuResourceUtils::CreateConstantBuffer(
	device,
	sizeof(RenderOptionConstant),
	renderOptionConstantBuffer.GetAddressOf());







	//	定数バッファ
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		{
			buffer_desc.ByteWidth = sizeof(common_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, common_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}

		{
			buffer_desc.ByteWidth = sizeof(bitonic_sort_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, bitonic_sort_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}



	}

	//	パーティクルバッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(particle_data) * num_particles;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(particle_data);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, particle_data_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		hr = device->CreateShaderResourceView(particle_data_buffer.Get(), nullptr, particle_data_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		hr = device->CreateUnorderedAccessView(particle_data_buffer.Get(), nullptr, particle_data_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	//	パーティクルの生成/破棄番号をため込むバッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(UINT) * num_particles;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(UINT);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, particle_append_consume_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		//	Append/Consumeを利用する場合はビュー側にフラグを立てる
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Format = DXGI_FORMAT_UNKNOWN;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = num_particles;
		uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		hr = device->CreateUnorderedAccessView(particle_append_consume_buffer.Get(), &uav_desc, particle_append_consume_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	//	パーティクルエミット用バッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.ByteWidth = sizeof(emit_particle_data) * num_emit_particles;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(emit_particle_data);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, particle_emit_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		hr = device->CreateShaderResourceView(particle_emit_buffer.Get(), nullptr, particle_emit_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}


	//	パーティクルヘッダーバッファ
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(particle_header) * num_particles;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(particle_header);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, particle_header_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		hr = device->CreateShaderResourceView(particle_header_buffer.Get(), nullptr, particle_header_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		hr = device->CreateUnorderedAccessView(particle_header_buffer.Get(), nullptr, particle_header_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	//	パーティクルの更新・描画数の削減のためのバッファ
	{
		/*D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = DrawIndirectSize;
		desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, indirect_data_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));*/

		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = DrawIndirectSize;
		desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, indirect_data_buffer.GetAddressOf());
		D3D11_SUBRESOURCE_DATA initialize_data = {};
		std::vector<UINT>	initialize_buffer(desc.ByteWidth / sizeof(UINT));
		{
			//	初期値設定
			draw_indirect* draw_data = reinterpret_cast<draw_indirect*>(initialize_buffer.data() + DrawIndirectOffset / sizeof(UINT));
			draw_data->vertex_count_per_instance = num_particles;
			draw_data->instance_count = 1;
			draw_data->start_vertex_location = 0;
			draw_data->start_instance_location = 0;
		}
		initialize_data.pSysMem = initialize_buffer.data();
		initialize_data.SysMemPitch = desc.ByteWidth;
		hr = device->CreateBuffer(&desc, &initialize_data, indirect_data_buffer.GetAddressOf());



		//	Append/Consumeを利用する場合はビュー側にフラグを立てる
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = desc.ByteWidth / sizeof(UINT);
		uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		hr = device->CreateUnorderedAccessView(indirect_data_buffer.Get(), &uav_desc, indirect_data_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}







	// コンピュートシェーダー読み込み（初期化）
	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/compute_particle_init_cs.cso",
		init_shader.GetAddressOf());

	// コンピュートシェーダー読み込み（エミット）
	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/compute_particle_emit_cs.cso",
		emit_shader.GetAddressOf());

	// コンピュートシェーダー読み込み（アップデート）
	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/compute_particle_update_cs.cso",
		update_shader.GetAddressOf());

	// コンピュートシェーダー読み込み(begin)
	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/compute_particle_begin_frame_cs.cso",
		begin_frame_shader.GetAddressOf());

	// コンピュートシェーダー読み込み(end)
	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/compute_particle_end_frame_cs.cso",
		end_frame_shader.GetAddressOf());

	// コンピュートシェーダー読み込み(sort)
	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/compute_particle_bitonic_sort_b2_cs.cso",
		sort_b2_shader.GetAddressOf());

	// コンピュートシェーダー読み込み(sort)
	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/compute_particle_bitonic_sort_c2_cs.cso",
		sort_c2_shader.GetAddressOf());




	//	描画用情報生成
	this->shader_resource_view = shader_resource_view;

	// 頂点シェーダー読み込み
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/compute_particle_render_vs.cso",
		nullptr,      
		0,
		nullptr,
		vertex_shader.GetAddressOf()
	);

	// ジオメトリシェーダー読み込み
	GpuResourceUtils::LoadGeometryShader(
		device,
		"Data/Shader/compute_particle_render_gs.cso",
		geometry_shader.GetAddressOf()
	);

	// ピクセルシェーダー読み込み
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/compute_particle_render_ps.cso",
		pixel_shader.GetAddressOf()
	);



}

compute_particle_system::~compute_particle_system()
{
	init_shader.Reset();
	update_shader.Reset();
	emit_shader.Reset();
	vertex_shader.Reset();
	geometry_shader.Reset();
	pixel_shader.Reset();
	shader_resource_view.Reset();
}

void compute_particle_system::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

	// ── シェーダー設定 ──
	dc->VSSetShader(vertex_shader.Get(), nullptr, 0);
	dc->GSSetShader(geometry_shader.Get(), nullptr, 0);
	dc->PSSetShader(pixel_shader.Get(), nullptr, 0);

	// ── シーン定数バッファ更新 ──
	CbScene cbScene{};
	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
	DirectX::XMStoreFloat4x4(&cbScene.viewProjection, V * P);
	DirectX::XMMATRIX invV = XMMatrixInverse(nullptr, V);
	DirectX::XMStoreFloat4x4(&cbScene.inverseviewProjection, invV);  


	const DirectionalLight& directionalLight = rc.directionalLight;
	cbScene.lightDirection.x = directionalLight.direction.x;
	cbScene.lightDirection.y = directionalLight.direction.y;
	cbScene.lightDirection.z = directionalLight.direction.z;
	cbScene.lightColor.x = directionalLight.color.x;
	cbScene.lightColor.y = directionalLight.color.y;
	cbScene.lightColor.z = directionalLight.color.z;

	cbScene.cameraPosition = { rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z, 1.0f };


	dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, nullptr, &cbScene, 0, 0);

	ID3D11Buffer* sceneConstantBuffers[] = { sceneConstantBuffer.Get() };
	dc->VSSetConstantBuffers(0, 1, sceneConstantBuffers);  
	dc->GSSetConstantBuffers(0, 1, sceneConstantBuffers);  
	dc->PSSetConstantBuffers(0, 1, sceneConstantBuffers);  


	dc->UpdateSubresource(renderOptionConstantBuffer.Get(), 0, nullptr, &renderOptionConstantData, 0, 0);
	// GSへ（b2に合わせる）
	ID3D11Buffer* cbs[] = { renderOptionConstantBuffer.Get() };
	dc->GSSetConstantBuffers(2, 1, cbs);
	dc->PSSetConstantBuffers(2, 1, cbs);
	dc->CSSetConstantBuffers(2, 1, cbs);




	//ID3D11SamplerState* samplers[] =
	//{
	//rc.renderState->GetSamplerState(SamplerState::LinearClamp),     
	//rc.renderState->GetSamplerState(SamplerState::LinearWrap)     
	//};
	//dc->PSSetSamplers(0, _countof(samplers), samplers);
	//dc->CSSetSamplers(0, _countof(samplers), samplers);

	dc->CSSetShaderResources(2, 1, curl_noise_shader_resource_view.GetAddressOf());

	////レンダーステート設定
	//const float blend_factor[4] = { 1.0f,1.0f,1.0f,1.0f };
	//dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Additive), blend_factor, 0xFFFFFFFF);
	//switch (blendMode)
	//{
	//case ParticleBlendMode::Additive:
	//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Additive), blend_factor, 0xFFFFFFFF);
	//	break;

	//case ParticleBlendMode::Alpha:
	//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Alpha), blend_factor, 0xFFFFFFFF);
	//	break;

	//case ParticleBlendMode::Multiply:
	//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Multiply), blend_factor, 0xFFFFFFFF);
	//	break;
	//case ParticleBlendMode::Opaque:
	//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blend_factor, 0xFFFFFFFF);
	//	break;

	//case ParticleBlendMode::Subtraction:
	//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Subtraction), blend_factor, 0xFFFFFFFF);
	//	break;

	//case ParticleBlendMode::Transparency:
	//	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), blend_factor, 0xFFFFFFFF);
	//	break;




	//}

	//dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	//dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));


}

void compute_particle_system::emit( emit_particle_data& data)
{
	if (emit_particles.size() >= num_emit_particles)
		return;

	emit_particles.emplace_back(data);
}

void compute_particle_system::Update(const RenderContext& rc, float dt)
{
	ID3D11DeviceContext* immediate_context = rc.commandList->GetNativeContext();

	immediate_context->CSSetConstantBuffers(10, 1, common_constant_buffer.GetAddressOf());
	immediate_context->CSSetConstantBuffers(11, 1, bitonic_sort_constant_buffer.GetAddressOf());


	common_constants constant{};
	constant.elapsed_time = dt;
	constant.texture_split_count = texture_split_count;
	constant.system_num_particles = num_particles;
	constant.total_emit_count = static_cast<UINT>(emit_particles.size());

	immediate_context->UpdateSubresource(common_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);

	immediate_context->CSSetShaderResources(0, 1, particle_emit_shader_resource_view.GetAddressOf());


	//	SRV/UAV設定
	{
		//	
		immediate_context->CSSetShaderResources(0, 1, particle_emit_shader_resource_view.GetAddressOf());

		ID3D11UnorderedAccessView* uavs[] =
		{
			particle_data_unordered_access_view.Get(),
			particle_append_consume_unordered_access_view.Get(),
			indirect_data_unordered_access_view.Get(),
			particle_header_unordered_access_view.Get(),
		};
		immediate_context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
	}

	//	初期化処理
	if (!one_shot_initialize)
	{
		one_shot_initialize = true;
		immediate_context->CSSetShader(init_shader.Get(), nullptr, 0);
		immediate_context->Dispatch(num_particles / NumParticleThread, 1, 1);
	}

	//	フレーム開始時の処理
	{
		//	現在フレームでのパーティクル総数を算出
		//	それに合わせて各種設定を行う
		immediate_context->CSSetShader(begin_frame_shader.Get(), nullptr, 0);
		immediate_context->Dispatch(1, 1, 1);
	}


	//	エミット処理
	if (!emit_particles.empty())
	{
		//	エミットバッファ更新
		D3D11_BOX write_box = {};
		write_box.left = 0;
		write_box.right = static_cast<UINT>(emit_particles.size() * sizeof(emit_particle_data));
		write_box.top = 0;
		write_box.bottom = 1;
		write_box.front = 0;
		write_box.back = 1;
		immediate_context->UpdateSubresource(particle_emit_buffer.Get(),
			0,
			&write_box,
			emit_particles.data(),
			static_cast<UINT>(emit_particles.size() * sizeof(emit_particle_data)),
			0);
		immediate_context->CSSetShader(emit_shader.Get(), nullptr, 0);
		//immediate_context->Dispatch(static_cast<UINT>(emit_particles.size()), 1, 1);
		immediate_context->DispatchIndirect(indirect_data_buffer.Get(), EmitDispatchIndirectOffset);
		emit_particles.clear();
	}

	//	更新処理
	{
		immediate_context->CSSetShader(update_shader.Get(), nullptr, 0);
		//immediate_context->Dispatch(num_particles / NumParticleThread, 1, 1);
		immediate_context->DispatchIndirect(indirect_data_buffer.Get(), UpdateDispatchIndirectOffset);
	}

	{
		//	ソート処理
		//immediate_context->CSSetShader(sort_shader.Get(), nullptr, 0);
		//immediate_context->Dispatch(1, 1, 1);

			//	バイトニックソート
		//	https://www.bealto.com/gpu-sorting_parallel-bitonic-1.html
		float	f_exponent = log2f(static_cast<float>(num_particles));
		UINT	exponent = static_cast<UINT>(ceilf(f_exponent) + 0.5f);
		for (UINT i = 0; i < exponent; ++i)
		{
			immediate_context->CSSetShader(sort_b2_shader.Get(), nullptr, 0);
			UINT increment = 1 << i;
			for (UINT j = 0; j < i + 1; ++j)
			{
				bitonic_sort_constants	constant;
				constant.increment = increment;
				constant.direction = 2 << i;
				immediate_context->UpdateSubresource(bitonic_sort_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);

				if (increment <= BitonicSortC2Thread)
				{
					immediate_context->CSSetShader(sort_c2_shader.Get(), nullptr, 0);
					immediate_context->Dispatch(num_particles / 2 / BitonicSortC2Thread, 1, 1);
					break;
				}

				immediate_context->Dispatch(num_particles / 2 / BitonicSortB2Thread, 1, 1);
				increment /= 2;
			}
		}


	}

	//	フレーム終了時の処理
	{
		//	総パーティクル数を変動させる
		immediate_context->CSSetShader(end_frame_shader.Get(), nullptr, 0);
		immediate_context->Dispatch(1, 1, 1);
	}


	//	UAV設定
	{
		ID3D11UnorderedAccessView* uavs[] = { nullptr, nullptr, nullptr, nullptr, };
		immediate_context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
	}


}

void compute_particle_system::Draw(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

	dc->GSSetConstantBuffers(10, 1, common_constant_buffer.GetAddressOf());


	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);  

	//	入力レイアウト設定
	dc->IASetInputLayout(nullptr);


	dc->PSSetShaderResources(0, 1, shader_resource_view.GetAddressOf());
	//dc->GSSetShaderResources(0, 1, particle_data_shader_resource_view.GetAddressOf());
	dc->GSSetShaderResources(0, 1, particle_data_shader_resource_view.GetAddressOf());
	dc->GSSetShaderResources(1, 1, particle_header_shader_resource_view.GetAddressOf());



	//	バッファクリア
	ID3D11Buffer* clear_buffer[] = { nullptr };
	UINT strides[] = { 0 };
	UINT offsets[] = { 0 };
	dc->IASetVertexBuffers(0, 1, clear_buffer, strides, offsets);
	dc->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

	//dc->Draw(num_particles, 0);

	//	GPU側で計算したパーティクルの描画数をCPU側で取得して描画コールを呼び出すのはもったいないので、
	//	DrawIndirect命令を使ってGPU側だけで処理させる
	dc->DrawInstancedIndirect(indirect_data_buffer.Get(), DrawIndirectOffset);


}




void compute_particle_system::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->GSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);

	ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };

	dc->GSSetShaderResources(0, 2, nullSRV);
	dc->PSSetShaderResources(0, 1, nullSRV);
}

void compute_particle_system::SetIndirectDrawIndexCount(ID3D11DeviceContext* dc, UINT indexCount)
{
	// ここだけをピンポイントで書き換える処理
	D3D11_BOX destBox;
	destBox.left = 40;            // DrawIndirect構造体の開始位置
	destBox.right = 40 + sizeof(UINT);
	destBox.top = 0;
	destBox.bottom = 1;
	destBox.front = 0;
	destBox.back = 1;

	dc->UpdateSubresource(indirect_data_buffer.Get(), 0, &destBox, &indexCount, 0, 0);
}


void compute_particle_system::Clear(const RenderContext& rc)
{
	ID3D11DeviceContext* immediate_context = rc.commandList->GetNativeContext();

	// 1. エミットリストのクリア
	emit_particles.clear();

	// -----------------------------------------------------------------------
	// ★追加修正: Indirect Buffer (カウンター類) のリセット処理
	// これを行わないと、生存数カウントが残ったままになり、次回再生時に生成が止まります
	// -----------------------------------------------------------------------
	D3D11_BUFFER_DESC desc;
	indirect_data_buffer->GetDesc(&desc);

	// バッファの初期値を生成（コンストラクタと同様のデータを作成）
	// 全体を0で埋めた後、DrawIndirect用の引数だけ正しい値セットします
	std::vector<UINT> initialize_buffer(desc.ByteWidth / sizeof(UINT), 0);

	// DrawArgs部分の設定 (DrawIndirectOffsetの位置)
	draw_indirect* draw_data = reinterpret_cast<draw_indirect*>(initialize_buffer.data() + DrawIndirectOffset / sizeof(UINT));
	draw_data->vertex_count_per_instance = num_particles; // コンストラクタと同じ設定値
	draw_data->instance_count = 1;
	draw_data->start_vertex_location = 0;
	draw_data->start_instance_location = 0;

	// GPUバッファに転送してリセット
	immediate_context->UpdateSubresource(indirect_data_buffer.Get(), 0, nullptr, initialize_buffer.data(), 0, 0);
	// -----------------------------------------------------------------------


	ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
	immediate_context->VSSetShaderResources(0, 2, nullSRV);
	immediate_context->GSSetShaderResources(0, 2, nullSRV);
	immediate_context->PSSetShaderResources(0, 2, nullSRV);
	immediate_context->CSSetShaderResources(0, 2, nullSRV);


	UINT initCounts[] = { 0, 0, 0, 0 };
	ID3D11UnorderedAccessView* uavs[] =
	{
		particle_data_unordered_access_view.Get(),
		particle_append_consume_unordered_access_view.Get(),
		indirect_data_unordered_access_view.Get(),
		particle_header_unordered_access_view.Get(),
	};
	// Append/Consumeバッファのカウンタはここで0リセットされる
	immediate_context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, initCounts);

	// 3. 初期化シェーダー実行 (DeadListの充填)
	immediate_context->CSSetShader(init_shader.Get(), nullptr, 0);
	immediate_context->Dispatch(num_particles / NumParticleThread, 1, 1);

	// 4. 後始末
	ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr, nullptr, nullptr };
	immediate_context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), nullUAVs, nullptr);
	immediate_context->CSSetShader(nullptr, nullptr, 0);

	one_shot_initialize = true;
}
