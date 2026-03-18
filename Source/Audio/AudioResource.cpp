#include "AudioResource.h"
#include <cstdio>
#include <cassert>

// WAVEタグ作成マクロ
#define MAKE_WAVE_TAG_VALUE(c1, c2, c3, c4)  ( c1 | (c2<<8) | (c3<<16) | (c4<<24) )

AudioResource::AudioResource(const char* filename)
{
    FILE* fp = nullptr;
    errno_t error = fopen_s(&fp, filename, "rb");
    if (error != 0)
    {
        // 読み込み失敗時はアサートやログ出力
        char buf[256];
        sprintf_s(buf, "WAV File not found: %s", filename);
        OutputDebugStringA(buf);
        return;
    }

    // ファイルサイズ取得
    fseek(fp, 0, SEEK_END);
    size_t size = static_cast<size_t>(ftell(fp));
    fseek(fp, 0, SEEK_SET);

    size_t readBytes = 0;
    Riff riff = {};

    // RIFFヘッダ
    fread(&riff, sizeof(riff), 1, fp);
    readBytes += sizeof(riff);

    if (riff.tag != MAKE_WAVE_TAG_VALUE('R', 'I', 'F', 'F') ||
        riff.type != MAKE_WAVE_TAG_VALUE('W', 'A', 'V', 'E'))
    {
        fclose(fp);
        return;
    }

    Fmt fmt = {};

    while (size > readBytes)
    {
        Chunk chunk = {};
        if (fread(&chunk, sizeof(chunk), 1, fp) < 1) break;
        readBytes += sizeof(chunk);

        // 'fmt '
        if (chunk.tag == MAKE_WAVE_TAG_VALUE('f', 'm', 't', ' '))
        {
            fread(&fmt, sizeof(fmt), 1, fp);
            readBytes += sizeof(fmt);

            // 拡張領域スキップ
            if (chunk.size > sizeof(Fmt))
            {
                size_t skip = chunk.size - sizeof(Fmt);
                fseek(fp, (long)skip, SEEK_CUR);
                readBytes += skip;
            }
        }
        // 'data'
        else if (chunk.tag == MAKE_WAVE_TAG_VALUE('d', 'a', 't', 'a'))
        {
            data.resize(chunk.size);
            fread(data.data(), chunk.size, 1, fp);
            readBytes += chunk.size;

            // 8bit変換 (unsigned -> signed)
            if (fmt.quantumBits == 8)
            {
                for (size_t i = 0; i < data.size(); ++i)
                {
                    data[i] -= 128;
                }
            }
        }
        // その他チャンクはスキップ
        else
        {
            fseek(fp, chunk.size, SEEK_CUR);
            readBytes += chunk.size;
        }
    }

    fclose(fp);

    // WAVEFORMATEX設定
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = fmt.channel;
    wfx.nSamplesPerSec = fmt.sampleRate;
    wfx.wBitsPerSample = fmt.quantumBits;
    wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    wfx.cbSize = 0;
}

AudioResource::~AudioResource()
{
}