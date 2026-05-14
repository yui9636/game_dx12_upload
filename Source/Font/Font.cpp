#include "Font.h"

// Fontクラスの実装。
// BMFont形式のFNTを読み込み、文字ごとに矩形頂点を生成して描画する。

#include "Console/Logger.h"
#include "Graphics.h"
#include "RHI/IBuffer.h"
#include "RHI/ICommandList.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IShader.h"
#include "RenderContext/RenderState.h"
#include "System/PathResolver.h"
#include "System/ResourceManager.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>

using namespace DirectX;

namespace
{
    // フォント描画用のコンパイル済み頂点シェーダ。
    constexpr const char* kFontVS = "Data\\Shader\\Font_VS.cso";

    // フォント描画用のコンパイル済みピクセルシェーダ。
    constexpr const char* kFontPS = "Data\\Shader\\Font_PS.cso";

    // フォントページ番号がテクスチャ配列の範囲内か確認する。
    bool IsValidPageIndex(int page, size_t count)
    {
        return page >= 0 && static_cast<size_t>(page) < count;
    }
}

// フォント描画に必要なシェーダ、入力レイアウト、バッファ、FNTデータを初期化する。
Font::Font(IResourceFactory* factory, const char* filename, int maxSpriteCount)
    : m_maxSpriteCount((std::max)(1, maxSpriteCount))
{
    // 3D描画用の行列は安全な初期値として単位行列にしておく。
    XMStoreFloat4x4(&m_currentWorld, XMMatrixIdentity());
    XMStoreFloat4x4(&m_currentView, XMMatrixIdentity());
    XMStoreFloat4x4(&m_currentProj, XMMatrixIdentity());

    // ファクトリまたはファイル名が無効なら初期化を中断する。
    if (!factory || !filename || filename[0] == '\0') {
        LOG_WARN("[Font] Invalid font initialization request.");
        return;
    }

    // フォント用シェーダをRHIファクトリ経由で読み込む。
    m_vertexShader = factory->CreateShader(ShaderType::Vertex, kFontVS);
    m_pixelShader = factory->CreateShader(ShaderType::Pixel, kFontPS);
    if (!m_vertexShader || !m_pixelShader) {
        LOG_WARN("[Font] Failed to create font shaders.");
        return;
    }

    // Font::Vertexのメモリ配置をシェーダ入力へ対応させる。
    const InputLayoutElement inputElements[] = {
        { "POSITION", 0, TextureFormat::R32G32B32_FLOAT,    0, static_cast<uint32_t>(offsetof(Vertex, position)) },
        { "COLOR",    0, TextureFormat::R32G32B32A32_FLOAT, 0, static_cast<uint32_t>(offsetof(Vertex, color)) },
        { "TEXCOORD", 0, TextureFormat::R32G32_FLOAT,       0, static_cast<uint32_t>(offsetof(Vertex, texcoord)) },
    };
    const InputLayoutDesc inputLayoutDesc{
        inputElements,
        static_cast<uint32_t>(sizeof(inputElements) / sizeof(inputElements[0]))
    };
    m_inputLayout = factory->CreateInputLayout(inputLayoutDesc, m_vertexShader.get());
    if (!m_inputLayout) {
        LOG_WARN("[Font] Failed to create font input layout.");
        return;
    }

    // 1文字につき4頂点を使うため、最大文字数分のCPU側頂点配列を確保する。
    m_vertices.resize(static_cast<size_t>(m_maxSpriteCount) * 4u);
    m_vertexBuffer = factory->CreateBuffer(
        static_cast<uint32_t>(sizeof(Vertex) * m_vertices.size()),
        BufferType::Vertex);

    // 各文字矩形を2三角形で描くため、インデックスは1文字6個用意する。
    std::vector<uint32_t> indices(static_cast<size_t>(m_maxSpriteCount) * 6u);
    uint32_t* indexWrite = indices.data();
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_maxSpriteCount) * 4u; i += 4u) {
        indexWrite[0] = i + 0u;
        indexWrite[1] = i + 1u;
        indexWrite[2] = i + 2u;
        indexWrite[3] = i + 2u;
        indexWrite[4] = i + 1u;
        indexWrite[5] = i + 3u;
        indexWrite += 6;
    }
    m_indexBuffer = factory->CreateBuffer(
        static_cast<uint32_t>(sizeof(uint32_t) * indices.size()),
        BufferType::Index,
        indices.data());

    // SDF設定と3D行列をシェーダへ渡す定数バッファを作成する。
    m_sdfConstantBuffer = factory->CreateBuffer(sizeof(SDFData), BufferType::Constant);
    m_matrixBuffer = factory->CreateBuffer(sizeof(CBMatrix), BufferType::Constant);

    if (!m_vertexBuffer || !m_indexBuffer || !m_sdfConstantBuffer || !m_matrixBuffer) {
        LOG_WARN("[Font] Failed to create font buffers.");
        return;
    }

    // 最後にFNTファイルを読み込み、描画可能かどうかを確定する。
    m_isValid = LoadFontData(factory, filename);
}

