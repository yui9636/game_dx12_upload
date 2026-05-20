#pragma once

#include <d3d11.h>

namespace DirectX { class ScratchImage; struct TexMetadata; }

class GpuResourceUtils
{
public:
	// DirectXTex で画像ファイルを読み込み、pixel data と metadata を返す。
	static HRESULT LoadImageFromFile(
		const char* filename,
		DirectX::ScratchImage& outImage,
		DirectX::TexMetadata& outMetadata);

	// .cso から vertex shader を作成し、同時に input layout も作る。
	static HRESULT LoadVertexShader(
		ID3D11Device* device,
		const char* filename,
		const D3D11_INPUT_ELEMENT_DESC inputElementDescs[],
		UINT inputElementCount,
		ID3D11InputLayout** inputLayout,
		ID3D11VertexShader** vertexShader);

	// .cso から pixel shader を作成する。
	static HRESULT LoadPixelShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11PixelShader** pixelShader);

	// .cso から geometry shader を作成する。
	static HRESULT LoadGeometryShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11GeometryShader** geometryShader);

	// .cso から compute shader を作成する。
	static HRESULT LoadComputeShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11ComputeShader** computeShader);



	// 画像ファイルから DX11 SRV を作成する。
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const char* filename,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// メモリ上の画像データから DX11 SRV を作成する。
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const void* data,
		size_t size,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// 単色 1x1 texture を作成し、missing texture などの fallback に使う。
	static HRESULT CreateDummyTexture(
		ID3D11Device* device,
		UINT color,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// 16 byte alignment を満たした DX11 constant buffer を作成する。
	static HRESULT CreateConstantBuffer(
		ID3D11Device* device,
		UINT bufferSize,
		ID3D11Buffer** constantBuffer);

};
