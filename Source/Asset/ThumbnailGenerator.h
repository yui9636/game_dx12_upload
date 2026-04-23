#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <functional>
#include <DirectXMath.h>
#include "Entity/Entity.h"
#include "Render/PreviewTexturePool.h"

class Registry;
class ITexture;
class Model;
class OffscreenRenderer;

// サムネイル生成要求 1 件ぶんの情報。
// モデル用かマテリアル用かを区別するためのフラグも持つ。
struct ThumbnailRequest {
    // サムネイル生成対象のアセットパス。
    std::string path;

    // true の時はマテリアル、false の時はモデルとして扱う。
    bool isMaterial = false;
};

// モデルやマテリアルのサムネイルを非同期気味に生成・管理するクラス。
// OffscreenRenderer を使って 1 フレームに 1 件ずつ描画し、キャッシュして再利用する。
class ThumbnailGenerator {
public:
    // singleton インスタンスを返す。
    static ThumbnailGenerator& Instance();

    // OffscreenRenderer を登録し、内部状態を初期化する。
    void Initialize(OffscreenRenderer* offscreen);

    // サムネイル生成機能が利用可能かどうかを返す。
    bool IsAvailable() const;

    // モデルサムネイル生成を要求する。
    void Request(const std::string& modelPath);

    // マテリアルサムネイル生成を要求する。
    void RequestMaterial(const std::string& matPath);

    // 指定パスのサムネイルキャッシュを無効化し、必要なら再生成要求を入れる。
    void Invalidate(const std::string& path);

    // pending queue から 1 件だけサムネイル生成を進める。
    void PumpOne();

    // 未処理の生成要求が残っているかどうかを返す。
    bool HasPending() const { return !m_pendingQueue.empty(); }

    // 指定パスに対応するキャッシュ済みサムネイルを取得する。
    // 無ければ nullptr を返す。
    std::shared_ptr<ITexture> Get(const std::string& path) const;

    // 現在画面に見えているパス集合を設定する。
    // PumpOne で可視アセットを優先するために使う。
    void SetVisiblePaths(const std::unordered_set<std::string>& paths);

private:
    // singleton 用なのでコンストラクタは private。
    ThumbnailGenerator() = default;

    // デストラクタ。
    ~ThumbnailGenerator();

    // 必要になった時だけ内部リソースを初期化する。
    bool LazyInitialize();

    // モデル全体が画面に収まるようにカメラ位置と ViewProjection を計算する。
    void SetupCamera(Model* model, DirectX::XMFLOAT4X4& outViewProj, DirectX::XMFLOAT3& outCamPos);

    // モデルファイルのサムネイルを生成する。
    std::shared_ptr<ITexture> GenerateTexture(const std::string& modelPath, std::shared_ptr<ITexture> target);

    // サムネイル生成用のモデルパスを必要に応じて解決する。
    std::string ResolvePreviewModelPath(const std::string& modelPath) const;

    // マテリアルアセットのサムネイルを生成する。
    std::shared_ptr<ITexture> GenerateMaterialTexture(const std::string& matPath, std::shared_ptr<ITexture> target);

    // 1 枚のサムネイルを offscreen へ描画する共通処理。
    void RenderThumbnail(ITexture* target, std::function<void()> setupAndDraw);

    // キャッシュ上限を超えた時、最も古いサムネイルを追い出す。
    void EvictOldest();

    // 保持するサムネイル最大数。
    static constexpr size_t MAX_CACHE = 128;

    // 1 枚のサムネイル解像度。
    static constexpr int THUMB_SIZE = 128;

    // サムネイル生成に使う offscreen 描画器。
    OffscreenRenderer* m_offscreen = nullptr;

    // マテリアルプレビュー用の共通球モデル。
    std::shared_ptr<Model> m_sphereModel;

    // サムネイル描画用テクスチャを再利用するプール。
    PreviewTexturePool m_texturePool;

    // パス文字列をキーにしたサムネイルキャッシュ本体。
    std::unordered_map<std::string, std::shared_ptr<ITexture>> m_cache;

    // LRU 追い出し順管理用のパス一覧。
    std::deque<std::string> m_cacheOrder;

    // 未処理のサムネイル生成要求キュー。
    std::deque<ThumbnailRequest> m_pendingQueue;

    // 重複要求を防ぐための pending セット。
    std::unordered_set<std::string> m_pendingSet;

    // 現在画面に見えているアセットパス集合。
    std::unordered_set<std::string> m_visiblePaths;

    // 遅延初期化が完了しているかどうか。
    bool m_initialized = false;

    // 利用不可警告を既に出したかどうか。
    bool m_loggedUnavailable = false;
};