// FNTファイルを読み込み、文字メトリクスとフォントページテクスチャを構築する。
bool Font::LoadFontData(IResourceFactory*, const char* filename)
{
    // エンジンのパス解決ルールに従い、実ファイルパスへ変換する。
    const std::string resolvedFilename = PathResolver::Resolve(filename);

    // FNTファイルをバイナリとして読み込む。
    FILE* fp = nullptr;
    fopen_s(&fp, resolvedFilename.c_str(), "rb");
    if (!fp) {
        LOG_WARN("[Font] FNT file not found: %s", resolvedFilename.c_str());
        return false;
    }

    fseek(fp, 0, SEEK_END);
    const long fntSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fntSize <= 0) {
        fclose(fp);
        LOG_WARN("[Font] Empty FNT file: %s", resolvedFilename.c_str());
        return false;
    }

    // strtok_sで安全に行分割できるよう、終端文字ぶん多く確保する。
    std::vector<char> fntData(static_cast<size_t>(fntSize) + 1u, '\0');
    fread(fntData.data(), static_cast<size_t>(fntSize), 1, fp);
    fclose(fp);

    // フォントページ画像はFNTファイルからの相対パスで指定される。
    const std::filesystem::path fontPath(resolvedFilename);
    const std::filesystem::path fontDir = fontPath.parent_path();

    // Unicodeの基本範囲を直接引けるよう、変換テーブルを初期化する。
    m_characterInfos.assign(0x10000, CharacterInfo{});
    m_characterIndices.assign(0x10000, CharacterInfo::NonCode);

    char* lineContext = nullptr;
    char* line = strtok_s(fntData.data(), "\r\n", &lineContext);

    int charInfoIndex = 1;
    float scaleW = 1.0f;
    float scaleH = 1.0f;

    while (line)
    {
        // common行にはフォント全体の高さ、テクスチャサイズ、ページ数が入っている。
        if (strncmp(line, "common", 6) == 0)
        {
            char* tokenCtx = nullptr;
            char* token = strtok_s(line, " ", &tokenCtx);
            int pages = 0;

            while (token)
            {
                if (strncmp(token, "lineHeight=", 11) == 0) {
                    m_fontHeight = static_cast<float>(atoi(token + 11));
                } else if (strncmp(token, "scaleW=", 7) == 0) {
                    scaleW = static_cast<float>(atoi(token + 7));
                } else if (strncmp(token, "scaleH=", 7) == 0) {
                    scaleH = static_cast<float>(atoi(token + 7));
                } else if (strncmp(token, "pages=", 6) == 0) {
                    pages = atoi(token + 6);
                }

                token = strtok_s(nullptr, " ", &tokenCtx);
            }

            m_fontWidth = scaleW;
            m_textureCount = pages;
            m_textures.assign(static_cast<size_t>((std::max)(0, pages)), nullptr);
        }
        // page行にはフォントページ画像のファイル名が入っている。
        else if (strncmp(line, "page", 4) == 0)
        {
            int id = 0;
            char* pId = strstr(line, "id=");
            if (pId) {
                id = atoi(pId + 3);
            }

            char* pFile = strstr(line, "file=\"");
            if (pFile && IsValidPageIndex(id, m_textures.size()))
            {
                pFile += 6;
                char* pQuote = strchr(pFile, '\"');
                if (pQuote)
                {
                    *pQuote = '\0';
                    std::filesystem::path pagePath = fontDir / pFile;
                    pagePath = pagePath.lexically_normal();
                    m_textures[static_cast<size_t>(id)] = ResourceManager::Instance().GetTexture(pagePath.string());
                    if (!m_textures[static_cast<size_t>(id)]) {
                        LOG_WARN("[Font] Failed to load font texture page: %s", pagePath.string().c_str());
                    }
                }
            }
        }
        // chars行には登録文字数が入っている。
        else if (strncmp(line, "chars", 5) == 0)
        {
            char* pCount = strstr(line, "count=");
            if (pCount)
            {
                const int count = atoi(pCount + 6);
                m_characterCount = count + 1;
                m_characterInfos.assign(static_cast<size_t>((std::max)(1, m_characterCount)), CharacterInfo{});
            }
        }
        // char行には1文字分の位置、サイズ、送り幅、ページ番号が入っている。
        else if (strncmp(line, "char", 4) == 0)
        {
            int id = 0;
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            int xoffset = 0;
            int yoffset = 0;
            int xadvance = 0;
            int page = 0;

            char* tokenCtx = nullptr;
            char* token = strtok_s(line, " ", &tokenCtx);

            while (token)
            {
                if (strncmp(token, "id=", 3) == 0) {
                    id = atoi(token + 3);
                } else if (strncmp(token, "x=", 2) == 0) {
                    x = atoi(token + 2);
                } else if (strncmp(token, "y=", 2) == 0) {
                    y = atoi(token + 2);
                } else if (strncmp(token, "width=", 6) == 0) {
                    width = atoi(token + 6);
                } else if (strncmp(token, "height=", 7) == 0) {
                    height = atoi(token + 7);
                } else if (strncmp(token, "xoffset=", 8) == 0) {
                    xoffset = atoi(token + 8);
                } else if (strncmp(token, "yoffset=", 8) == 0) {
                    yoffset = atoi(token + 8);
                } else if (strncmp(token, "xadvance=", 9) == 0) {
                    xadvance = atoi(token + 9);
                } else if (strncmp(token, "page=", 5) == 0) {
                    page = atoi(token + 5);
                }

                token = strtok_s(nullptr, " ", &tokenCtx);
            }

            // 有効な文字IDだけ変換テーブルへ登録する。
            if (id > 0 && id < static_cast<int>(m_characterIndices.size()) &&
                charInfoIndex < static_cast<int>(m_characterInfos.size()))
            {
                m_characterIndices[static_cast<size_t>(id)] = static_cast<uint16_t>(charInfoIndex);
                CharacterInfo& info = m_characterInfos[static_cast<size_t>(charInfoIndex)];

                info.left = scaleW != 0.0f ? static_cast<float>(x) / scaleW : 0.0f;
                info.top = scaleH != 0.0f ? static_cast<float>(y) / scaleH : 0.0f;
                info.right = scaleW != 0.0f ? static_cast<float>(x + width) / scaleW : 0.0f;
                info.bottom = scaleH != 0.0f ? static_cast<float>(y + height) / scaleH : 0.0f;
                info.xoffset = static_cast<float>(xoffset);
                info.yoffset = static_cast<float>(yoffset);
                info.xadvance = static_cast<float>(xadvance);
                info.width = static_cast<float>(width);
                info.height = static_cast<float>(height);
                info.page = page;
                info.ascii = (id < 0x100);

                ++charInfoIndex;
            }
        }

        line = strtok_s(nullptr, "\r\n", &lineContext);
    }

    // 制御文字は通常の文字とは別の特殊コードとして登録する。
    m_characterIndices[0x00] = CharacterInfo::EndCode;
    m_characterIndices[0x0a] = CharacterInfo::ReturnCode;
    m_characterIndices[0x09] = CharacterInfo::TabCode;
    m_characterIndices[0x20] = CharacterInfo::SpaceCode;

    return !m_textures.empty() &&
        std::any_of(m_textures.begin(), m_textures.end(), [](const std::shared_ptr<ITexture>& texture) {
            return texture != nullptr;
        });
}

