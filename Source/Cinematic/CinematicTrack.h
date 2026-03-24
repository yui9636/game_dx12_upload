#pragma once
#include "CinematicCurve.h"
#include <string>
#include <memory>
#include <vector>
#include "Camera/CameraController.h"
#include "Effect/EffectComponent.h"
#include "Effect/EffectManager.h"
#include "Message/Messenger.h"
#include "Message/MessageData.h"
#include <windows.h> 

namespace Cinematic
{
    enum class TrackType { Camera, Animation, Event, Effect }; 

    // ==========================================
    // ==========================================
    class Track
    {
    public:
        std::string name;
        bool isMuted = false;
        bool isLocked = false;

        virtual ~Track() = default;
        virtual TrackType GetType() const = 0;

        virtual void Evaluate(float time) = 0;

        virtual void Bind(void* target) = 0;

        virtual void Serialize(json& out) const {
            out["name"] = name;
            out["mute"] = isMuted;
            out["lock"] = isLocked;
            out["type"] = (int)GetType();
        }
        virtual void Deserialize(const json& in) {
            if (in.contains("name")) in.at("name").get_to(name);
            if (in.contains("mute")) in.at("mute").get_to(isMuted);
            if (in.contains("lock")) in.at("lock").get_to(isLocked);
        }
    };

    class CameraTrack : public Track
    {
    public:
        CameraController* targetController = nullptr;

        Curve<DirectX::XMFLOAT3> eyeCurve;
        Curve<DirectX::XMFLOAT3> focusCurve;
        Curve<float> fovCurve;

        TrackType GetType() const override { return TrackType::Camera; }

        void Bind(void* target) override {
            targetController = static_cast<CameraController*>(target);
        }

        void Evaluate(float time) override
        {
            if (isMuted || !targetController) return;
            DirectX::XMFLOAT3 eye = eyeCurve.Evaluate(time);
            DirectX::XMFLOAT3 focus = focusCurve.Evaluate(time);
            // float fov = fovCurve.Evaluate(time);
            targetController->ResetCameraState(eye, focus);
        }

        void Serialize(json& out) const override {
            Track::Serialize(out);
            out["eye"] = eyeCurve.keys;
            out["focus"] = focusCurve.keys;
            out["fov"] = fovCurve.keys;
        }

        void Deserialize(const json& in) override {
            Track::Deserialize(in);
            if (in.contains("eye")) in.at("eye").get_to(eyeCurve.keys);
            if (in.contains("focus")) in.at("focus").get_to(focusCurve.keys);
            if (in.contains("fov")) in.at("fov").get_to(fovCurve.keys);
            eyeCurve.SortKeys();
            focusCurve.SortKeys();
            fovCurve.SortKeys();
        }
    };

    class AnimationTrack : public Track
    {
    public:
        struct Key {
            float time;
            float duration;
            int animIndex;
            std::string animName;
        };
        std::vector<Key> keys;
        int currentAnimIndex = -1;

        TrackType GetType() const override { return TrackType::Animation; }

        void Bind(void* target) override {}

        void Evaluate(float time) override
        {
            currentAnimIndex = -1;
            if (isMuted || keys.empty()) return;

            for (const auto& key : keys)
            {
                if (time >= key.time && time < (key.time + key.duration))
                {
                    currentAnimIndex = key.animIndex;
                    break;
                }
            }
        }

        void AddKey(float time, int index, const std::string& name, float defaultDuration = 2.0f)
        {
            keys.push_back({ time, defaultDuration, index, name });
            SortKeys();
        }

        void SortKeys() {
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }

        void Serialize(json& out) const override {
            Track::Serialize(out);
            std::vector<json> kArray;
            for (const auto& k : keys) {
                kArray.push_back({ {"t", k.time}, {"d", k.duration}, {"idx", k.animIndex}, {"n", k.animName} });
            }
            out["keys"] = kArray;
        }

        void Deserialize(const json& in) override {
            Track::Deserialize(in);
            keys.clear();
            if (in.contains("keys")) {
                for (const auto& j : in["keys"]) {
                    keys.push_back({
                        j.value("t", 0.0f),
                        j.value("d", 2.0f),
                        j.value("idx", -1),
                        j.value("n", "")
                        });
                }
            }
            SortKeys();
        }
    };

    // ==========================================
    // ==========================================
    class EffectTrack : public Track
    {
    public:
        struct Key {
            float time;
            float duration;
            std::string effectName;
            std::string boneName;

            DirectX::XMFLOAT3 offsetPos = { 0,0,0 };
            DirectX::XMFLOAT3 offsetRot = { 0,0,0 };
            DirectX::XMFLOAT3 offsetScale = { 1,1,1 };

            std::weak_ptr<EffectInstance> activeInstance;
        };

        std::vector<Key> keys;
        EffectComponent* targetComponent = nullptr;

        TrackType GetType() const override { return TrackType::Effect; }

