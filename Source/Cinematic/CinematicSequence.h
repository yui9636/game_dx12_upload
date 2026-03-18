#pragma once
#include "CinematicTrack.h"
#include "JSONManager.h"
#include <vector>
#include <memory>

namespace Cinematic
{
    class Sequence
    {
    public:
        std::string name = "New Sequence";
        float duration = 10.0f;
        float frameRate = 60.0f;

        // トラックリスト
        std::vector<std::shared_ptr<Track>> tracks;

        // 全トラックを評価
        void Evaluate(float time)
        {
            for (auto& track : tracks)
            {
                if (track) track->Evaluate(time);
            }
        }

        // トラック追加ヘルパー
        template<typename T>
        std::shared_ptr<T> AddTrack(const std::string& trackName)
        {
            auto newTrack = std::make_shared<T>();
            newTrack->name = trackName;
            tracks.push_back(newTrack);
            return newTrack;
        }

        // JSON保存
        // (Track::Serialize が EffectTrack にも対応しているため変更不要)
        void SaveToFile(const std::string& filePath) const
        {
            JSONManager manager(filePath);

            manager.Set("name", name);
            manager.Set("duration", duration);

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

        // JSON読み込み
        void LoadFromFile(const std::string& filePath)
        {
            JSONManager manager(filePath);

            name = manager.Get<std::string>("name", "New Sequence");
            duration = manager.Get<float>("duration", 10.0f);

            tracks.clear();

            try {
                // トラックリストの復元
                std::vector<json> tracksJson = manager.Get<std::vector<json>>("tracks");

                for (const auto& j : tracksJson)
                {
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
                    else if (type == (int)TrackType::Effect) // ★追加
                    {
                        newTrack = std::make_shared<EffectTrack>();
                    }

                    if (newTrack)
                    {
                        newTrack->Deserialize(j);
                        tracks.push_back(newTrack);
                    }
                }
            }
            catch (...) {
                // ファイルが空、または形式が古い場合は無視
            }
        }
    };
}