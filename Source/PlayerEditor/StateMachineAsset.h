#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <DirectXMath.h>
// ステート種別
enum class StateNodeType : uint8_t
{
    Locomotion = 0,
    Action,
    Dodge,
    Jump,
    Damage,
    Dead,
    Custom
};
// 遷移条件
enum class ConditionType : uint8_t
{
    Input = 0,     // アクションボタンの押下 / 押し続け / 離し
    Timer,         // ステート内の経過時間
    AnimEnd,       // アニメーション終了
    Health,        // HP しきい値
    Stamina,       // スタミナしきい値
    Parameter,     // カスタムパラメータ判定
};

enum class CompareOp : uint8_t
{
    Equal = 0, NotEqual, Greater, Less, GreaterEqual, LessEqual
};

struct TransitionCondition
{
    ConditionType type    = ConditionType::Input;
    char          param[64] = {};   // アクション名、パラメータ名など
    CompareOp     compare = CompareOp::Equal;
    float         value   = 0.0f;
};
// ステート遷移
struct StateTransition
{
    uint32_t id       = 0;
    uint32_t fromState = 0;
    uint32_t toState   = 0;
    int      priority  = 0;

    std::vector<TransitionCondition> conditions;

    float exitTimeNormalized = 0.0f;  // 0 なら即時、0.9 ならアニメーション 90% 時点
    float blendDuration      = 0.2f;  // 遷移ブレンド秒数
    bool  hasExitTime        = false; // exitTimeNormalized まで待つ必要がある
};
// ステートノード
struct StateNode
{
    uint32_t      id   = 0;
    std::string   name;
    StateNodeType type = StateNodeType::Locomotion;

    int           animationIndex = -1;
    uint32_t      timelineId      = 0;
    bool          loopAnimation  = false;
    float         animSpeed      = 1.0f;
    bool          canInterrupt   = true;

    // エディタ上の配置位置
    DirectX::XMFLOAT2 position{ 0, 0 };

    // カスタムプロパティ（名前 -> 値）
    std::unordered_map<std::string, float> properties;
    // ステートごとに AI / Behavior Tree の紐づけ情報を保持する。
    // .bt ファイルへのパス。空なら「このステートでは AI なし」。
    // BehaviorTreeSystem はエンティティがこのステート中のときだけ、この BT を tick する。
    // ActorEditor_StateBoundBT_Spec_v2.0_2026-04-27.md を参照。
    std::string behaviorTreePath;

    // State Inspector の AI セクションに表示するデザイナー向け自由記述メモ。
    std::string aiNote;
};
// パラメータ定義
enum class ParameterType : uint8_t
{
    Float = 0, Int, Bool, Trigger
};

struct ParameterDef
{
    std::string   name;
    ParameterType type         = ParameterType::Float;
    float         defaultValue = 0.0f;
};
// ステートマシンアセット（最上位ドキュメント）
struct StateMachineAsset
{
    std::string name;

    std::vector<StateNode>       states;
    std::vector<StateTransition> transitions;
    std::vector<ParameterDef>    parameters;

    uint32_t defaultStateId = 0;
    uint32_t nextId         = 1;

    // 補助関数
    uint32_t GenerateId() { return nextId++; }

    StateNode* AddState(const std::string& stateName, StateNodeType type)
    {
        StateNode s;
        s.id   = GenerateId();
        s.name = stateName;
        s.type = type;
        states.push_back(std::move(s));
        return &states.back();
    }

    StateTransition* AddTransition(uint32_t from, uint32_t to)
    {
        StateTransition t;
        t.id        = GenerateId();
        t.fromState = from;
        t.toState   = to;
        transitions.push_back(std::move(t));
        return &transitions.back();
    }

    StateNode* FindState(uint32_t id)
    {
        for (auto& s : states) if (s.id == id) return &s;
        return nullptr;
    }

    const StateNode* FindState(uint32_t id) const
    {
        for (auto& s : states) if (s.id == id) return &s;
        return nullptr;
    }

    void RemoveState(uint32_t id)
    {
        states.erase(
            std::remove_if(states.begin(), states.end(),
                [id](const StateNode& s) { return s.id == id; }),
            states.end()
        );
        // 関連する遷移を削除する。
        transitions.erase(
            std::remove_if(transitions.begin(), transitions.end(),
                [id](const StateTransition& t) { return t.fromState == id || t.toState == id; }),
            transitions.end()
        );
    }

    void RemoveTransition(uint32_t id)
    {
        transitions.erase(
            std::remove_if(transitions.begin(), transitions.end(),
                [id](const StateTransition& t) { return t.id == id; }),
            transitions.end()
        );
    }

    // 指定ステートから出るすべての遷移を取得する。
    std::vector<const StateTransition*> GetTransitionsFrom(uint32_t stateId) const
    {
        std::vector<const StateTransition*> result;
        for (auto& t : transitions)
            if (t.fromState == stateId) result.push_back(&t);
        return result;
    }
};
