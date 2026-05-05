#pragma once
// 複数のシネマティックトラックをまとめるシーケンス定義。
#include "CinematicTrack.h"
#include "JSONManager.h"
#include <vector>
#include <memory>

// シネマティック再生用のシーケンスをまとめる名前空間。
namespace Cinematic
{
    // カメラ・アニメーション・エフェクトなどのトラックを時間軸で管理する。
    class Sequence
    {
    public:
        // シーケンス名。
        std::string name = "New Sequence";
        // シーケンス全体の長さ。
        float duration = 10.0f;
        // 編集・表示用のフレームレート。
        float frameRate = 60.0f;

        // このシーケンスに含まれるトラック一覧。
        std::vector<std::shared_ptr<Track>> tracks;

        // 指定時間で全トラックを評価する。
        void Evaluate(float time)
        {
            for (auto& track : tracks)
            {
                if (track) track->Evaluate(time);
            }
        }

        // 指定型のトラックを生成してシーケンスに追加する。
        template<typename T>
        std::shared_ptr<T> AddTrack(const std::string& trackName)
        {
            auto newTrack = std::make_shared<T>();
            newTrack->name = trackName;
            tracks.push_back(newTrack);
            return newTrack;
        }

        // シーケンス内容を JSON ファイルに保存する。
        void SaveToFile(const std::string& filePath) const
        {
            JSONManager manager(filePath);

            manager.Set("name", name);
            manager.Set("duration", duration);

            // 各トラックに自分自身を JSON 化させる。
            std::vector<json> tracksJson;
            for (const auto& track : tracks)
            {
                json t;
                track->Serialize(t);
                tracksJson.push_back(t);
            }
            manager.Set("tracks", tracksJson);

            manager.Save();
        }

        // JSON ファイルからシーケンス内容を読み込む。
        void LoadFromFile(const std::string& filePath)
        {
            JSONManager manager(filePath);

            name = manager.Get<std::string>("name", "New Sequence");
            duration = manager.Get<float>("duration", 10.0f);

            // 読み込み前に既存トラックを破棄する。
            tracks.clear();

            try {
                std::vector<json> tracksJson = manager.Get<std::vector<json>>("tracks");

                for (const auto& j : tracksJson)
                {
                    // 保存された種類に応じて具体的なトラック型を復元する。
                    int type = j.value("type", -1);
                    std::shared_ptr<Track> newTrack = nullptr;

                    
                    if (type == (int)TrackType::Camera)
                    {
                        newTrack = std::make_shared<CameraTrack>();
                    }
                    else if (type == (int)TrackType::Animation)
                    {
                        newTrack = std::make_shared<AnimationTrack>();
                    }
                    else if (type == (int)TrackType::Effect)
                    {
                        newTrack = std::make_shared<EffectTrack>();
                    }

                    if (newTrack)
                    {
                        // トラック固有のデータを読み込み、一覧に追加する。
                        newTrack->Deserialize(j);
                        tracks.push_back(newTrack);
                    }
                }
            }
            catch (...) {
                // 壊れた JSON や古い形式の場合は、読み込める範囲だけで継続する。
            }
        }
    };
}
