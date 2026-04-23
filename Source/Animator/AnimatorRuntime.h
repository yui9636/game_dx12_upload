#pragma once

#include "Entity/Entity.h"
#include "Model/Model.h"
#include <DirectXMath.h>
#include <unordered_map>
#include <vector>
#include <string>

// 1体の entity に対応する Animator runtime 用の一時データを保持する構造体。
// モデル参照、主要ボーン index、各種 pose バッファ、補間用状態などをまとめて持つ。
struct AnimatorRuntimeEntry
{
    // この runtime entry が参照するモデル本体。
    // entity に対応する描画モデルやボーン階層を参照する。
    Model* modelRef = nullptr;

    // ルートノード index。
    // モデル全体の基準ノードとして使う。
    int rootNodeIndex = 1;

    // pelvis ボーンの index。
    // ルートモーションや下半身基準の処理で使う想定。
    int pelvisNodeIndex = -1;

    // spine ボーンの index。
    // 上半身マスクの起点として使う。
    int spineNodeIndex = -1;

    // 各ノードが「上半身に含まれるかどうか」を表すマスク。
    // spine から下にぶら下がるノードを true にする。
    std::vector<bool> isUpperBody;

    // base layer 用の pose 配列。
    // locomotion や通常移動アニメーションなど、下地となる姿勢を入れる。
    std::vector<Model::NodePose> basePoses;

    // action layer 用の pose 配列。
    // 攻撃や上半身アクションなど、追加で重ねたい姿勢を入れる。
    std::vector<Model::NodePose> actionPoses;

    // 一時計算用の pose 配列。
    // ブレンド途中や中間結果の退避に使う。
    std::vector<Model::NodePose> tempPoses;

    // 最終出力用の pose 配列。
    // 実際にモデルへ反映する最終姿勢をここに作る。
    std::vector<Model::NodePose> finalPoses;

    // ブレンド補間用の pose オフセット配列。
    // action 遷移時のなめらかな補間などに使う。
    std::vector<Model::NodePose> blendOffsets;

    // オフセットブレンド全体の継続時間。
    float offsetBlendDuration = 0.0f;

    // オフセットブレンドの経過時間。
    float offsetBlendTimer = 0.0f;

    // オフセットブレンドを使うかどうか。
    bool useOffsetBlending = false;

    // 前回 action 再生時刻。
    // action 再生の継続・切り替え判定で使う想定。
    float prevActionTime = 0.0f;

    // アニメーション名から index を引くためのキャッシュ。
    // 毎回文字列検索しないようにする。
    std::unordered_map<std::string, int> animNameCache;
};

// entity ごとの AnimatorRuntimeEntry を管理するレジストリ。
// AnimatorSystem などから entity をキーに runtime 状態へアクセスする。
class AnimatorRuntimeRegistry
{
public:
    // 指定 entity の runtime entry を検索する。
    // 見つからなければ nullptr を返す。
    AnimatorRuntimeEntry* Find(EntityID entity);

    // 指定 entity の runtime entry を必ず取得する。
    // 無ければ新規生成し、model が変わっていれば Rebind する。
    AnimatorRuntimeEntry& Ensure(EntityID entity, Model* model);

    // 指定 entity の runtime entry を削除する。
    void Remove(EntityID entity);

    // すべての runtime entry を削除する。
    void Clear();

private:
    // 1つの runtime entry を新しい model に結び直し、
    // pose バッファやボーン index などを初期化する。
    void Rebind(AnimatorRuntimeEntry& entry, Model* model);

private:
    // entity をキーに、その entity 専用の animator runtime 状態を保持する。
    std::unordered_map<EntityID, AnimatorRuntimeEntry> m_entries;
};