
#include <fstream>
#include "Sprite.h"
#include "System/Misc.h"
#include "GpuResourceUtils.h"

// コンストラクタ
Sprite::Sprite(ID3D11Device* device)
	: Sprite(device, nullptr)
{
}

// コンストラクタ
Sprite::Sprite(ID3D11Device* device, const char* filename)
{
	HRESULT hr = S_OK;

	// 頂点バッファの生成
	{
		// 頂点バッファを作成するための設定オプション
		D3D11_BUFFER_DESC buffer_desc = {};
		buffer_desc.ByteWidth = sizeof(Vertex) * 4;
		buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
		buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		// 頂点バッファオブジェクトの生成
		hr = device->CreateBuffer(&buffer_desc, nullptr, vertexBuffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	// 頂点シェーダー
	{
		// 入力レイアウト
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

	// ピクセルシェーダー
	{
		hr = GpuResourceUtils::LoadPixelShader(
			device,
			"Data/Shader/SpritePS.cso",
			pixelShader.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}



	// テクスチャの生成	
	if (filename != nullptr)
	{
		// テクスチャファイル読み込み
		D3D11_TEXTURE2D_DESC desc;
		hr = GpuResourceUtils::LoadTexture(device, filename, shaderResourceView.GetAddressOf(), &desc);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		textureWidth = static_cast<float>(desc.Width);
		textureHeight = static_cast<float>(desc.Height);
	}
	else
	{
		// ダミーテクスチャ生成
		D3D11_TEXTURE2D_DESC desc;
		hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF, shaderResourceView.GetAddressOf(),
			&desc);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		textureWidth = static_cast<float>(desc.Width);
		textureHeight = static_cast<float>(desc.Height);
	}


	// ★追加: UI専用リソースの初期化
	{
		// 専用ピクセルシェーダー (SpriteUI_PS.cso)
		// ※必ずコンパイルを通しておくこと
		GpuResourceUtils::LoadPixelShader(device,
			"Data/Shader/SpriteUI_PS.cso", 
			uiPixelShader.GetAddressOf());

		// 定数バッファ作成
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = sizeof(UIConstants); // 16byte倍数であること(48byte OK)
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		device->CreateBuffer(&cbDesc, nullptr, uiConstantBuffer.GetAddressOf());
	}



}

// 描画実行
void Sprite::Render(ID3D11DeviceContext* dc,
	float dx, float dy,					// 左上位置
	float dz,							// 奥行
	float dw, float dh,					// 幅、高さ
	float sx, float sy,					// 画像切り抜き位置
	float sw, float sh,					// 画像切り抜きサイズ
	float angle,						// 角度
	float r, float g, float b, float a	// 色
) const
{
	// 頂点座標
	DirectX::XMFLOAT2 positions[] = {
		DirectX::XMFLOAT2(dx,      dy),			// 左上
		DirectX::XMFLOAT2(dx + dw, dy),			// 右上
		DirectX::XMFLOAT2(dx,      dy + dh),	// 左下
		DirectX::XMFLOAT2(dx + dw, dy + dh),	// 右下
	};

	// テクスチャ座標
	DirectX::XMFLOAT2 texcoords[] = {
		DirectX::XMFLOAT2(sx,      sy),			// 左上
		DirectX::XMFLOAT2(sx + sw, sy),			// 右上
		DirectX::XMFLOAT2(sx,      sy + sh),	// 左下
		DirectX::XMFLOAT2(sx + sw, sy + sh),	// 右下
	};

	// スプライトの中心で回転させるために４頂点の中心位置が
	// 原点(0, 0)になるように一旦頂点を移動させる。
	float mx = dx + dw * 0.5f;
	float my = dy + dh * 0.5f;
	for (auto& p : positions)
	{
		p.x -= mx;
		p.y -= my;
	}

	// 頂点を回転させる
	float theta = DirectX::XMConvertToRadians(angle);
	float c = cosf(theta);
	float s = sinf(theta);
	for (auto& p : positions)
	{
		DirectX::XMFLOAT2 r = p;
		p.x = c * r.x + -s * r.y;
		p.y = s * r.x + c * r.y;
	}

	// 回転のために移動させた頂点を元の位置に戻す
	for (auto& p : positions)
	{
		p.x += mx;
		p.y += my;
	}

	// 現在設定されているビューポートからスクリーンサイズを取得する。
	D3D11_VIEWPORT viewport;
	UINT numViewports = 1;
	dc->RSGetViewports(&numViewports, &viewport);
	float screenWidth = viewport.Width;
	float screenHeight = viewport.Height;

	// スクリーン座標系からNDC座標系へ変換する。
	for (DirectX::XMFLOAT2& p : positions)
	{
		p.x = 2.0f * p.x / screenWidth - 1.0f;
		p.y = 1.0f - 2.0f * p.y / screenHeight;
	}

	// 頂点バッファの内容の編集を開始する。
	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	HRESULT hr = dc->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	// 頂点バッファの内容を編集
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

	// 頂点バッファの内容の編集を終了する。
	dc->Unmap(vertexBuffer.Get(), 0);

	if (uiConstantBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE mappedCB;
		if (SUCCEEDED(dc->Map(uiConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
		{
			UIConstants* data = static_cast<UIConstants*>(mappedCB.pData);

			// 現在のカラー (currentColor) をセット
			data->color = currentColor;

			// 発光パラメータをセット
			// HLSL側で colorGlow.rgb * colorGlow.a と計算するため、wに強度を入れます
			data->glowColor.x = currentGlowColor.x;
			data->glowColor.y = currentGlowColor.y;
			data->glowColor.z = currentGlowColor.z;
			data->glowColor.w = currentGlowIntensity;

			// 構造体のメンバーに合わせてセット (冗長ですが安全のため)
			data->glowIntensity = currentGlowIntensity;

			dc->Unmap(uiConstantBuffer.Get(), 0);
		}
	}

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
	dc->IASetInputLayout(inputLayout.Get());
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// 頂点シェーダー設定
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);

	// ★重要変更: UI用のピクセルシェーダーがあればそちらを使う
	if (uiPixelShader)
	{
		// 新しいシェーダーをセット
		dc->PSSetShader(uiPixelShader.Get(), nullptr, 0);

		// ★忘れがち: 定数バッファをスロット0 (b0) にセット
		dc->PSSetConstantBuffers(0, 1, uiConstantBuffer.GetAddressOf());
	}
	else
	{
		// 従来のシェーダー (フォールバック)
		dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	}

	// テクスチャ設定
	dc->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

	// 描画
	dc->Draw(4, 0);


}

// 描画実行（テクスチャ切り抜き指定なし）
void Sprite::Render(ID3D11DeviceContext* dc,
	float dx, float dy,					// 左上位置
	float dz,							// 奥行
	float dw, float dh,					// 幅、高さ
	float angle,						// 角度
	float r, float g, float b, float a	// 色
) const
{
	Render(dc, dx, dy, dz, dw, dh, 0, 0, textureWidth, textureHeight, angle, r, g, b, a);
}


// シェーダーリソースビューの設定
void Sprite::SetShaderResourceView(const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, float texWidth, float texHeight)
{
	shaderResourceView = srv;
	textureWidth = static_cast<int>(texWidth);
	textureHeight = static_cast<int>(texHeight);
}





