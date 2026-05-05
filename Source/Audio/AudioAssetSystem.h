#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Audio/AudioClipAsset.h"

// 音声アセットのメタ情報を読み取り、キャッシュして管理するクラスです。
// 音声の実再生は AudioWorldSystem が担当し、このクラスはファイル情報の解決に集中します。
class AudioAssetSystem
{
public:
    // 指定されたパスが読み込み対象の音声拡張子かどうかを判定します。
    bool IsSupportedClipPath(const std::string& clipPath) const;

    // 音声クリップのメタ情報を取得します。
    // まだキャッシュに無い場合は、ファイルを調べてメタ情報を作成します。
    const AudioClipAsset* GetClip(const std::string& clipPath);

    // 音声クリップのメタ情報を取得します。
    // 取得に失敗した場合でも、最低限のパス情報を持つデフォルト値を返します。
    AudioClipAsset GetClipOrDefault(const std::string& clipPath);

    // 読み取り済みの音声メタ情報キャッシュを全て破棄します。
    void ClearCache();

    // 現在キャッシュされている音声クリップ数を返します。
    size_t GetCachedClipCount() const;

    // 現在キャッシュされている音声クリップ一覧を返します。
    std::vector<AudioClipAsset> GetCachedClips() const;

private:
    // 指定された音声ファイルを調べ、AudioClipAsset のメタ情報を構築します。
    AudioClipAsset BuildClipMetadata(const std::string& clipPath) const;

    // 正規化済みパスをキーにした音声メタ情報キャッシュです。
    std::unordered_map<std::string, AudioClipAsset> m_clipCache;
};
