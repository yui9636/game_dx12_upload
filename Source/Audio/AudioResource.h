#pragma once

#include <vector>
#include <string>
#include <Windows.h>
#include <xaudio2.h>

class AudioResource
{
public:
    AudioResource(const char* filename);
    ~AudioResource();

    const BYTE* GetAudioData() const { return data.data(); }

    UINT32 GetAudioBytes() const { return static_cast<UINT32>(data.size()); }

    const WAVEFORMATEX& GetWaveFormat() const { return wfx; }

private:
    struct Riff
    {
        UINT32  tag;    // 'RIFF'
        UINT32  size;
        UINT32  type;   // 'WAVE'
    };

    struct Chunk
    {
        UINT32  tag;
        UINT32  size;
    };

    struct Fmt
    {
        UINT16  fmtId;
        UINT16  channel;
        UINT32  sampleRate;
        UINT32  transRate;
        UINT16  blockSize;
        UINT16  quantumBits;
    };

    std::vector<BYTE> data;
    WAVEFORMATEX      wfx = { 0 };
};