        void Bind(void* target) override {
            if (auto actor = static_cast<Actor*>(target)) {
                auto comp = actor->GetComponent<EffectComponent>();
                targetComponent = comp.get();
            }
        }

        void Evaluate(float time) override
        {
            if (isMuted || !targetComponent) return;

            for (auto& key : keys)
            {
                bool isInside = (time >= key.time && time < (key.time + key.duration));

                auto instance = key.activeInstance.lock();

                if (isInside)
                {
                    if (!instance)
                    {
                        key.activeInstance = targetComponent->Play(
                            key.effectName,
                            key.boneName,
                            key.offsetPos,
                            key.offsetRot,
                            key.offsetScale,
                            false
                        );
                        instance = key.activeInstance.lock();

                       
                    }

                    if (instance)
                    {
                        float relativeTime = time - key.time;

                        instance->age = relativeTime;
                        instance->lifeTime = key.duration;

                        targetComponent->SetEffectTransform(
                            key.activeInstance,
                            key.offsetPos,
                            key.offsetRot,
                            key.offsetScale 
                        );

                    }
                }
                else
                {
                    if (instance)
                    {
                        targetComponent->Stop(key.activeInstance);
                        key.activeInstance.reset();
                    }
                }
            }
        }

        void Serialize(json& out) const override {
            Track::Serialize(out);
            std::vector<json> kArray;
            for (const auto& k : keys) {
                json j;
                j["t"] = k.time;
                j["d"] = k.duration;
                j["n"] = k.effectName;
                j["b"] = k.boneName;
                j["p"] = { k.offsetPos.x, k.offsetPos.y, k.offsetPos.z };
                j["r"] = { k.offsetRot.x, k.offsetRot.y, k.offsetRot.z };
                j["s"] = { k.offsetScale.x, k.offsetScale.y, k.offsetScale.z };
                kArray.push_back(j);
            }
            out["keys"] = kArray;
        }

        void Deserialize(const json& in) override {
            Track::Deserialize(in);
            keys.clear();
            if (in.contains("keys")) {
                for (const auto& j : in["keys"]) {
                    Key k;
                    k.time = j.value("t", 0.0f);
                    k.duration = j.value("d", 2.0f);
                    k.effectName = j.value("n", "");
                    k.boneName = j.value("b", "");

                    if (j.contains("p")) { auto v = j["p"]; k.offsetPos = { v[0], v[1], v[2] }; }
                    if (j.contains("r")) { auto v = j["r"]; k.offsetRot = { v[0], v[1], v[2] }; }
                    if (j.contains("s")) { auto v = j["s"]; k.offsetScale = { v[0], v[1], v[2] }; }

                    keys.push_back(k);
                }
            }
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }

        void AddKey(float time, const std::string& effectName, float duration = 2.0f) {
            Key k;
            k.time = time;
            k.duration = duration;
            k.effectName = effectName;
            keys.push_back(k);
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.time < b.time; });
        }
    };

    // ==========================================
    // ==========================================
    class EventTrack : public Track
    {
    public:
        struct Key {
            float time;
            std::string eventName;
            bool fired = false;
        };

        std::vector<Key> keys;
        float lastEvaluateTime = -1.0f;

        TrackType GetType() const override { return TrackType::Event; }

        void Bind(void* target) override {}

        void Evaluate(float time) override
        {
            if (isMuted) return;

            if (time < lastEvaluateTime)
            {
                for (auto& key : keys) key.fired = false;
            }

            for (auto& key : keys)
            {
                if (!key.fired && time >= key.time)
                {
                    MessageData::CINEMATIC_EVENT_TRIGGER_DATA data;
                    data.eventName = key.eventName;

                    Messenger::Instance().SendData(MessageData::CINEMATIC_EVENT_TRIGGER, &data);

                    key.fired = true;

                    char buf[256];
                    sprintf_s(buf, "[Sequencer] Event Fired: %s at %.2f\n", key.eventName.c_str(), time);
                    OutputDebugStringA(buf);
                }
            }

            lastEvaluateTime = time;
        }

        void AddKey(float time, const std::string& name)
        {
            Key k;
            k.time = time;
            k.eventName = name;
            k.fired = false;
            keys.push_back(k);

            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }

        void Serialize(json& out) const override {
            Track::Serialize(out);
            std::vector<json> kArray;
            for (const auto& k : keys) {
                json j;
                j["t"] = k.time;
                j["n"] = k.eventName;
                kArray.push_back(j);
            }
            out["keys"] = kArray;
        }

        void Deserialize(const json& in) override {
            Track::Deserialize(in);
            keys.clear();
            if (in.contains("keys")) {
                for (const auto& j : in["keys"]) {
                    Key k;
                    k.time = j.value("t", 0.0f);
                    k.eventName = j.value("n", "");
                    k.fired = false;
                    keys.push_back(k);
                }
            }
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }
    };

}
