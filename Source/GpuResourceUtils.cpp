#include <filesystem>
#include <cstring>
#include <wrl.h>
#include <DirectXTex.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "System/Misc.h"
#include "GpuResourceUtils.h"

namespace
{
	HRESULT LoadImageWithStb(
		const char* filename,
		DirectX::ScratchImage& outImage,
		DirectX::TexMetadata& outMetadata)
	{
		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* pixels = stbi_load(filename, &width, &height, &channels, 4);
		if (!pixels || width <= 0 || height <= 0) {
			if (pixels) {
				stbi_image_free(pixels);
			}
			return E_FAIL;
		}

		HRESULT hr = outImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,
			static_cast<size_t>(width),
			static_cast<size_t>(height),
			1u,
			1u);
		if (FAILED(hr)) {
			stbi_image_free(pixels);
			return hr;
		}

		const DirectX::Image* image = outImage.GetImage(0, 0, 0);
		if (!image || !image->pixels) {
			stbi_image_free(pixels);
			return E_FAIL;
		}

		const size_t srcRowPitch = static_cast<size_t>(width) * 4u;
		for (int y = 0; y < height; ++y) {
			memcpy(
				image->pixels + image->rowPitch * static_cast<size_t>(y),
				pixels + srcRowPitch * static_cast<size_t>(y),
				srcRowPitch);
		}

		outMetadata = outImage.GetMetadata();
		stbi_image_free(pixels);
		return S_OK;
	}
}

HRESULT GpuResourceUtils::LoadVertexShader(
	ID3D11Device* device,
	const char* filename,
	const D3D11_INPUT_ELEMENT_DESC inputElementDescs[],
	UINT inputElementCount,
	ID3D11InputLayout** inputLayout,
	ID3D11VertexShader** vertexShader)
{
	FILE* fp = nullptr;
	fopen_s(&fp, filename, "rb");
	_ASSERT_EXPR_A(fp, "Vertex Shader File not found");

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	std::unique_ptr<u_char[]> data = std::make_unique<u_char[]>(size);
	fread(data.get(), size, 1, fp);
	fclose(fp);

	HRESULT hr = device->CreateVertexShader(data.get(), size, nullptr, vertexShader);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	if (inputLayout != nullptr)
	{
		hr = device->CreateInputLayout(inputElementDescs, inputElementCount, data.get(), size, inputLayout);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	return hr;
}

HRESULT GpuResourceUtils::LoadPixelShader(
	ID3D11Device* device,
	const char* filename,
	ID3D11PixelShader** pixelShader)
{
	FILE* fp = nullptr;
	fopen_s(&fp, filename, "rb");
	_ASSERT_EXPR_A(fp, "Pixel Shader File not found");

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	std::unique_ptr<u_char[]> data = std::make_unique<u_char[]>(size);
	fread(data.get(), size, 1, fp);
	fclose(fp);

	HRESULT hr = device->CreatePixelShader(data.get(), size, nullptr, pixelShader);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	return hr;
}

HRESULT GpuResourceUtils::LoadGeometryShader(
	ID3D11Device* device, 
	const char* filename, 
	ID3D11GeometryShader** geometryShader)
{
	FILE* fp = nullptr;
	fopen_s(&fp, filename, "rb");
	_ASSERT_EXPR_A(fp, "Geometr Shader File not found");

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	std::unique_ptr<u_char[]> data = std::make_unique<u_char[]>(size);
	fread(data.get(), size, 1, fp);
	fclose(fp);

	HRESULT hr = device->CreateGeometryShader(data.get(), size, nullptr, geometryShader);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	return hr;

}

HRESULT GpuResourceUtils::LoadComputeShader(
	ID3D11Device* device,
	const char* filename,
	ID3D11ComputeShader** computeShader)
{
	FILE* filePointer = nullptr;
	fopen_s(&filePointer, filename, "rb");
	_ASSERT_EXPR_A(filePointer, "Compute Shader File not found");

	fseek(filePointer, 0, SEEK_END);
	long fileSize = ftell(filePointer);
	fseek(filePointer, 0, SEEK_SET);

	std::unique_ptr<u_char[]> shaderData = std::make_unique<u_char[]>(fileSize);
	fread(shaderData.get(), fileSize, 1, filePointer);
	fclose(filePointer);

	HRESULT hr = device->CreateComputeShader(shaderData.get(), fileSize, nullptr, computeShader);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	return hr;
}











// ========================================================
// ========================================================
HRESULT GpuResourceUtils::LoadImageFromFile(
	const char* filename,
	DirectX::ScratchImage& outImage,
	DirectX::TexMetadata& outMetadata)
{
	std::filesystem::path filepath(filename);
	std::string extension = filepath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

	std::wstring wfilename = filepath.wstring();

	HRESULT hr;
	if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp")
	{
		hr = LoadImageWithStb(filename, outImage, outMetadata);
	}
	else if (extension == ".tga")
	{
		hr = DirectX::GetMetadataFromTGAFile(wfilename.c_str(), outMetadata);
		if (FAILED(hr)) return hr;
		hr = DirectX::LoadFromTGAFile(wfilename.c_str(), &outMetadata, outImage);
	}
	else if (extension == ".dds")
	{
		hr = DirectX::GetMetadataFromDDSFile(wfilename.c_str(), DirectX::DDS_FLAGS_NONE, outMetadata);
		if (FAILED(hr)) return hr;
		hr = DirectX::LoadFromDDSFile(wfilename.c_str(), DirectX::DDS_FLAGS_NONE, &outMetadata, outImage);
	}
	else if (extension == ".hdr")
	{
		hr = DirectX::GetMetadataFromHDRFile(wfilename.c_str(), outMetadata);
		if (FAILED(hr)) return hr;
		hr = DirectX::LoadFromHDRFile(wfilename.c_str(), &outMetadata, outImage);
	}
	else
	{
		hr = DirectX::GetMetadataFromWICFile(wfilename.c_str(), DirectX::WIC_FLAGS_NONE, outMetadata);
		if (FAILED(hr)) return hr;
		hr = DirectX::LoadFromWICFile(wfilename.c_str(), DirectX::WIC_FLAGS_NONE, &outMetadata, outImage);
	}
	return hr;
}

// ========================================================
// ========================================================
HRESULT GpuResourceUtils::LoadTexture(
	ID3D11Device* device,
	const char* filename,
	ID3D11ShaderResourceView** shaderResourceView,
	D3D11_TEXTURE2D_DESC* texture2dDesc)
{
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratch_image;
	HRESULT hr = LoadImageFromFile(filename, scratch_image, metadata);
	if (FAILED(hr)) return hr;

	hr = DirectX::CreateShaderResourceView(device, scratch_image.GetImages(), scratch_image.GetImageCount(),
		metadata, shaderResourceView);

	if (SUCCEEDED(hr) && texture2dDesc != nullptr)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		(*shaderResourceView)->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		hr = resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		texture2d->GetDesc(texture2dDesc);
	}
	return hr;
}

