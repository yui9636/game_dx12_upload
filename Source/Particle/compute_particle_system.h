#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>

#include "System/Misc.h"
#include "GpuResourceUtils.h"
#include "ShaderClass/shader.h"
#include "RenderContext/RenderContext.h"
#include "RHI/ICommandList.h"

enum class ParticleBlendMode
{
	Opaque,
	Transparency,
	Additive,
	Subtraction,
	Multiply,
	Alpha
};





class compute_particle_system
{
public:
	static constexpr UINT NumParticleThread = 1024;

	void ChangeTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv)
	{
		shader_resource_view = srv;
	}
	
	static constexpr int MaxGradientKeys = 4;

	struct GradientColor {
		DirectX::XMFLOAT4 color;
		float             time;
		float             pad[3]{};          // 16byte align
	};


	void SetBlendMode(ParticleBlendMode mode) { blendMode = mode; }
	ParticleBlendMode GetBlendMode() const { return blendMode; }




	struct emit_particle_data
	{
		DirectX::XMFLOAT4	parameter;

		DirectX::XMFLOAT4	position = { 0, 0, 0, 0 };
		DirectX::XMFLOAT4	rotation = { 0, 0, 0, 0 };
		DirectX::XMFLOAT4	scale = { 1, 1, 1, 0 };

		DirectX::XMFLOAT4	velocity = { 0, 0, 0, 0 };
		DirectX::XMFLOAT4	acceleration = { 0, 0, 0, 0 };



		GradientColor gradientColors[MaxGradientKeys]{};
		int           gradientCount = 0;
		float         padGrad[3]{};

		DirectX::XMFLOAT4 scale_begin = { 1, 1, 1, 0 };
		DirectX::XMFLOAT4 scale_end = { 1, 1, 1, 0 };

		DirectX::XMFLOAT4 angularVelocity{ 0,0,0,0 };


		DirectX::XMFLOAT2 fade;
	};

	struct particle_data
	{
		DirectX::XMFLOAT4	parameter;

		DirectX::XMFLOAT4	position;
		DirectX::XMFLOAT4	rotation;
		DirectX::XMFLOAT4	scale;

		DirectX::XMFLOAT4	velocity;
		DirectX::XMFLOAT4	acceleration;

		DirectX::XMFLOAT4	texcoord;

		GradientColor gradientColors[MaxGradientKeys]{};
		int           gradientCount;
		float         padGrad[3]{};
		DirectX::XMFLOAT4	color;

		DirectX::XMFLOAT4   scale_begin;
		DirectX::XMFLOAT4   scale_end;

		DirectX::XMFLOAT4 angularVelocity;


		DirectX::XMFLOAT2 fade;
	};

	struct common_constants
	{
		float				elapsed_time;
		DirectX::XMUINT2	texture_split_count;
		UINT				system_num_particles;

		UINT				total_emit_count;
		UINT				common_dummy[3];
	};

	struct draw_indirect
	{
		UINT	vertex_count_per_instance;
		UINT	instance_count;
		UINT	start_vertex_location;
		UINT	start_instance_location;
	};

	struct particle_header
	{
		UINT	alive;
		UINT	particle_index;
		float	depth;

	};

	struct bitonic_sort_constants
	{
		UINT	increment;
		UINT	direction;
		UINT	dummy[2];
	};



	using	dispatch_indirect = DirectX::XMUINT3;

	static	constexpr	UINT	NumCurrentParticleOffset = 0;
	static	constexpr	UINT	NumPreviousParticleOffset = NumCurrentParticleOffset + sizeof(UINT);
	static	constexpr	UINT	NumDeadParticleOffset = NumPreviousParticleOffset + sizeof(UINT);
	static	constexpr	UINT	EmitDispatchIndirectOffset = NumDeadParticleOffset + sizeof(UINT);
	//static	constexpr	UINT	DrawIndirectSize = EmitDispatchIndirectOffset + sizeof(dispatch_indirect);

	static	constexpr	UINT	UpdateDispatchIndirectOffset = EmitDispatchIndirectOffset + sizeof(dispatch_indirect);
	static	constexpr	UINT	NumEmitParticleIndexOffset = UpdateDispatchIndirectOffset + sizeof(dispatch_indirect);
	static	constexpr	UINT	DrawIndirectOffset = NumEmitParticleIndexOffset + sizeof(UINT);
	static	constexpr	UINT	DrawIndirectSize = DrawIndirectOffset + sizeof(draw_indirect);
		
	static constexpr UINT BitonicSortB2Thread = 256;
	static constexpr UINT BitonicSortC2Thread = 512;



