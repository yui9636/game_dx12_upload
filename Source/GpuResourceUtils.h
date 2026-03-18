#pragma once

#include <d3d11.h>

namespace DirectX { class ScratchImage; struct TexMetadata; }

// GPU���\�[�X���[�e�B���e�B
class GpuResourceUtils
{
public:
	// API非依存: ファイルからScratchImageを読み込む（DX11/DX12共用）
	static HRESULT LoadImageFromFile(
		const char* filename,
		DirectX::ScratchImage& outImage,
		DirectX::TexMetadata& outMetadata);

	// ���_�V�F�[�_�[�ǂݍ���
	static HRESULT LoadVertexShader(
		ID3D11Device* device,
		const char* filename,
		const D3D11_INPUT_ELEMENT_DESC inputElementDescs[],
		UINT inputElementCount,
		ID3D11InputLayout** inputLayout,
		ID3D11VertexShader** vertexShader);

	// �s�N�Z���V�F�[�_�[�ǂݍ���
	static HRESULT LoadPixelShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11PixelShader** pixelShader);

	// �W�I���g���V�F�[�_�[�ǂݍ���
	static HRESULT LoadGeometryShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11GeometryShader** geometryShader);

	// �R���s���[�g�V�F�[�_�[�ǂݍ���
	static HRESULT LoadComputeShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11ComputeShader** computeShader);



	// �e�N�X�`���ǂݍ���
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const char* filename,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// �e�N�X�`���ǂݍ���
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const void* data,
		size_t size,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// �_�~�[�e�N�X�`���쐬
	static HRESULT CreateDummyTexture(
		ID3D11Device* device,
		UINT color,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// �萔�o�b�t�@�쐬
	static HRESULT CreateConstantBuffer(
		ID3D11Device* device,
		UINT bufferSize,
		ID3D11Buffer** constantBuffer);

};
