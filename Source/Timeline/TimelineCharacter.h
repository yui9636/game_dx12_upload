#pragma once
#include "Component/Component.h"
#include "Storage/GameplayAsset.h"
#include <memory>
#include <vector>

class RunnerComponent;
class ColliderComponent;
class Actor;

class TimelineCharacter : public Component
{
public:
    const char* GetName() const override { return "TimelineCharacter"; }

    void Start() override;
    void Update(float elapsedTime) override;

    // 初期化時にデータを渡す
    void SetGameplayData(const GameplayAsset& data);

    // Runnerと連動させる
    void SetRunner(std::shared_ptr<RunnerComponent> runner);

    // アニメーション切り替え時に呼ぶ（リセット処理など）
    void OnAnimationChange(int newAnimIndex);

private:
    // 内部ヘルパー
    DirectX::XMMATRIX CalcWorldMatrixForItem(const GESequencerItem& item);
    int SecondsToFrames(float seconds) const;
    float FramesToSeconds(int frames) const;

private:
    std::shared_ptr<RunnerComponent> runner;
    std::shared_ptr<ColliderComponent> collider; // ヒットボックス同期用

    // 読み込んだ全データ
    GameplayAsset gameplayData;

    // 現在再生中のデータへの参照（コピーを持たずポインタで持つ＝軽量）
    const std::vector<GESequencerItem>* currentTimeline = nullptr;

    // ランタイム状態管理用（元のデータはconstなので、状態はこっちで持つ）
    // ※GESequencerItem自体にランタイム変数(instance等)が含まれているが、
    // const参照でデータを持つ以上、状態管理は別途必要になる。
    // ここではシンプルにするため「データのコピー」を持つ戦略に切り替えるか、
    // あるいは「状態管理用構造体」を別途定義するかが分岐点。
    // → 【結論】今回は安全のため「現在のアクションのデータのみコピーして持つ」方式にします。
    // （アクション切り替え時の一瞬だけコピーコストがかかるが、毎フレームの安定性が高い）
    std::vector<GESequencerItem> activeItems;

    int currentAnimIndex = -1;
    float fps = 60.0f;
};