#include "System/Misc.h"
#include "Font.h"
#include "GpuResourceUtils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

using namespace DirectX;

// フォント描画に必要なシェーダ・バッファ・状態オブジェクトを作成し、
// .fnt ファイルを読み込んで文字情報とテクスチャを初期化する。
Font::Font(ID3D11Device* device, const char* filename, int maxSpriteCount)
{
    HRESULT hr = S_OK;

    // 頂点レイアウトを定義する。
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // フォント描画用シェーダを読み込む。
    GpuResourceUtils::LoadVertexShader(device, "Data\\Shader\\Font_VS.cso", inputElementDesc, ARRAYSIZE(inputElementDesc), inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
    GpuResourceUtils::LoadPixelShader(device, "Data\\Shader\\Font_PS.cso", pixelShader.GetAddressOf());

    // SDF パラメータ用定数バッファを作成する。
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SDFData);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&desc, nullptr, sdfConstantBuffer.GetAddressOf());
    }

    // αブレンド用ステートを作成する。
    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&desc, blendState.GetAddressOf());
    }

    // 深度無効の DepthStencilState を作成する。
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = FALSE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        device->CreateDepthStencilState(&desc, depthStencilState.GetAddressOf());
    }

    // カリング無しのラスタライザステートを作成する。
    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FrontCounterClockwise = TRUE;
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        device->CreateRasterizerState(&desc, rasterizerState.GetAddressOf());
    }

    // 線形サンプラを作成する。
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&desc, samplerState.GetAddressOf());
    }

    // ワールド・ビュー・射影行列用の定数バッファを作成する。
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(CBMatrix);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&desc, nullptr, matrixBuffer.GetAddressOf());
    }

    // 動的頂点バッファを作成する。
    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * maxSpriteCount * 4);
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bufferDesc, nullptr, vertexBuffer.GetAddressOf());
    }

    // インデックスバッファを作成する。
    // 1文字 = 4頂点 / 6インデックス の固定構造にしておく。
    {
        std::unique_ptr<UINT[]> indices = std::make_unique<UINT[]>(maxSpriteCount * 6);
        UINT* p = indices.get();
        for (int i = 0; i < maxSpriteCount * 4; i += 4) {
            p[0] = i + 0; p[1] = i + 1; p[2] = i + 2;
            p[3] = i + 2; p[4] = i + 1; p[5] = i + 3;
            p += 6;
        }

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * maxSpriteCount * 6);
        bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA initData = { indices.get(), 0, 0 };
        device->CreateBuffer(&bufferDesc, &initData, indexBuffer.GetAddressOf());
    }

    // ---------------------------------------------------
    // .fnt ファイルを読み込んで文字情報を構築する。
    // ---------------------------------------------------
    {
        FILE* fp = nullptr;
        fopen_s(&fp, filename, "rb");
        _ASSERT_EXPR_A(fp, "FNT File not found");

        // ファイル全体をメモリへ読み込む。
        fseek(fp, 0, SEEK_END);
        long fntSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        std::unique_ptr<char[]> fntData = std::make_unique<char[]>(fntSize + 1);
        fread(fntData.get(), fntSize, 1, fp);
        fntData[fntSize] = '\0';
        fclose(fp);

        // フォント画像への相対パス解決用にディレクトリ名を抜き出す。
        char dirname[256];
        _splitpath_s(filename, nullptr, 0, dirname, 256, nullptr, 0, nullptr, 0);

        // 文字索引テーブルを初期化する。
        characterInfos.resize(0xFFFF);
        characterIndices.resize(0xFFFF);
        memset(characterIndices.data(), 0, sizeof(WORD) * characterIndices.size());

        char* lineContext = nullptr;
        char* line = strtok_s(fntData.get(), "\r\n", &lineContext);

        int charInfoIndex = 1;
        float scaleW = 1.0f;
        float scaleH = 1.0f;

        while (line)
        {
            // common 行: フォント全体の基本情報を取得する。
            if (strncmp(line, "common", 6) == 0)
            {
                char* tokenCtx = nullptr;
                char* token = strtok_s(line, " ", &tokenCtx);
                int pages = 0;

                while (token)
                {
                    if (strncmp(token, "lineHeight=", 11) == 0) fontHeight = (float)atoi(token + 11);
                    else if (strncmp(token, "scaleW=", 7) == 0) scaleW = (float)atoi(token + 7);
                    else if (strncmp(token, "scaleH=", 7) == 0) scaleH = (float)atoi(token + 7);
                    else if (strncmp(token, "pages=", 6) == 0)  pages = atoi(token + 6);

                    token = strtok_s(nullptr, " ", &tokenCtx);
                }

                fontWidth = scaleW;
                textureCount = pages;
                shaderResourceViews.resize(pages);
            }
            // page 行: 各ページ画像を読み込む。
            else if (strncmp(line, "page", 4) == 0)
            {
                int id = 0;
                char fname[256] = {};

                char* pId = strstr(line, "id=");
                if (pId) id = atoi(pId + 3);

                char* pFile = strstr(line, "file=\"");
                if (pFile)
                {
                    pFile += 6;
                    char* pQuote = strchr(pFile, '\"');
                    if (pQuote)
                    {
                        *pQuote = '\0';
                        char fullpath[256];
                        _makepath_s(fullpath, 256, nullptr, dirname, pFile, nullptr);

                        hr = GpuResourceUtils::LoadTexture(device, fullpath, shaderResourceViews.at(id).GetAddressOf());
                        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
                    }
                }
            }
            // chars 行: 総文字数を取得する。
            else if (strncmp(line, "chars", 5) == 0)
            {
                char* pCount = strstr(line, "count=");
                if (pCount)
                {
                    int count = atoi(pCount + 6);
                    characterCount = count + 1;
                    characterInfos.resize(characterCount);
                }
            }
            // char 行: 各文字の矩形とオフセット情報を取り込む。
            else if (strncmp(line, "char", 4) == 0)
            {
                int id = 0, x = 0, y = 0, width = 0, height = 0;
                int xoffset = 0, yoffset = 0, xadvance = 0, page = 0;

                char* tokenCtx = nullptr;
                char* token = strtok_s(line, " ", &tokenCtx);

                while (token)
                {
                    if (strncmp(token, "id=", 3) == 0)       id = atoi(token + 3);
                    else if (strncmp(token, "x=", 2) == 0)   x = atoi(token + 2);
                    else if (strncmp(token, "y=", 2) == 0)   y = atoi(token + 2);
                    else if (strncmp(token, "width=", 6) == 0)  width = atoi(token + 6);
                    else if (strncmp(token, "height=", 7) == 0) height = atoi(token + 7);
                    else if (strncmp(token, "xoffset=", 8) == 0) xoffset = atoi(token + 8);
                    else if (strncmp(token, "yoffset=", 8) == 0) yoffset = atoi(token + 8);
                    else if (strncmp(token, "xadvance=", 9) == 0) xadvance = atoi(token + 9);
                    else if (strncmp(token, "page=", 5) == 0) page = atoi(token + 5);

                    token = strtok_s(nullptr, " ", &tokenCtx);
                }

                if (id > 0 && id < 0x10000 && charInfoIndex < characterInfos.size())
                {
                    characterIndices.at(id) = static_cast<WORD>(charInfoIndex);
                    CharacterInfo& info = characterInfos.at(charInfoIndex);

                    info.left = static_cast<float>(x) / scaleW;
                    info.top = static_cast<float>(y) / scaleH;
                    info.right = static_cast<float>(x + width) / scaleW;
                    info.bottom = static_cast<float>(y + height) / scaleH;
                    info.xoffset = static_cast<float>(xoffset);
                    info.yoffset = static_cast<float>(yoffset);
                    info.xadvance = static_cast<float>(xadvance);
                    info.width = static_cast<float>(width);
                    info.height = static_cast<float>(height);
                    info.page = page;
                    info.ascii = (id < 0x100);

                    charInfoIndex++;
                }
            }

            line = strtok_s(nullptr, "\r\n", &lineContext);
        }

        // 特殊コードを予約登録する。
        characterIndices.at(0x00) = CharacterInfo::EndCode;
        characterIndices.at(0x0a) = CharacterInfo::ReturnCode;
        characterIndices.at(0x09) = CharacterInfo::TabCode;
        characterIndices.at(0x20) = CharacterInfo::SpaceCode;
    }
}

