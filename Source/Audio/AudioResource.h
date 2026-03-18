#pragma once

#include <vector>
#include <string>
#include <Windows.h>
#include <xaudio2.h> // WAVEFORMATEXのために必要

// オーディオリソース (WAVデータのメモリ保持担当)
class AudioResource
{
public:
    AudioResource(const char* filename);
    ~AudioResource();

    // データ取得
    const BYTE* GetAudioData() const { return data.data(); }

    // データサイズ取得
    UINT32 GetAudioBytes() const { return static_cast<UINT32>(data.size()); }

    // WAVEフォーマット取得
    const WAVEFORMATEX& GetWaveFormat() const { return wfx; }

private:
    // RIFFヘッダ
    struct Riff
    {
        UINT32  tag;    // 'RIFF'
        UINT32  size;
        UINT32  type;   // 'WAVE'
    };

    // チャンク
    struct Chunk
    {
        UINT32  tag;
        UINT32  size;
    };

    // fmt チャンク
    struct Fmt
    {
        UINT16  fmtId;
        UINT16  channel;
        UINT32  sampleRate;
        UINT32  transRate;
        UINT16  blockSize;
        UINT16  quantumBits;
    };

    std::vector<BYTE> data; // 生データ
    WAVEFORMATEX      wfx = { 0 };
};