#pragma once
#include "Component/Component.h"
#include <string>
#include <memory>

class AudioSource; // 前方宣言

class BGMComponent : public Component
{
public:
    const char* GetName() const override { return "BGM"; }

    void Start() override;     // ゲーム開始時に再生
    void OnDestroy() override; // 削除時に停止

    void OnGUI() override;     // エディター設定
    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;

    // プレビュー機能
    void PlayPreview();
    void StopPreview();

private:
    std::string filePath;
    float volume = 0.5f;
    float pitch = 1.0f;
    bool loop = true;

    // 再生中の音源ハンドル
    std::shared_ptr<AudioSource> playingSource;
};