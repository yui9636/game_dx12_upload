#pragma once

// ビットマップフォント描画クラスの宣言。
// FNTファイルとテクスチャページを読み込み、2D / 3D 文字列をRHI経由で描画する。

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "RHI/IBuffer.h"
#include "RHI/IShader.h"
#include "RHI/IState.h"

// 描画コマンドを発行するためのRHIコマンドリスト。
class ICommandList;

// シェーダやバッファなどのRHIリソースを作成するファクトリ。
class IResourceFactory;

// フォントページ画像を保持するRHIテクスチャ。
class ITexture;

// BMFont形式の情報を元に、文字ごとの矩形を生成して描画するクラス。
class Font
{
public:
    // FNTファイルを読み込み、文字描画に必要なシェーダ・バッファ・テクスチャ情報を初期化する。
    Font(IResourceFactory* factory, const char* filename, int maxSpriteCount = 2048);

    // RHIリソースはスマートポインタで管理しているため、既定の破棄処理でよい。
    virtual ~Font() = default;

    // フォントデータと描画リソースの初期化に成功しているかを返す。
    bool IsValid() const { return m_isValid; }

    // 2D文字描画の開始処理。画面サイズを保持し、頂点作成用の一時状態を初期化する。
    void Begin(ICommandList* commandList, float viewportWidth, float viewportHeight);

    // 2Dスクリーン座標で文字列を描画キューに積む。
    void Draw(float x, float y, const wchar_t* string);

    // 3D空間上に文字列を描画キューへ積む。world/view/projectionで最終位置を決める。
    void Draw3D(DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, const wchar_t* string);

    // Beginから積まれた文字頂点をGPUへ送り、テクスチャページごとに描画を発行する。
    void End(ICommandList* commandList);

    // 指定した文字列を現在のフォント情報で描いた場合のおおよその幅を返す。
    float GetTextWidth(const wchar_t* string) const;

    // 文字の描画倍率を設定する。
    void SetScale(float x, float y) { m_scaleX = x; m_scaleY = y; }

    // 文字全体に乗算する色を設定する。
    void SetColor(const DirectX::XMFLOAT4& color) { m_fontColor = color; }

    // SDFフォントのしきい値とぼかし幅を設定する。
    void SetSDFParams(float threshold = 0.5f, float softness = 0.5f);

private:
    // 文字1頂点分の描画データ。
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    // SDFフォント描画用にピクセルシェーダへ渡す定数。
    struct SDFData
    {
        DirectX::XMFLOAT4 Color;
        float Threshold;
        float Softness;
        float Padding[2];
    };

    // 3D描画時に頂点シェーダへ渡す行列定数。
    struct CBMatrix
    {
        DirectX::XMFLOAT4X4 World;
        DirectX::XMFLOAT4X4 View;
        DirectX::XMFLOAT4X4 Projection;
    };

    // FNTファイルから読み込んだ1文字分のメトリクス情報。
    struct CharacterInfo
    {
        // 未登録文字を表す特殊コード。
        static const uint16_t NonCode = 0;

        // 文字列終端を表す特殊コード。
        static const uint16_t EndCode = 0xFFFF;

        // 改行を表す特殊コード。
        static const uint16_t ReturnCode = 0xFFFE;

        // タブを表す特殊コード。
        static const uint16_t TabCode = 0xFFFD;

        // 半角スペースを表す特殊コード。
        static const uint16_t SpaceCode = 0xFFFC;

        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        float xoffset = 0.0f;
        float yoffset = 0.0f;
        float xadvance = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        int page = 0;
        bool ascii = false;
    };

    // 同じフォントページテクスチャを使う描画範囲。
    struct Subset
    {
        ITexture* texture = nullptr;
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
    };

    // FNTファイルを解析し、文字情報とフォントページテクスチャを読み込む。
    bool LoadFontData(IResourceFactory* factory, const char* filename);

    // 1文字分の矩形頂点を現在の頂点バッファ作成領域へ追加する。
    void AddGlyphQuad(float x, float y, const CharacterInfo& info, bool ndc2D);

    // フォントページが切り替わったとき、新しい描画範囲を記録する。
    void PushSubsetIfNeeded(int page);

    // フォント描画用の頂点シェーダ。
    std::unique_ptr<IShader> m_vertexShader;
    // フォント描画用のピクセルシェーダ。
    std::unique_ptr<IShader> m_pixelShader;
    // 頂点構造とシェーダ入力を対応させる入力レイアウト。
    std::unique_ptr<IInputLayout> m_inputLayout;
    // 文字矩形の頂点をGPUへ渡す頂点バッファ。
    std::unique_ptr<IBuffer> m_vertexBuffer;
    // 各文字矩形を2三角形で描くためのインデックスバッファ。
    std::unique_ptr<IBuffer> m_indexBuffer;
    // SDF描画パラメータを渡す定数バッファ。
    std::unique_ptr<IBuffer> m_sdfConstantBuffer;
    // 3D文字描画用の行列を渡す定数バッファ。
    std::unique_ptr<IBuffer> m_matrixBuffer;

    // フォントページごとのテクスチャ配列。
    std::vector<std::shared_ptr<ITexture>> m_textures;
    // 実際に描画可能な文字情報の配列。
    std::vector<CharacterInfo> m_characterInfos;
    // Unicodeコードからm_characterInfosの添字へ変換するテーブル。
    std::vector<uint16_t> m_characterIndices;
    // テクスチャページごとにまとめた描画範囲。
    std::vector<Subset> m_subsets;
    // CPU側で一時的に組み立てる文字頂点配列。
    std::vector<Vertex> m_vertices;

    // 次に頂点を書き込む位置。
    Vertex* m_currentVertex = nullptr;
    // 今フレームで使用するインデックス数。
    uint32_t m_currentIndexCount = 0;
    // 現在追加中のフォントページ番号。
    int m_currentPage = -1;
    // 1回のBegin/Endで描ける最大文字数。
    int m_maxSpriteCount = 0;

    // FNTに記録されたフォントページ幅。
    float m_fontWidth = 0.0f;
    // 1行分の高さ。
    float m_fontHeight = 0.0f;
    // フォントページテクスチャ数。
    int m_textureCount = 0;
    // 読み込んだ文字情報数。
    int m_characterCount = 0;

    // 2D描画時の画面幅。
    float m_screenWidth = 0.0f;
    // 2D描画時の画面高さ。
    float m_screenHeight = 0.0f;
    // X方向の文字倍率。
    float m_scaleX = 1.0f;
    // Y方向の文字倍率。
    float m_scaleY = 1.0f;
    // 描画時に文字へ乗算する色。
    DirectX::XMFLOAT4 m_fontColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // SDFの輪郭判定しきい値。
    float m_sdfThreshold = 0.5f;
    // SDFの輪郭ぼかし幅。
    float m_sdfSoftness = 0.5f;

    // 現在の描画が3D文字描画かどうか。
    bool m_is3DMode = false;
    // 初期化が成功し、描画可能な状態かどうか。
    bool m_isValid = false;

    // 3D描画時に使用する現在のワールド行列。
    DirectX::XMFLOAT4X4 m_currentWorld;
    // 3D描画時に使用する現在のビュー行列。
    DirectX::XMFLOAT4X4 m_currentView;
    // 3D描画時に使用する現在のプロジェクション行列。
    DirectX::XMFLOAT4X4 m_currentProj;
};
