#pragma once

// フォントリソース管理と文字描画ヘルパーの宣言。
// 実行時描画用Fontと、エディタプレビュー用ImFontをまとめて管理する。
#include <map>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Font.h"

// RHI経由で文字描画を行うためのコマンドリスト。
class ICommandList;

// Font生成に使うRHIリソースファクトリ。
class IResourceFactory;

// ImGuiのエディタプレビュー用フォント。
struct ImFont;

// 文字列を描画するときの横方向揃え。
enum class FontAlign
{
    Left,
    Center,
    Right
};

// Fontインスタンスのキャッシュ、描画、エディタ用フォント読み込みを担当するシングルトン。
class FontManager
{
private:
    // singleton専用なので外部から生成できないようにする。
    FontManager() = default;

    // singleton専用なので外部から破棄できないようにする。
    ~FontManager() = default;

public:
    // FontManagerの唯一のインスタンスを返す。
    static FontManager& Instance()
    {
        static FontManager instance;
        return instance;
    }

    // 読み込み済みフォントのキャッシュをすべて破棄する。
    void Clear();

    // DX11互換呼び出し用。内部ではRHIファクトリ版のLoadへ委譲する。
    void Load(ID3D11Device* device, const std::string& key, const char* filename);

    // 指定キーでフォントを読み込み、キャッシュへ登録する。
    void Load(IResourceFactory* factory, const std::string& key, const char* filename);

    // 指定キーのフォントを取得する。存在しない場合はnullptrを返す。
    std::shared_ptr<Font> Get(const std::string& key);

    // ランタイム描画で使用するビューポートサイズを設定する。
    void SetRuntimeViewport(float width, float height);

    // ランタイム描画用ビューポートサイズを未設定状態へ戻す。
    void ClearRuntimeViewport();

    // 文字列描画ヘルパー。

    // DX11コンテキストを使って、色・倍率・揃え指定ありの2D文字列を描画する。
    void DrawFormat(
        ID3D11DeviceContext* dc,
        const std::string& key,
        float x, float y,
        const DirectX::XMFLOAT4& color,
        float scale,
        FontAlign align,
        const wchar_t* format,
        ...
    );

    // RHIコマンドリストを使って、色・倍率・揃え指定ありの2D文字列を描画する。
    void DrawFormat(
        ICommandList* commandList,
        const std::string& key,
        float x, float y,
        const DirectX::XMFLOAT4& color,
        float scale,
        FontAlign align,
        const wchar_t* format,
        ...
    );

    // DX11コンテキストを使って、既定色・既定倍率の2D文字列を描画する。
    void DrawFormat(
        ID3D11DeviceContext* dc,
        const std::string& key,
        float x, float y,
        const wchar_t* format,
        ...
    );

    // RHIコマンドリストを使って、既定色・既定倍率の2D文字列を描画する。
    void DrawFormat(
        ICommandList* commandList,
        const std::string& key,
        float x, float y,
        const wchar_t* format,
        ...
    );

    // DX11コンテキストを使って、3D空間上に文字列を描画する。
    void DrawFormat3D(
        ID3D11DeviceContext* dc,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const std::string& key,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& rotation,
        float scale,
        const DirectX::XMFLOAT4& color,
        FontAlign align,
        const wchar_t* format,
        ...
    );

    // RHIコマンドリストを使って、3D空間上に文字列を描画する。
    void DrawFormat3D(
        ICommandList* commandList,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const std::string& key,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& rotation,
        float scale,
        const DirectX::XMFLOAT4& color,
        FontAlign align,
        const wchar_t* format,
        ...
    );

    // エディタプレビュー用にTTFフォントの読み込みを予約する。
    void QueueEditorPreviewFont(const std::string& assetPath, float previewSize = 48.0f);

    // 読み込み済みのエディタプレビュー用ImFontを取得する。
    ImFont* GetEditorPreviewFont(const std::string& assetPath) const;

    // 読み込み予約されたエディタプレビュー用フォントをImGuiフォントアトラスへ追加する。
    void ProcessEditorPreviewFonts();

    // エディタプレビュー用フォントのキャッシュと失敗履歴を破棄する。
    void ClearEditorPreviewFonts();




private:
    // 指定キーのフォントを取得し、無ければ既定フォントを読み込む。
    std::shared_ptr<Font> GetOrLoadDefault(const std::string& key);

    // ランタイム描画用Fontのキャッシュ。
    std::map<std::string, std::shared_ptr<Font>> fonts;

    // エディタプレビュー用に読み込んだImGuiフォント。
    std::unordered_map<std::string, ImFont*> m_editorPreviewFonts;

    // 読み込みに失敗したエディタプレビューフォントのパス集合。
    std::unordered_set<std::string> m_editorPreviewFontFailures;

    // 次のProcessEditorPreviewFontsで読み込む予定のフォントパス集合。
    std::unordered_set<std::string> m_pendingEditorPreviewFonts;

    // エディタプレビュー用フォントの基準サイズ。
    float m_editorPreviewFontSize = 48.0f;
};
