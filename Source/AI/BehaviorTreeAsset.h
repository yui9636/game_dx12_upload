#pragma once

// ビヘイビアツリーのノード型、アセット構造、検証結果を定義するファイル。
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "BlackboardComponent.h"

// ビヘイビアツリーノードの大分類を表す列挙型。
enum class BTNodeCategory : uint8_t
{
    Root      = 0,
    Composite = 1,
    Decorator = 2,
    Action    = 3,
    Condition = 4,
};

// ビヘイビアツリーで使用できる具体的なノード種別。
enum class BTNodeType : uint16_t
{
    Root              = 0,

    Sequence          = 100,
    Selector          = 101,
    Parallel          = 102,

    Inverter          = 200,
    Repeat            = 201,
    Cooldown          = 202,
    ConditionGuard    = 203,

    HasTarget         = 300,
    TargetInRange     = 301,
    TargetVisible     = 302,
    HealthBelow       = 303,
    StaminaAbove      = 304,
    BlackboardEqual   = 305,

    Wait              = 400,
    FaceTarget        = 401,
    MoveToTarget      = 402,
    StrafeAroundTarget= 403,
    Retreat           = 404,

    Attack            = 500,
    DodgeAction       = 502,

    SetSMParam        = 600,
    PlayState         = 601,

    SetBlackboard     = 700,
};

// ビヘイビアツリー内の 1 ノード分の保存データ。
struct BTNode
{
    // ノードを一意に識別する ID。
    uint32_t          id    = 0;
    // このノードの処理種別。
    BTNodeType        type  = BTNodeType::Root;
    // エディタ表示用のノード名。
    std::string       name;
    // 子ノード ID の一覧。
    std::vector<uint32_t> childrenIds;

    // ノード種別ごとの float パラメータ 0。
    float       fParam0 = 0.0f;
    // ノード種別ごとの float パラメータ 1。
    float       fParam1 = 0.0f;
    // ノード種別ごとの float パラメータ 2。
    float       fParam2 = 0.0f;
    // ノード種別ごとの int パラメータ 0。
    int         iParam0 = 0;
    // ノード種別ごとの文字列パラメータ 0。
    std::string sParam0;
    // ノード種別ごとの文字列パラメータ 1。
    std::string sParam1;
    // ブラックボードに書き込む値の型。
    BlackboardValueType bbType = BlackboardValueType::None;

    // エディタ上のグラフ表示位置。
    DirectX::XMFLOAT2 graphPos { 0.0f, 0.0f };
};

// ビヘイビアツリー全体を保持するアセットデータ。
struct BehaviorTreeAsset
{
    // 保存データのバージョン番号。
    int                  version = 1;
    // Root ノードの ID。
    uint32_t             rootId  = 0;
    // このツリーに含まれる全ノード。
    std::vector<BTNode>  nodes;

    // 指定 ID のノードを読み取り専用で検索する。
    const BTNode* FindNode(uint32_t id) const;
    // 指定 ID のノードを編集可能な形で検索する。
    BTNode*       FindNode(uint32_t id);
    // 新規ノード用の未使用 ID を発行する。
    uint32_t      AllocateNodeId() const;

    // 攻撃型テンプレートを生成する。
    static BehaviorTreeAsset CreateAggressiveTemplate();
    // 防御型テンプレートを生成する。
    static BehaviorTreeAsset CreateDefensiveTemplate();
    // 巡回型テンプレートを生成する。
    static BehaviorTreeAsset CreatePatrolTemplate();

    // アセットファイルから設定を読み込む。
    bool LoadFromFile(const std::filesystem::path& path);
    // 現在の設定をアセットファイルへ保存する。
    bool SaveToFile(const std::filesystem::path& path) const;
};

// ビヘイビアツリー検証メッセージの重要度。
enum class BTValidateSeverity : uint8_t
{
    Info    = 0,
    Warning = 1,
    Error   = 2,
};

// ビヘイビアツリー検証で出す 1 件分のメッセージ。
struct BTValidateMessage
{
    BTValidateSeverity severity = BTValidateSeverity::Info;
    std::string        message;
};

// ビヘイビアツリー検証結果をまとめて保持する構造体。
struct BTValidateResult
{
    // 検証中に見つかったメッセージ一覧。
    std::vector<BTValidateMessage> messages;

    // Error が 1 件以上あるか返す。
    bool HasError() const;
    // Error の件数を返す。
    int  ErrorCount() const;
    // Warning の件数を返す。
    int  WarningCount() const;
};

// ビヘイビアツリー全体を検証する。
BTValidateResult ValidateBehaviorTree(const BehaviorTreeAsset& asset);

// BTNodeType を文字列に変換する。
const char*    BTNodeTypeToString(BTNodeType t);
// 文字列から BTNodeType を取得する。
BTNodeType     BTNodeTypeFromString(const std::string& s);
// BTNodeType のカテゴリを取得する。
BTNodeCategory CategoryOfBTNodeType(BTNodeType t);
