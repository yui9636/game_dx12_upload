#pragma once
// シネマティックで使う各種トラックを定義するヘッダー。
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

// カメラ・アニメーション・エフェクト・イベントのタイムライン要素をまとめる。
namespace Cinematic
{
    // トラックの種類。保存時にも整数値として使われる。
    enum class TrackType { Camera, Animation, Event, Effect }; 

    // 全トラックに共通する基底クラス。
    class Track
    {
    public:
        // エディタ上に表示するトラック名。
        std::string name;
        // true のとき、このトラックは評価しない。
        bool isMuted = false;
        // true のとき、編集ロック状態として扱う。
        bool isLocked = false;

        virtual ~Track() = default;
        // 派生トラックの種類を返す。
        virtual TrackType GetType() const = 0;

        // 指定時間の状態を対象へ反映する。
        virtual void Evaluate(float time) = 0;

        // このトラックが操作する対象を接続する。
        virtual void Bind(void* target) = 0;

        // 共通項目を JSON へ保存する。
        virtual void Serialize(json& out) const {
            out["name"] = name;
            out["mute"] = isMuted;
            out["lock"] = isLocked;
            out["type"] = (int)GetType();
        }

        // 共通項目を JSON から読み込む。
        virtual void Deserialize(const json& in) {
            if (in.contains("name")) in.at("name").get_to(name);
            if (in.contains("mute")) in.at("mute").get_to(isMuted);
            if (in.contains("lock")) in.at("lock").get_to(isLocked);
        }
    };

    // カメラの位置・注視点・FOV を保持するトラック。
    class CameraTrack : public Track
    {
    public:
        // カメラ位置のカーブ。
        Curve<DirectX::XMFLOAT3> eyeCurve;
        // カメラ注視点のカーブ。
        Curve<DirectX::XMFLOAT3> focusCurve;
        // 視野角のカーブ。
        Curve<float> fovCurve;

        // カメラトラックであることを返す。
        TrackType GetType() const override { return TrackType::Camera; }

        // 現在はカメラ制御対象を直接持たないため何もしない。
        void Bind(void* target) override {}

        // 現在は編集データを保持するだけで、実カメラへの反映は無効化されている。
        void Evaluate(float time) override
        {
            if (isMuted) return;
            // 旧 CameraController 再生は削除済み。
            // 現在の CameraTrack は作成済みデータの保持のみを担当する。
        }

        // カメラ用カーブを JSON へ保存する。
        void Serialize(json& out) const override {
            Track::Serialize(out);
            out["eye"] = eyeCurve.keys;
            out["focus"] = focusCurve.keys;
            out["fov"] = fovCurve.keys;
        }

        // JSON からカメラ用カーブを復元する。
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

    // 指定時間範囲でアニメーション番号を上書きするトラック。
    class AnimationTrack : public Track
    {
    public:
        // アニメーション再生区間を表すキー。
        struct Key {
            // 開始時間。
            float time;
            // 再生区間の長さ。
            float duration;
            // 再生するアニメーション番号。
            int animIndex;
            // エディタ表示用のアニメーション名。
            std::string animName;
        };

        // アニメーションキー一覧。
        std::vector<Key> keys;
        // 現在時間で有効なアニメーション番号。-1 はなし。
        int currentAnimIndex = -1;

        // アニメーショントラックであることを返す。
        TrackType GetType() const override { return TrackType::Animation; }

        // 現在は対象を直接保持しないため何もしない。
        void Bind(void* target) override {}

        // 現在時間が含まれるキーを探し、有効なアニメーション番号を更新する。
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

        // アニメーションキーを追加する。
        void AddKey(float time, int index, const std::string& name, float defaultDuration = 2.0f)
        {
            keys.push_back({ time, defaultDuration, index, name });
            SortKeys();
        }

        // アニメーションキーを開始時間順に並べる。
        void SortKeys() {
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }

        // アニメーションキーを JSON へ保存する。
        void Serialize(json& out) const override {
            Track::Serialize(out);
            std::vector<json> kArray;
            for (const auto& k : keys) {
                kArray.push_back({ {"t", k.time}, {"d", k.duration}, {"idx", k.animIndex}, {"n", k.animName} });
            }
            out["keys"] = kArray;
        }

        // JSON からアニメーションキーを復元する。
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

    // 指定時間にエフェクトを再生し、必要ならボーンへ追従させるトラック。
    class EffectTrack : public Track
    {
    public:
        // エフェクト再生区間を表すキー。
        struct Key {
            // 開始時間。
            float time;
            // 再生区間の長さ。
            float duration;
            // 再生するエフェクトアセット名またはパス。
            std::string effectName;
            // 追従させるボーン名。空なら Actor のルートに追従する。
            std::string boneName;