// SDF文字の輪郭しきい値とぼかし幅を設定する。
void Font::SetSDFParams(float threshold, float softness)
{
    m_sdfThreshold = threshold;
    m_sdfSoftness = softness;
}

// 文字描画の開始処理。頂点作成状態とページ分割情報を初期化する。
void Font::Begin(ICommandList*, float viewportWidth, float viewportHeight)
{
    if (!m_isValid || m_vertices.empty()) {
        return;
    }

    m_screenWidth = viewportWidth;
    m_screenHeight = viewportHeight;
    m_currentVertex = m_vertices.data();
    m_currentIndexCount = 0;
    m_currentPage = -1;
    m_subsets.clear();
    m_is3DMode = false;
}

// 使用するフォントページが変わったとき、描画範囲を分割して記録する。
void Font::PushSubsetIfNeeded(int page)
{
    if (m_currentPage == page) {
        return;
    }

    m_currentPage = page;
    Subset subset;
    if (IsValidPageIndex(page, m_textures.size()) && m_textures[static_cast<size_t>(page)]) {
        subset.texture = m_textures[static_cast<size_t>(page)].get();
    }
    subset.startIndex = m_currentIndexCount;
    subset.indexCount = 0;
    m_subsets.emplace_back(subset);
}

// 1文字分の矩形を4頂点としてCPU側頂点配列へ追加する。
void Font::AddGlyphQuad(float x, float y, const CharacterInfo& info, bool ndc2D)
{
    if (!m_currentVertex) {
        return;
    }
    if (m_currentIndexCount + 6u > static_cast<uint32_t>(m_maxSpriteCount) * 6u) {
        return;
    }

    // FNTのオフセットと現在のスケールを反映して、描画位置とサイズを求める。
    const float positionX = x + info.xoffset * m_scaleX;
    const float positionY = ndc2D ? y + info.yoffset * m_scaleY : y - info.yoffset * m_scaleY;
    const float width = info.width * m_scaleX;
    const float height = info.height * m_scaleY;

    // 2D描画では左上原点のスクリーン座標として矩形を組む。
    if (ndc2D)
    {
        m_currentVertex[0].position = { positionX,         positionY,          0.0f };
        m_currentVertex[1].position = { positionX + width, positionY,          0.0f };
        m_currentVertex[2].position = { positionX,         positionY + height, 0.0f };
        m_currentVertex[3].position = { positionX + width, positionY + height, 0.0f };
    }
    else
    {
        // 3D描画ではローカル空間上の文字板として矩形を組む。
        m_currentVertex[0].position = { positionX,         positionY,          0.0f };
        m_currentVertex[1].position = { positionX + width, positionY,          0.0f };
        m_currentVertex[2].position = { positionX,         positionY - height, 0.0f };
        m_currentVertex[3].position = { positionX + width, positionY - height, 0.0f };
    }

    // FNTに記録されたUV範囲を頂点へ設定する。
    m_currentVertex[0].texcoord = { info.left,  info.top };
    m_currentVertex[1].texcoord = { info.right, info.top };
    m_currentVertex[2].texcoord = { info.left,  info.bottom };
    m_currentVertex[3].texcoord = { info.right, info.bottom };

    for (int j = 0; j < 4; ++j)
    {
        m_currentVertex[j].color = DirectX::XMFLOAT4(1, 1, 1, 1);

        // 2D描画時はCPU側でスクリーン座標からNDCへ変換しておく。
        if (ndc2D && m_screenWidth > 0.0f && m_screenHeight > 0.0f)
        {
            m_currentVertex[j].position.x = 2.0f * m_currentVertex[j].position.x / m_screenWidth - 1.0f;
            m_currentVertex[j].position.y = 1.0f - 2.0f * m_currentVertex[j].position.y / m_screenHeight;
        }
    }

    m_currentVertex += 4;
    m_currentIndexCount += 6;
}

