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

        std::vector<std::shared_ptr<Track>> tracks;

        void Evaluate(float time)
        {
            for (auto& track : tracks)
            {
                if (track) track->Evaluate(time);
            }
        }

        template<typename T>
        std::shared_ptr<T> AddTrack(const std::string& trackName)
        {
            auto newTrack = std::make_shared<T>();
            newTrack->name = trackName;
            tracks.push_back(newTrack);
            return newTrack;
        }

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

        void LoadFromFile(const std::string& filePath)
        {
            JSONManager manager(filePath);

            name = manager.Get<std::string>("name", "New Sequence");
            duration = manager.Get<float>("duration", 10.0f);

            tracks.clear();

            try {
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
                    else if (type == (int)TrackType::Effect)
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
            }
        }
    };
}