            // ボーンまたは Actor からの位置オフセット。
            DirectX::XMFLOAT3 offsetPos = { 0,0,0 };
            // ボーンまたは Actor からの回転オフセット。
            DirectX::XMFLOAT3 offsetRot = { 0,0,0 };
            // エフェクトに掛けるスケール。
            DirectX::XMFLOAT3 offsetScale = { 1,1,1 };

            // 現在再生中のエフェクトハンドル。
            EffectHandle activeHandle;
        };

        // エフェクトキー一覧。
        std::vector<Key> keys;
        // エフェクトを追従させる対象 Actor。
        Actor* targetActor = nullptr;

        // エフェクトトラックであることを返す。
        TrackType GetType() const override { return TrackType::Effect; }

        // 対象 Actor を接続する。
        void Bind(void* target) override {
            targetActor = static_cast<Actor*>(target);
        }

        // 現在時間に応じてエフェクトの生成・時間同期・停止を行う。
        void Evaluate(float time) override
        {
            if (isMuted || !targetActor) return;

            // ボーン追従に必要なモデル情報と Actor のワールド行列を取得する。
            Model* model = targetActor->GetModelRaw();
            const auto& nodes = model ? model->GetNodes() : std::vector<Model::Node>();
            DirectX::XMMATRIX actorWorld = DirectX::XMLoadFloat4x4(&targetActor->GetTransform());

            for (auto& key : keys)
            {
                // 現在時間がこのエフェクトキーの有効区間内か調べる。
                bool isInside = (time >= key.time && time < (key.time + key.duration));
                const bool hasActiveHandle = EffectService::Instance().IsAlive(key.activeHandle);

                if (isInside)
                {
                    if (!hasActiveHandle)
                    {
                        // 区間に入った瞬間だけエフェクトを生成する。
                        EffectPlayDesc desc;
                        desc.assetPath = key.effectName;
                        desc.position = targetActor->GetPosition();
                        desc.loop = false;
                        desc.debugName = "Cinematic Effect";
                        key.activeHandle = EffectService::Instance().PlayWorld(desc);
                    }

                    if (key.activeHandle.IsValid())
                    {
                        // シーケンス時間をエフェクト内の相対時間へ変換して同期する。
                        float relativeTime = time - key.time;
                        EffectService::Instance().Seek(key.activeHandle, relativeTime, key.duration, false);

                        // ボーン指定があれば、そのボーンのワールド行列を追従先にする。
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

                        // スケール混入を避けるため、追従先行列の軸を正規化してから使用する。
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

                        // オフセット行列を作り、追従先行列へ掛け合わせる。
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
                        // 区間外に出たら再生中エフェクトを停止する。
                        EffectService::Instance().Stop(key.activeHandle, true);
                        key.activeHandle.Reset();
                    }
                }
            }
        }

        // エフェクトキーを JSON へ保存する。
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

        // JSON からエフェクトキーを復元する。
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

        // エフェクトキーを追加する。
        void AddKey(float time, const std::string& effectName, float duration = 2.0f) {
            Key k;
            k.time = time;
            k.duration = duration;
            k.effectName = effectName;
            keys.push_back(k);
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.time < b.time; });
        }
    };

    // 指定時間にメッセージイベントを発火するトラック。
    class EventTrack : public Track
    {
    public:
        // イベント発火タイミングを表すキー。
        struct Key {
            // イベントを発火する時間。
            float time;
            // Messenger へ送るイベント名。
            std::string eventName;
            // 同じキーを二重発火しないためのフラグ。
            bool fired = false;
        };

        // イベントキー一覧。
        std::vector<Key> keys;
        // 前回評価した時間。巻き戻し検出に使う。
        float lastEvaluateTime = -1.0f;

        // イベントトラックであることを返す。
        TrackType GetType() const override { return TrackType::Event; }

        // 現在は対象を直接保持しないため何もしない。
        void Bind(void* target) override {}

        // 現在時間を超えたイベントを一度だけ発火する。
        void Evaluate(float time) override
        {
            if (isMuted) return;

            // タイムラインが巻き戻ったら、イベント発火済み状態をリセットする。
            if (time < lastEvaluateTime)
            {
                for (auto& key : keys) key.fired = false;
            }

            for (auto& key : keys)
            {
                if (!key.fired && time >= key.time)
                {
                    // Messenger 経由でシネマティックイベントを通知する。
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

        // イベントキーを追加する。
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

        // イベントキーを JSON へ保存する。
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

        // JSON からイベントキーを復元する。
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