// 2Dスクリーン座標で文字列を描画キューへ積む。
void Font::Draw(float x, float y, const wchar_t* string)
{
    if (!m_isValid || !string) {
        return;
    }

    // 改行・タブ・スペースを処理しながら、文字ごとの矩形を追加する。
    const size_t length = wcslen(string);
    const float startX = x;
    const float space = 20.0f * m_scaleX;

    for (size_t i = 0; i < length; ++i)
    {
        const uint16_t word = static_cast<uint16_t>(string[i]);
        if (word >= m_characterIndices.size()) {
            continue;
        }

        const uint16_t code = m_characterIndices[word];
        if (code == CharacterInfo::EndCode) {
            break;
        }
        if (code == CharacterInfo::ReturnCode) {
            x = startX;
            y += m_fontHeight * m_scaleY;
            continue;
        }
        if (code == CharacterInfo::TabCode) {
            x += space * 4.0f;
            continue;
        }
        if (code == CharacterInfo::SpaceCode) {
            x += space;
            continue;
        }
        if (code == CharacterInfo::NonCode || code >= m_characterInfos.size()) {
            continue;
        }

        const CharacterInfo& info = m_characterInfos[code];
        PushSubsetIfNeeded(info.page);
        AddGlyphQuad(x, y, info, true);
        x += info.xadvance * m_scaleX;
    }
}

