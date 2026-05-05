#pragma once
#include <memory>

class ITexture;
class Model;
class MaterialAsset;
class OffscreenRenderer;

// マテリアルプレビュー用の小さなレンダリング環境を管理するクラス。
// オフスクリーン描画で球モデルへマテリアルを適用し、エディタ表示用のプレビュー画像を生成します。
class MaterialPreviewStudio {
public:
    // シングルトンインスタンスを返します。
    static MaterialPreviewStudio& Instance();

    // プレビュー描画に使う OffscreenRenderer と必要な RT / Depth / 球モデルを準備します。
    void Initialize(OffscreenRenderer* offscreen);

    // 指定マテリアルのプレビュー更新を予約します。
    void RequestPreview(MaterialAsset* material);

    // GPU が空いているタイミングで、予約済みプレビューを実際に描画します。
    void PumpPreview();

    // エディタ UI に表示するプレビュー用テクスチャを返します。
    ITexture* GetPreviewTexture() const { return m_previewTexture.get(); }

    // プレビュー描画に必要なリソースがそろっているかを返します。
    bool IsReady() const;

    // プレビューの再描画要求が残っているかを返します。
    bool IsDirty() const { return m_dirty; }

private:
    // シングルトン専用のため外部生成を禁止します。
    MaterialPreviewStudio() = default;

    // 内部リソースを破棄します。
    ~MaterialPreviewStudio();

    // 予約されているマテリアルを球モデルへ適用し、プレビュー RT に描画します。
    void ExecuteRender();

    // プレビュー画像の一辺の解像度です。
    static constexpr int PREVIEW_SIZE = 256;

    // プレビュー描画に使う共有オフスクリーンレンダラです。
    OffscreenRenderer* m_offscreen = nullptr;

    // プレビュー結果を保持するカラー RT です。
    // エディタ UI から SRV として参照されます。
    std::shared_ptr<ITexture> m_previewTexture;

    // プレビュー描画で使う深度バッファです。
    std::unique_ptr<ITexture> m_previewDepth;

    // マテリアル確認用に描画する球モデルです。
    std::shared_ptr<Model> m_sphereModel;

    // 次にプレビューへ反映するマテリアルです。
    MaterialAsset* m_pendingMaterial = nullptr;

    // プレビュー再描画が必要かどうかのフラグです。
    bool m_dirty = false;
};
