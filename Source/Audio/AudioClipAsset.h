#pragma once

#include <cstdint>
#include <string>

// 音声ファイル1つ分のメタ情報を保持する構造体です。
// 実際の再生データではなく、パス・長さ・チャンネル数などの管理情報を持ちます。
struct AudioClipAsset
{
    // 元の音声ファイルのパスです。
    std::string sourcePath;

    // インポート後、または実際に再生に使う音声ファイルのパスです。
    std::string importedPath;

    // true の場合はストリーミング再生向けとして扱います。
    bool streaming = false;

    // このクリップを再生するときの初期音量です。
    float defaultVolume = 1.0f;

    // このクリップを再生するときの初期ピッチです。
    float defaultPitch = 1.0f;

    // このクリップを標準でループ再生するかどうかです。
    bool defaultLoop = false;

    // 音声のチャンネル数です。
    uint32_t channelCount = 0;

    // 音声のサンプリングレートです。
    uint32_t sampleRate = 0;

    // 音声の長さです。単位は秒です。
    float lengthSec = 0.0f;

    // 音声ファイルのサイズです。単位は byte です。
    uint64_t fileSizeBytes = 0;

    // メタ情報の取得に成功しているかどうかです。
    bool valid = false;
};