// 3D空間上に文字列を描画キューへ積む。
void Font::Draw3D(DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, const wchar_t* string)
{
    if (!m_isValid || !string) {
        return;
    }

    // 3D描画では行列を頂点シェーダで使うため、転置して保持する。
    m_is3DMode = true;
    XMStoreFloat4x4(&m_currentWorld, XMMatrixTranspose(world));
    XMStoreFloat4x4(&m_currentView, XMMatrixTranspose(view));
    XMStoreFloat4x4(&m_currentProj, XMMatrixTranspose(projection));

    // 改行・タブ・スペースを処理しながら、文字ごとの矩形を追加する。
    const size_t length = wcslen(string);
    float x = 0.0f;
    float y = 0.0f;
    const float startX = x;
    const float space = 20.0f * m_scaleX;

    for (size_t i = 0; i < length; ++i)
    {
        const uint16_t word = static_cast<uint16_t>(string[i]);
        if (word >= m_characterIndices.size()) {
            continue;
        }

        const uint16_t code = m_characterIndices[word];
        if (code == CharacterInfo::EndCode) {
            break;
        }
        if (code == CharacterInfo::ReturnCode) {
            x = startX;
            y -= m_fontHeight * m_scaleY;
            continue;
        }
        if (code == CharacterInfo::TabCode) {
            x += space * 4.0f;
            continue;
        }
        if (code == CharacterInfo::SpaceCode) {
            x += space;
            continue;
        }
        if (code == CharacterInfo::NonCode || code >= m_characterInfos.size()) {
            continue;
        }

        const CharacterInfo& info = m_characterInfos[code];
        PushSubsetIfNeeded(info.page);
        AddGlyphQuad(x, y, info, false);
        x += info.xadvance * m_scaleX;
    }
}

// 現在のフォント情報で文字列を描いた場合の横幅を計算する。
float Font::GetTextWidth(const wchar_t* string) const
{
    if (!string) {
        return 0.0f;
    }

    float width = 0.0f;
    // 改行・タブ・スペースを処理しながら、文字ごとの矩形を追加する。
    const size_t length = wcslen(string);

    for (size_t i = 0; i < length; ++i)
    {
        const uint16_t word = static_cast<uint16_t>(string[i]);
        if (word >= m_characterIndices.size()) {
            continue;
        }

        const uint16_t code = m_characterIndices[word];
        if (code == CharacterInfo::SpaceCode) {
            width += 20.0f;
            continue;
        }
        if (code == CharacterInfo::TabCode) {
            width += 80.0f;
            continue;
        }
        if (code == CharacterInfo::NonCode || code >= CharacterInfo::ReturnCode || code >= m_characterInfos.size()) {
            continue;
        }

        width += m_characterInfos[code].xadvance;
    }

    return width;
}