private:
	struct CbScene
	{
		DirectX::XMFLOAT4X4	viewProjection;
		DirectX::XMFLOAT4X4	inverseviewProjection;
		DirectX::XMFLOAT4 lightDirection;
		DirectX::XMFLOAT4 lightColor;
		DirectX::XMFLOAT4 cameraPosition;
		DirectX::XMFLOAT4X4 lightViewProjection; 
	};

public:
	compute_particle_system(ID3D11Device* device, UINT particles_count, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, DirectX::XMUINT2 split_count = DirectX::XMUINT2(1, 1));
	~compute_particle_system();

	//void emit(const emit_particle_data& data);
	//void update(ID3D11DeviceContext* immediate_context, float elapsed_time);
	//void render(ID3D11DeviceContext* immediate_context);

	void BindParticleDataToVS(const RenderContext& rc, UINT slot = 0)
	{
		rc.commandList->GetNativeContext()->VSSetShaderResources(slot, 1, particle_data_shader_resource_view.GetAddressOf());
	}

	void SetIndirectDrawIndexCount(ID3D11DeviceContext* dc, UINT indexCount);


	ID3D11Buffer* GetIndirectBuffer() const { return indirect_data_buffer.Get(); }

	ID3D11Buffer* GetRenderConstantBuffer() const { return renderOptionConstantBuffer.Get(); }

	void Begin(const RenderContext& rc);
	void emit( emit_particle_data& data);  
	void Update(const RenderContext& rc, float dt);
	void Draw(const RenderContext& rc);
	void End(const RenderContext& rc);

	void Clear(const RenderContext& rc);

	struct RenderOptionConstant
	{
		UINT  enable_velocity_stretch = 0;
		float velocity_stretch_scale = 0.05f;
		float velocity_stretch_max_aspect = 8.0f;
		float velocity_stretch_min_speed = 0.0f;

		float global_alpha = 1.0f;
		
		float curl_noise_strength = 0.0f;
		float curl_noise_scale = 0.1f;
		float curl_move_speed = 0.2f;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> renderOptionConstantBuffer;
	RenderOptionConstant renderOptionConstantData{};

	void SetVelocityStretchEnabled(bool v) { renderOptionConstantData.enable_velocity_stretch = v ? 1u : 0u; }
	void SetVelocityStretchScale(float v) { renderOptionConstantData.velocity_stretch_scale = v; }
	void SetVelocityStretchMaxAspect(float v) { renderOptionConstantData.velocity_stretch_max_aspect = v; }
	void SetVelocityStretchMinSpeed(float v) { renderOptionConstantData.velocity_stretch_min_speed = v; }
	void SetGlobalAlpha(float alpha) { renderOptionConstantData.global_alpha = alpha; }
	void SetCurlNoiseStrength(float v) { renderOptionConstantData.curl_noise_strength = v; }
	void SetCurlNoiseScale(float v) { renderOptionConstantData.curl_noise_scale = v; }
	void SetCurlMoveSpeed(float v) { renderOptionConstantData.curl_move_speed = v; }
	void SetCurlNoiseTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv)
	{
		curl_noise_shader_resource_view = srv;
	}








	void SetTextureSplitCount(const DirectX::XMUINT2& count) { texture_split_count = count; }

private:
	UINT num_particles;
	UINT num_emit_particles;
	bool one_shot_initialize;
	DirectX::XMUINT2 texture_split_count;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;

	std::vector<emit_particle_data> emit_particles;
	Microsoft::WRL::ComPtr<ID3D11Buffer> common_constant_buffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_data_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_data_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_data_unordered_access_view;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_append_consume_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_append_consume_unordered_access_view;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_emit_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_emit_shader_resource_view;

	Microsoft::WRL::ComPtr<ID3D11ComputeShader> init_shader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> emit_shader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> update_shader;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometry_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;

	Microsoft::WRL::ComPtr<ID3D11Buffer> indirect_data_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> indirect_data_unordered_access_view;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_header_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_header_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_header_unordered_access_view;

	Microsoft::WRL::ComPtr<ID3D11ComputeShader> begin_frame_shader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> end_frame_shader;

	Microsoft::WRL::ComPtr<ID3D11Buffer> bitonic_sort_constant_buffer;

	//Microsoft::WRL::ComPtr<ID3D11ComputeShader> sort_shader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> sort_b2_shader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> sort_c2_shader;

	Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer; 


	

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> curl_noise_shader_resource_view;






	ParticleBlendMode blendMode = ParticleBlendMode::Additive;
};