HRESULT GpuResourceUtils::LoadTexture(
	ID3D11Device* device,
	const void* data,
	size_t size,
	ID3D11ShaderResourceView** shaderResourceView,
	D3D11_TEXTURE2D_DESC* texture2dDesc)
{
	HRESULT hr = E_FAIL;
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratch_image;

	// .tga
	{
		hr = DirectX::GetMetadataFromTGAMemory(data, size, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromTGAMemory(data, size, &metadata, scratch_image);
		}
	}
	// .dds
	if (FAILED(hr))
	{
		hr = DirectX::GetMetadataFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, &metadata, scratch_image);
		}
	}
	// .hdr
	if (FAILED(hr))
	{
		hr = DirectX::GetMetadataFromHDRMemory(data, size, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromHDRMemory(data, size, &metadata, scratch_image);
		}
	}
	if (FAILED(hr))
	{
		hr = DirectX::GetMetadataFromWICMemory(data, size, DirectX::WIC_FLAGS_NONE, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromWICMemory(data, size, DirectX::WIC_FLAGS_NONE, &metadata, scratch_image);
		}
	}
	if (FAILED(hr))
	{
		return hr;
	}

	hr = DirectX::CreateShaderResourceView(device, scratch_image.GetImages(), scratch_image.GetImageCount(),
		metadata, shaderResourceView);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	if (texture2dDesc != nullptr)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		(*shaderResourceView)->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		hr = resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		texture2d->GetDesc(texture2dDesc);
	}
	return hr;
}

HRESULT GpuResourceUtils::CreateDummyTexture(
	ID3D11Device* device,
	UINT color,
	ID3D11ShaderResourceView** shaderResourceView,
	D3D11_TEXTURE2D_DESC* texture2dDesc)
{
	D3D11_TEXTURE2D_DESC desc = { 0 };
	desc.Width = 1;
	desc.Height = 1;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = &color;
	data.SysMemPitch = desc.Width;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>	texture;
	HRESULT hr = device->CreateTexture2D(&desc, &data, texture.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	hr = device->CreateShaderResourceView(texture.Get(), nullptr, shaderResourceView);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	if (texture2dDesc != nullptr)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		(*shaderResourceView)->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		hr = resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		texture2d->GetDesc(texture2dDesc);
	}

	return hr;
}

HRESULT GpuResourceUtils::CreateConstantBuffer(
	ID3D11Device* device,
	UINT bufferSize,
	ID3D11Buffer** constantBuffer)
{
	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.ByteWidth = bufferSize;
	desc.StructureByteStride = 0;

	HRESULT hr = device->CreateBuffer(&desc, 0, constantBuffer);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	return hr;
}
