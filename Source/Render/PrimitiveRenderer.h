#pragma once

#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

class PrimitiveRenderer
{
public:
	// DX11 の簡易線描画用 resource を作成する。
	PrimitiveRenderer(ID3D11Device* device);

	// 一時 vertex list に 1 頂点追加する。
	void AddVertex(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color);

	// transform 原点に RGB 軸線を追加する。
	void DrawAxis(const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color);

	// XZ 平面の grid 線を追加する。
	void DrawGrid(int subdivisions, float scale);

	// 蓄積済み vertices を GPU buffer へ転送し、指定 topology で描画する。
	void Render(
		ID3D11DeviceContext* dc,
		const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& projection,
		D3D11_PRIMITIVE_TOPOLOGY primitiveTopology);

private:
	static const UINT VertexCapacity = 3 * 1024; // 1 回の Render で扱う最大頂点数。

	struct CbScene
	{
		DirectX::XMFLOAT4X4		viewProjection; // primitive 用 view projection。
		DirectX::XMFLOAT4		color;          // shader 側の補助色。
	};

	struct Vertex
	{
		DirectX::XMFLOAT3	position; // world space position。
		DirectX::XMFLOAT4	color;    // vertex color。
	};
	std::vector<Vertex>		vertices; // CPU 側に蓄積する一時 vertex list。

	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;   // primitive VS。
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;    // primitive PS。
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;    // position/color layout。
	Microsoft::WRL::ComPtr<ID3D11Buffer>		vertexBuffer;   // dynamic vertex buffer。
	Microsoft::WRL::ComPtr<ID3D11Buffer>		constantBuffer; // CbScene 用 constant buffer。
};