// Begin以降に作成した文字頂点をGPUへ送り、実際の描画を発行する。
void Font::End(ICommandList* commandList)
{
    if (!m_isValid || !commandList || m_currentIndexCount == 0) {
        m_currentVertex = nullptr;
        return;
    }

    // ページ切り替え位置から各Subsetの描画インデックス数を確定する。
    if (!m_subsets.empty())
    {
        const size_t size = m_subsets.size();
        for (size_t i = 1; i < size; ++i)
        {
            m_subsets[i - 1].indexCount = m_subsets[i].startIndex - m_subsets[i - 1].startIndex;
        }
        m_subsets.back().indexCount = m_currentIndexCount - m_subsets.back().startIndex;
    }

    // 今回使った頂点だけGPUバッファへ転送する。
    const uint32_t vertexCount = (m_currentIndexCount / 6u) * 4u;
    commandList->UpdateBuffer(m_vertexBuffer.get(), m_vertices.data(), vertexCount * static_cast<uint32_t>(sizeof(Vertex)));

    // 色とSDFパラメータをピクセルシェーダへ渡す。
    SDFData sdfData{};
    sdfData.Color = m_fontColor;
    sdfData.Threshold = m_sdfThreshold;
    sdfData.Softness = m_sdfSoftness;
    commandList->UpdateBuffer(m_sdfConstantBuffer.get(), &sdfData, sizeof(sdfData));

    // 2Dでは単位行列、3DではDraw3Dで保存した行列を使う。
    CBMatrix matrixData{};
    if (m_is3DMode)
    {
        matrixData.World = m_currentWorld;
        matrixData.View = m_currentView;
        matrixData.Projection = m_currentProj;
    }
    else
    {
        XMFLOAT4X4 identity;
        XMStoreFloat4x4(&identity, XMMatrixTranspose(XMMatrixIdentity()));
        matrixData.World = identity;
        matrixData.View = identity;
        matrixData.Projection = identity;
    }
    commandList->UpdateBuffer(m_matrixBuffer.get(), &matrixData, sizeof(matrixData));

    // フォント描画用のパイプライン状態を設定する。
    commandList->VSSetShader(m_vertexShader.get());
    commandList->PSSetShader(m_pixelShader.get());
    commandList->SetInputLayout(m_inputLayout.get());
    commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);
    commandList->SetVertexBuffer(0, m_vertexBuffer.get(), sizeof(Vertex), 0);
    commandList->SetIndexBuffer(m_indexBuffer.get(), IndexFormat::Uint32, 0);
    commandList->PSSetConstantBuffer(0, m_sdfConstantBuffer.get());
    commandList->VSSetConstantBuffer(1, m_matrixBuffer.get());

    // 文字は透過描画なので、深度なし・アルファブレンドで描画する。
    if (RenderState* renderState = Graphics::Instance().GetRenderState())
    {
        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        commandList->SetBlendState(renderState->GetBlendState(BlendState::Transparency), blendFactor, 0xFFFFFFFF);
        commandList->SetDepthStencilState(renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        commandList->SetRasterizerState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));
        commandList->PSSetSampler(0, renderState->GetSamplerState(SamplerState::LinearWrap));
    }

    // フォントページごとにテクスチャを切り替えて描画する。
    for (const auto& subset : m_subsets)
    {
        if (subset.texture && subset.indexCount > 0)
        {
            commandList->PSSetTexture(0, subset.texture);
            commandList->DrawIndexed(subset.indexCount, subset.startIndex, 0);
        }
    }

    commandList->PSSetTexture(0, nullptr);
    m_currentVertex = nullptr;
}
