#pragma once
#include "CinematicCurve.h"
#include <string>
#include <memory>
#include <vector>
#include "Actor/Actor.h"
#include "EffectRuntime/EffectService.h"
#include "Message/Messenger.h"
#include "Message/MessageData.h"
#include "Model/Model.h"
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
        Curve<DirectX::XMFLOAT3> eyeCurve;
        Curve<DirectX::XMFLOAT3> focusCurve;
        Curve<float> fovCurve;

        TrackType GetType() const override { return TrackType::Camera; }

        void Bind(void* target) override {}

        void Evaluate(float time) override
        {
            if (isMuted) return;
            // Legacy CameraController playback was removed.
            // Cinematic camera tracks currently preserve authored data only.
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

            EffectHandle activeHandle;
        };

        std::vector<Key> keys;
        Actor* targetActor = nullptr;

        TrackType GetType() const override { return TrackType::Effect; }

        void Bind(void* target) override {
            targetActor = static_cast<Actor*>(target);
        }

        void Evaluate(float time) override
        {
            if (isMuted || !targetActor) return;

            Model* model = targetActor->GetModelRaw();
            const auto& nodes = model ? model->GetNodes() : std::vector<Model::Node>();
            DirectX::XMMATRIX actorWorld = DirectX::XMLoadFloat4x4(&targetActor->GetTransform());

            for (auto& key : keys)
            {
                bool isInside = (time >= key.time && time < (key.time + key.duration));
                const bool hasActiveHandle = EffectService::Instance().IsAlive(key.activeHandle);

                if (isInside)
                {
                    if (!hasActiveHandle)
                    {
                        EffectPlayDesc desc;
                        desc.assetPath = key.effectName;
                        desc.position = targetActor->GetPosition();
                        desc.loop = false;
                        desc.debugName = "Cinematic Effect";
                        key.activeHandle = EffectService::Instance().PlayWorld(desc);
                    }

                    if (key.activeHandle.IsValid())
                    {
                        float relativeTime = time - key.time;
                        EffectService::Instance().Seek(key.activeHandle, relativeTime, key.duration, false);

                        DirectX::XMMATRIX socketWorld = actorWorld;
                        if (!key.boneName.empty() && model)
                        {
                            int targetBoneIndex = -1;
                            for (size_t i = 0; i < nodes.size(); ++i) {
                                if (nodes[i].name == key.boneName) {
                                    targetBoneIndex = static_cast<int>(i);
                                    break;
                                }
                            }

                            if (targetBoneIndex >= 0 && targetBoneIndex < static_cast<int>(nodes.size())) {
                                const DirectX::XMMATRIX boneWorld = DirectX::XMLoadFloat4x4(&nodes[targetBoneIndex].worldTransform);
                                socketWorld = boneWorld * actorWorld;
                            }
                        }

                        DirectX::XMFLOAT4X4 socketMatrix;
                        DirectX::XMStoreFloat4x4(&socketMatrix, socketWorld);
                        DirectX::XMVECTOR ax = DirectX::XMVector3Normalize(DirectX::XMVectorSet(socketMatrix._11, socketMatrix._12, socketMatrix._13, 0));
                        DirectX::XMVECTOR ay = DirectX::XMVector3Normalize(DirectX::XMVectorSet(socketMatrix._21, socketMatrix._22, socketMatrix._23, 0));
                        DirectX::XMVECTOR az = DirectX::XMVector3Normalize(DirectX::XMVectorSet(socketMatrix._31, socketMatrix._32, socketMatrix._33, 0));
                        DirectX::XMVECTOR p = DirectX::XMVectorSet(socketMatrix._41, socketMatrix._42, socketMatrix._43, 1);

                        DirectX::XMMATRIX normalizedSocket = DirectX::XMMatrixIdentity();
                        normalizedSocket.r[0] = ax;
                        normalizedSocket.r[1] = ay;
                        normalizedSocket.r[2] = az;
                        normalizedSocket.r[3] = p;

                        const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(key.offsetScale.x, key.offsetScale.y, key.offsetScale.z);
                        const DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
                            DirectX::XMConvertToRadians(key.offsetRot.x),
                            DirectX::XMConvertToRadians(key.offsetRot.y),
                            DirectX::XMConvertToRadians(key.offsetRot.z));
                        const DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(
                            key.offsetPos.x,
                            key.offsetPos.y,
                            key.offsetPos.z);
                        const DirectX::XMMATRIX effectWorld = scaleMatrix * rotationMatrix * translationMatrix * normalizedSocket;

                        DirectX::XMFLOAT4X4 worldMatrix;
                        DirectX::XMStoreFloat4x4(&worldMatrix, effectWorld);
                        EffectService::Instance().SetWorldMatrix(key.activeHandle, worldMatrix);
                    }
                }
                else
                {
                    if (hasActiveHandle)
                    {
                        EffectService::Instance().Stop(key.activeHandle, true);
                        key.activeHandle.Reset();
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
