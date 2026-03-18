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
    // トラックの種類
    enum class TrackType { Camera, Animation, Event, Effect }; 

    // ==========================================
    // トラック基底クラス
    // ==========================================
    class Track
    {
    public:
        std::string name;
        bool isMuted = false;
        bool isLocked = false;

        virtual ~Track() = default;
        virtual TrackType GetType() const = 0;

        // 評価関数：指定時間の状態を適用する
        virtual void Evaluate(float time) = 0;

        // 対象のバインド
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

    // ... (CameraTrack は変更なし) ...
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

    // ... (AnimationTrack は変更なし) ...
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
    // ★追加: エフェクト制御トラック
    // ==========================================
    class EffectTrack : public Track
    {
    public:
        // エフェクトキー（クリップ情報）
        struct Key {
            float time;             // 開始時間
            float duration;         // 再生期間
            std::string effectName; // ファイルパス
            std::string boneName;   // アタッチするボーン名

            // オフセット
            DirectX::XMFLOAT3 offsetPos = { 0,0,0 };
            DirectX::XMFLOAT3 offsetRot = { 0,0,0 };
            DirectX::XMFLOAT3 offsetScale = { 1,1,1 };

            // ランタイム用 (保存しない)
            std::weak_ptr<EffectInstance> activeInstance;
        };

        std::vector<Key> keys;
        EffectComponent* targetComponent = nullptr; // 操作対象

        TrackType GetType() const override { return TrackType::Effect; }

        // Bind: ActorからEffectComponentを取得する
        void Bind(void* target) override {
            if (auto actor = static_cast<Actor*>(target)) {
                // GetComponents<EffectComponent>() だと複数取れる可能性がありますが、
                // 通常は1つと仮定して GetComponent を使用します。
                auto comp = actor->GetComponent<EffectComponent>();
                targetComponent = comp.get();
            }
        }

        // Evaluate: 再生・停止・同期ロジック
        void Evaluate(float time) override
        {
            if (isMuted || !targetComponent) return;

            for (auto& key : keys)
            {
                // 現在時間がクリップの範囲内か？
                bool isInside = (time >= key.time && time < (key.time + key.duration));

                // 既に再生中のインスタンスがあるか確認
                auto instance = key.activeInstance.lock();

                if (isInside)
                {
                    // 範囲内かつ未再生なら Play
                    if (!instance)
                    {
                        key.activeInstance = targetComponent->Play(
                            key.effectName,
                            key.boneName,
                            key.offsetPos,
                            key.offsetRot,
                            key.offsetScale,
                            false // ループ制御はトラック側で行うためEffect自体はOneShot扱いにしつつ、Durationで管理する
                        );
                        instance = key.activeInstance.lock();

                       
                    }

                    // 再生中なら時間を同期 (Scrubbing対応)
                    if (instance)
                    {
                        // クリップ内での相対時間を計算
                        float relativeTime = time - key.time;

                        // エフェクトの時間を強制的に上書き
                        instance->age = relativeTime;
                        // 寿命をクリップの長さに合わせる
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
                    // 範囲外に出たのに再生中なら Stop
                    if (instance)
                    {
                        targetComponent->Stop(key.activeInstance);
                        key.activeInstance.reset();
                    }
                }
            }
        }

        // --- シリアライズ ---
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
            // StartTime順にソートしておくと描画時に便利
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }

        // 編集用ヘルパー
        void AddKey(float time, const std::string& effectName, float duration = 2.0f) {
            Key k;
            k.time = time;
            k.duration = duration;
            k.effectName = effectName;
            keys.push_back(k);
            // ソート
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.time < b.time; });
        }
    };

    // ==========================================
    // ★追加: ゲーム内イベント発生用トラック
    // ==========================================
    class EventTrack : public Track
    {
    public:
        struct Key {
            float time;
            std::string eventName; // "StartGame", "BossBattle", "FadeOut" など
            bool fired = false;    // 二重発火防止フラグ
        };

        std::vector<Key> keys;
        float lastEvaluateTime = -1.0f; // 前回の時間を記録（巻き戻り検知用）

        TrackType GetType() const override { return TrackType::Event; }

        // イベントは特定のターゲット（Actor）を持たず、全体に通知するのでBindは空でOK
        void Bind(void* target) override {}

        // 評価関数：時間がキーをまたいだら発火
        void Evaluate(float time) override
        {
            if (isMuted) return;

            // シークバーが巻き戻されたら（リセットまたはループ）、発火フラグを戻す
            if (time < lastEvaluateTime)
            {
                for (auto& key : keys) key.fired = false;
            }

            for (auto& key : keys)
            {
                // 「まだ発火していなくて」かつ「時間が到達した」場合
                if (!key.fired && time >= key.time)
                {
                    // 1. データを詰める
                    MessageData::CINEMATIC_EVENT_TRIGGER_DATA data;
                    data.eventName = key.eventName;

                    // 2. メッセンジャーで全体に送信！
                    Messenger::Instance().SendData(MessageData::CINEMATIC_EVENT_TRIGGER, &data);

                    // 3. 発火済みにする
                    key.fired = true;

                    // デバッグログ（出力ウィンドウで確認用）
                    char buf[256];
                    sprintf_s(buf, "[Sequencer] Event Fired: %s at %.2f\n", key.eventName.c_str(), time);
                    OutputDebugStringA(buf);
                }
            }

            lastEvaluateTime = time;
        }

        // --- 編集用ヘルパー ---
        void AddKey(float time, const std::string& name)
        {
            Key k;
            k.time = time;
            k.eventName = name;
            k.fired = false;
            keys.push_back(k);

            // 時間順にソート
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }

        // --- シリアライズ (保存) ---
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

        // --- デシリアライズ (ロード) ---
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
            // ロード後もソートしておく
            std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
                return a.time < b.time;
                });
        }
    };

}