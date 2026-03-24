
#include <fstream>
#include "Sprite.h"
#include "System/Misc.h"
#include "GpuResourceUtils.h"

Sprite::Sprite(ID3D11Device* device)
	: Sprite(device, nullptr)
{
}

Sprite::Sprite(ID3D11Device* device, const char* filename)
{
	HRESULT hr = S_OK;

	{
		D3D11_BUFFER_DESC buffer_desc = {};
		buffer_desc.ByteWidth = sizeof(Vertex) * 4;
		buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
		buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		hr = device->CreateBuffer(&buffer_desc, nullptr, vertexBuffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	{
		D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		hr = GpuResourceUtils::LoadVertexShader(
			device,
			"Data/Shader/SpriteVS.cso",
			inputElementDesc,
			ARRAYSIZE(inputElementDesc),
			inputLayout.GetAddressOf(),
			vertexShader.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	}

	{
		hr = GpuResourceUtils::LoadPixelShader(
			device,
			"Data/Shader/SpritePS.cso",
			pixelShader.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}



	if (filename != nullptr)
	{
		D3D11_TEXTURE2D_DESC desc;
		hr = GpuResourceUtils::LoadTexture(device, filename, shaderResourceView.GetAddressOf(), &desc);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		textureWidth = static_cast<float>(desc.Width);
		textureHeight = static_cast<float>(desc.Height);
	}
	else
	{
		D3D11_TEXTURE2D_DESC desc;
		hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF, shaderResourceView.GetAddressOf(),
			&desc);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		textureWidth = static_cast<float>(desc.Width);
		textureHeight = static_cast<float>(desc.Height);
	}


	{
		GpuResourceUtils::LoadPixelShader(device,
			"Data/Shader/SpriteUI_PS.cso", 
			uiPixelShader.GetAddressOf());

		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = sizeof(UIConstants);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		device->CreateBuffer(&cbDesc, nullptr, uiConstantBuffer.GetAddressOf());
	}



}

void Sprite::Render(ID3D11DeviceContext* dc,
	float dx, float dy,
	float dz,
	float dw, float dh,
	float sx, float sy,
	float sw, float sh,
	float angle,
	float r, float g, float b, float a
) const
{
	DirectX::XMFLOAT2 positions[] = {
		DirectX::XMFLOAT2(dx,      dy),
		DirectX::XMFLOAT2(dx + dw, dy),
		DirectX::XMFLOAT2(dx,      dy + dh),
		DirectX::XMFLOAT2(dx + dw, dy + dh),
	};

	DirectX::XMFLOAT2 texcoords[] = {
		DirectX::XMFLOAT2(sx,      sy),
		DirectX::XMFLOAT2(sx + sw, sy),
		DirectX::XMFLOAT2(sx,      sy + sh),
		DirectX::XMFLOAT2(sx + sw, sy + sh),
	};

	float mx = dx + dw * 0.5f;
	float my = dy + dh * 0.5f;
	for (auto& p : positions)
	{
		p.x -= mx;
		p.y -= my;
	}

	float theta = DirectX::XMConvertToRadians(angle);
	float c = cosf(theta);
	float s = sinf(theta);
	for (auto& p : positions)
	{
		DirectX::XMFLOAT2 r = p;
		p.x = c * r.x + -s * r.y;
		p.y = s * r.x + c * r.y;
	}

	for (auto& p : positions)
	{
		p.x += mx;
		p.y += my;
	}

	D3D11_VIEWPORT viewport;
	UINT numViewports = 1;
	dc->RSGetViewports(&numViewports, &viewport);
	float screenWidth = viewport.Width;
	float screenHeight = viewport.Height;

	for (DirectX::XMFLOAT2& p : positions)
	{
		p.x = 2.0f * p.x / screenWidth - 1.0f;
		p.y = 1.0f - 2.0f * p.y / screenHeight;
	}

	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	HRESULT hr = dc->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	Vertex* v = static_cast<Vertex*>(mappedSubresource.pData);
	for (int i = 0; i < 4; ++i)
	{
		v[i].position.x = positions[i].x;
		v[i].position.y = positions[i].y;
		v[i].position.z = dz;

		v[i].color.x = r;
		v[i].color.y = g;
		v[i].color.z = b;
		v[i].color.w = a;

		v[i].texcoord.x = texcoords[i].x / textureWidth;
		v[i].texcoord.y = texcoords[i].y / textureHeight;
	}

	dc->Unmap(vertexBuffer.Get(), 0);

	if (uiConstantBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE mappedCB;
		if (SUCCEEDED(dc->Map(uiConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
		{
			UIConstants* data = static_cast<UIConstants*>(mappedCB.pData);

			data->color = currentColor;

			data->glowColor.x = currentGlowColor.x;
			data->glowColor.y = currentGlowColor.y;
			data->glowColor.z = currentGlowColor.z;
			data->glowColor.w = currentGlowIntensity;

			data->glowIntensity = currentGlowIntensity;

			dc->Unmap(uiConstantBuffer.Get(), 0);
		}
	}

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
	dc->IASetInputLayout(inputLayout.Get());
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	dc->VSSetShader(vertexShader.Get(), nullptr, 0);

	if (uiPixelShader)
	{
		dc->PSSetShader(uiPixelShader.Get(), nullptr, 0);

		dc->PSSetConstantBuffers(0, 1, uiConstantBuffer.GetAddressOf());
	}
	else
	{
		dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	}

	dc->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

	dc->Draw(4, 0);


}

void Sprite::Render(ID3D11DeviceContext* dc,
	float dx, float dy,
	float dz,
	float dw, float dh,
	float angle,
	float r, float g, float b, float a
) const
{
	Render(dc, dx, dy, dz, dw, dh, 0, 0, textureWidth, textureHeight, angle, r, g, b, a);
}


void Sprite::SetShaderResourceView(const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, float texWidth, float texHeight)
{
	shaderResourceView = srv;
	textureWidth = static_cast<int>(texWidth);
	textureHeight = static_cast<int>(texHeight);
}





