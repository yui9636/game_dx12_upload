#pragma once

#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

class Sprite
{
public:
	Sprite(ID3D11Device* device);
	Sprite(ID3D11Device* device, const char* filename);

	struct Vertex
	{
		DirectX::XMFLOAT3	position;
		DirectX::XMFLOAT4	color;
		DirectX::XMFLOAT2	texcoord;
	};

	void Render(ID3D11DeviceContext* dc,
		float dx, float dy,
		float dz,
		float dw, float dh,
		float sx, float sy,
		float sw, float sh,
		float angle,
		float r, float g, float b, float a
	) const;

	void Render(ID3D11DeviceContext* dc,
		float dx, float dy,
		float dz,
		float dw, float dh,
		float angle,
		float r, float g, float b, float a
	) const;


	const Microsoft::WRL::ComPtr<ID3D11Buffer>& GetVertexBuffer() const { return vertexBuffer; }

	const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& GetShaderResourceView() const { return shaderResourceView; }
	void SetShaderResourceView(const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, float texWidth, float texHeight);
	int GetTextureWidth() const { return  textureWidth; }

	int GetTextureHeight() const { return textureHeight; }

	void SetGlow(const DirectX::XMFLOAT3& color, float intensity)
	{
		currentGlowColor = color;
		currentGlowIntensity = intensity;
	}

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>			vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>			pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>			inputLayout;

	Microsoft::WRL::ComPtr<ID3D11Buffer>				vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	shaderResourceView;

	float textureWidth = 0;
	float textureHeight = 0;

	Microsoft::WRL::ComPtr<ID3D11PixelShader>           uiPixelShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer>                uiConstantBuffer;

	struct UIConstants
	{
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT4 glowColor;
		float glowIntensity;
		float padding[3];
	};

	DirectX::XMFLOAT4 currentColor = { 1, 1, 1, 1 };
	DirectX::XMFLOAT3 currentGlowColor = { 0, 0, 0 };
	float currentGlowIntensity = 0.0f;



};