// SDF フォント描画用のしきい値とぼかし量を設定する。
void Font::SetSDFParams(float threshold, float softness)
{
    sdfThreshold = threshold;
    sdfSoftness = softness;
}

// 描画開始。
// SDF 定数を更新し、頂点バッファを Map して書き込み開始状態にする。
void Font::Begin(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT viewport;
    UINT num_viewports = 1;
    context->RSGetViewports(&num_viewports, &viewport);
    screenWidth = viewport.Width;
    screenHeight = viewport.Height;

    // SDF パラメータを GPU へ反映する。
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(context->Map(sdfConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        SDFData* data = reinterpret_cast<SDFData*>(ms.pData);
        data->Color = fontColor;
        data->Threshold = sdfThreshold;
        data->Softness = sdfSoftness;
        context->Unmap(sdfConstantBuffer.Get(), 0);
    }

    // 頂点バッファを開いて書き込み開始位置を設定する。
    context->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    currentVertex = reinterpret_cast<Vertex*>(ms.pData);
    currentIndexCount = 0;
    currentPage = -1;
    subsets.clear();

    is3DMode = false;
}

// 2D テキストを描画キューへ積む。
// 実際の DrawIndexed は End でまとめて行う。
void Font::Draw(float x, float y, const wchar_t* string)
{
    size_t length = wcslen(string);
    float start_x = x;

    float space = 20.0f * scaleX;

    for (size_t i = 0; i < length; ++i)
    {
        WORD word = static_cast<WORD>(string[i]);
        if (word >= characterIndices.size()) continue;
        WORD code = characterIndices.at(word);

        // 特殊文字処理。
        if (code == CharacterInfo::EndCode) break;
        else if (code == CharacterInfo::ReturnCode) { x = start_x; y += fontHeight * scaleY; continue; }
        else if (code == CharacterInfo::TabCode) { x += space * 4; continue; }
        else if (code == CharacterInfo::SpaceCode) { x += space; continue; }

        if (code == 0) continue;

        const CharacterInfo& info = characterInfos.at(code);

        float positionX = x + info.xoffset * scaleX;
        float positionY = y + info.yoffset * scaleY;
        float width = info.width * scaleX;
        float height = info.height * scaleY;

        // 4頂点分の矩形を組み立てる。
        currentVertex[0].position = { positionX,         positionY,          0.0f };
        currentVertex[1].position = { positionX + width, positionY,          0.0f };
        currentVertex[2].position = { positionX,         positionY + height, 0.0f };
        currentVertex[3].position = { positionX + width, positionY + height, 0.0f };

        currentVertex[0].texcoord = { info.left,  info.top };
        currentVertex[1].texcoord = { info.right, info.top };
        currentVertex[2].texcoord = { info.left,  info.bottom };
        currentVertex[3].texcoord = { info.right, info.bottom };

        // 2D はスクリーン座標から NDC へ変換する。
        for (int j = 0; j < 4; ++j)
        {
            currentVertex[j].color = DirectX::XMFLOAT4(1, 1, 1, 1);

            currentVertex[j].position.x = 2.0f * currentVertex[j].position.x / screenWidth - 1.0f;
            currentVertex[j].position.y = 1.0f - 2.0f * currentVertex[j].position.y / screenHeight;
        }

        currentVertex += 4;
        x += info.xadvance * scaleX;

        // ページが切り替わったら新しい subset を作る。
        if (currentPage != info.page)
        {
            currentPage = info.page;
            Subset subset;
            subset.shaderResourceView = shaderResourceViews.at(info.page).Get();
            subset.startIndex = currentIndexCount;
            subset.indexCount = 0;
            subsets.emplace_back(subset);
        }

        currentIndexCount += 6;
    }
}

// 3D 空間上のテキストを描画キューへ積む。
void Font::Draw3D(DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, const wchar_t* string)
{
    is3DMode = true;

    XMStoreFloat4x4(&currentWorld, XMMatrixTranspose(world));
    XMStoreFloat4x4(&currentView, XMMatrixTranspose(view));
    XMStoreFloat4x4(&currentProj, XMMatrixTranspose(projection));

    size_t length = wcslen(string);
    float x = 0.0f;
    float y = 0.0f;
    float start_x = x;
    float space = 20.0f * scaleX;

    for (size_t i = 0; i < length; ++i)
    {
        WORD word = static_cast<WORD>(string[i]);
        if (word >= characterIndices.size()) continue;
        WORD code = characterIndices.at(word);

        // 特殊文字処理。
        if (code == CharacterInfo::EndCode) break;
        else if (code == CharacterInfo::ReturnCode) { x = start_x; y -= fontHeight * scaleY; continue; }
        else if (code == CharacterInfo::TabCode) { x += space * 4; continue; }
        else if (code == CharacterInfo::SpaceCode) { x += space; continue; }

        if (code == 0) continue;

        const CharacterInfo& info = characterInfos.at(code);

        float positionX = x + info.xoffset * scaleX;
        float positionY = y - info.yoffset * scaleY;

        float width = info.width * scaleX;
        float height = info.height * scaleY;

        // 3D 空間用の矩形を組み立てる。
        currentVertex[0].position = { positionX,         positionY,          0.0f };
        currentVertex[1].position = { positionX + width, positionY,          0.0f };
        currentVertex[2].position = { positionX,         positionY - height, 0.0f };
        currentVertex[3].position = { positionX + width, positionY - height, 0.0f };

        currentVertex[0].texcoord = { info.left,  info.top };
        currentVertex[1].texcoord = { info.right, info.top };
        currentVertex[2].texcoord = { info.left,  info.bottom };
        currentVertex[3].texcoord = { info.right, info.bottom };

        for (int j = 0; j < 4; ++j)
        {
            currentVertex[j].color = DirectX::XMFLOAT4(1, 1, 1, 1);
        }

        currentVertex += 4;
        x += info.xadvance * scaleX;

        // ページごとに subset を分ける。
        if (currentPage != info.page)
        {
            currentPage = info.page;
            Subset subset;
            subset.shaderResourceView = shaderResourceViews.at(info.page).Get();
            subset.startIndex = currentIndexCount;
            subset.indexCount = 0;
            subsets.emplace_back(subset);
        }

        currentIndexCount += 6;
    }
}

// 文字列の横幅を概算で返す。
float Font::GetTextWidth(const wchar_t* string)
{
    float width = 0.0f;
    size_t length = wcslen(string);

    for (size_t i = 0; i < length; ++i)
    {
        WORD word = static_cast<WORD>(string[i]);
        if (word >= characterIndices.size()) continue;
        WORD code = characterIndices.at(word);

        if (code == CharacterInfo::SpaceCode) { width += 20.0f; continue; }
        if (code == CharacterInfo::TabCode) { width += 80.0f; continue; }
        if (code == 0 || code >= CharacterInfo::ReturnCode) continue;

        const CharacterInfo& info = characterInfos.at(code);
        width += info.xadvance;
    }
    return width;
}

// 描画終了。
// 頂点バッファを Unmap し、subset ごとに SRV を切り替えながら DrawIndexed する。
void Font::End(ID3D11DeviceContext* context)
{
    context->Unmap(vertexBuffer.Get(), 0);

    // subset ごとの indexCount を確定する。
    if (!subsets.empty())
    {
        size_t size = subsets.size();
        for (size_t i = 1; i < size; ++i)
        {
            subsets.at(i - 1).indexCount = subsets.at(i).startIndex - subsets.at(i - 1).startIndex;
        }
        subsets.back().indexCount = currentIndexCount - subsets.back().startIndex;
    }

    // シェーダと入力レイアウトを設定する。
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
    context->IASetInputLayout(inputLayout.Get());

    // SDF 定数バッファを PS に渡す。
    context->PSSetConstantBuffers(0, 1, sdfConstantBuffer.GetAddressOf());

    // 2D / 3D に応じた行列を設定する。
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(context->Map(matrixBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        CBMatrix* data = reinterpret_cast<CBMatrix*>(ms.pData);

        if (is3DMode)
        {
            data->World = currentWorld;
            data->View = currentView;
            data->Projection = currentProj;
        }
        else
        {
            XMMATRIX I = XMMatrixIdentity();
            XMFLOAT4X4 identity;
            XMStoreFloat4x4(&identity, XMMatrixTranspose(I));

            data->World = identity;
            data->View = identity;
            data->Projection = identity;
        }

        context->Unmap(matrixBuffer.Get(), 0);
    }
    context->VSSetConstantBuffers(1, 1, matrixBuffer.GetAddressOf());

    // 描画ステートを設定する。
    const float blend_factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    context->OMSetBlendState(blendState.Get(), blend_factor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(depthStencilState.Get(), 0);
    context->RSSetState(rasterizerState.Get());
    context->PSSetSamplers(0, 1, samplerState.GetAddressOf());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // subset 単位でテクスチャを切り替えて描画する。
    for (const auto& subset : subsets)
    {
        if (subset.shaderResourceView)
        {
            context->PSSetShaderResources(0, 1, &subset.shaderResourceView);
            context->DrawIndexed(subset.indexCount, subset.startIndex, 0);
        }
    }
}