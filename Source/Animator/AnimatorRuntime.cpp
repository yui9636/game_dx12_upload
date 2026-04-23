#include "AnimatorRuntime.h"
#include <stack>

namespace
{
    // モデルのノード情報から、bind pose 相当の初期姿勢を poses に書き込む。
    // ここでは各ノードの position / rotation / scale をそのままコピーする。
    static void FillBindPose(std::vector<Model::NodePose>& poses, const Model* model)
    {
        // モデルが無いなら何もできないので終了する。
        if (!model) return;

        // モデルが持つ全ノード情報を取得する。
        const auto& nodes = model->GetNodes();

        // ノード数に合わせて pose 配列を確保する。
        poses.resize(nodes.size());

        // 各ノードの初期 transform を pose 配列へコピーする。
        for (size_t i = 0; i < nodes.size(); ++i) {
            poses[i].position = nodes[i].position;
            poses[i].rotation = nodes[i].rotation;
            poses[i].scale = nodes[i].scale;
        }
    }

    // spine から下にぶら下がるノードを「上半身ボーン」としてマスク化する。
    // upper body 用アクション再生時に、どのボーンへ適用するかを判定するために使う。
    static void BuildBoneMask(AnimatorRuntimeEntry& entry)
    {
        // 以前のマスク情報を一旦破棄する。
        entry.isUpperBody.clear();

        // モデル参照が無いなら構築不能なので終了する。
        if (!entry.modelRef) return;

        // モデルの全ノードを取得する。
        const auto& nodes = entry.modelRef->GetNodes();

        // すべて false で初期化する。
        entry.isUpperBody.assign(nodes.size(), false);

        // spine ノードが見つかっていない、または範囲外なら終了する。
        if (entry.spineNodeIndex < 0 || entry.spineNodeIndex >= static_cast<int>(nodes.size())) {
            return;
        }

        // 深さ優先で辿るためのスタックを用意する。
        std::stack<int> pending;

        // 起点となる spine ノードを積む。
        pending.push(entry.spineNodeIndex);

        // spine 以下の子ノードをすべて辿って upper body 扱いにする。
        while (!pending.empty()) {
            // スタック先頭のノード index を取り出す。
            const int idx = pending.top();
            pending.pop();

            // 万が一範囲外なら無視する。
            if (idx < 0 || idx >= static_cast<int>(nodes.size())) {
                continue;
            }

            // このノードは上半身ボーンとしてマークする。
            entry.isUpperBody[idx] = true;

            // 子ノードをすべてスタックへ積む。
            for (auto* child : nodes[idx].children) {
                // child ポインタから配列内 index を算出する。
                const int childIdx = static_cast<int>(child - &nodes[0]);

                // 有効な index のみ追加する。
                if (childIdx >= 0 && childIdx < static_cast<int>(nodes.size())) {
                    pending.push(childIdx);
                }
            }
        }
    }
}

// 指定 entity の runtime entry を検索して返す。
// 無ければ nullptr を返す。
AnimatorRuntimeEntry* AnimatorRuntimeRegistry::Find(EntityID entity)
{
    // entity をキーに map を検索する。
    const auto it = m_entries.find(entity);

    // 見つかればその entry のアドレス、無ければ nullptr を返す。
    return (it != m_entries.end()) ? &it->second : nullptr;
}

// 指定 entity の runtime entry を必ず取得する。
// まだ無ければ新規作成し、model が変わっていたら Rebind する。
AnimatorRuntimeEntry& AnimatorRuntimeRegistry::Ensure(EntityID entity, Model* model)
{
    // entity に対応する entry を取得する。無ければ自動生成される。
    AnimatorRuntimeEntry& entry = m_entries[entity];

    // 参照モデルが変わっていたら runtime 情報を作り直す。
    if (entry.modelRef != model) {
        Rebind(entry, model);
    }

    // 有効な entry を返す。
    return entry;
}

// 指定 entity の runtime entry を削除する。
void AnimatorRuntimeRegistry::Remove(EntityID entity)
{
    m_entries.erase(entity);
}

// すべての runtime entry を削除する。
void AnimatorRuntimeRegistry::Clear()
{
    m_entries.clear();
}

// entry を新しい model に結び直し、runtime 用バッファや各種 index を初期化する。
void AnimatorRuntimeRegistry::Rebind(AnimatorRuntimeEntry& entry, Model* model)
{
    // いったん全状態を初期化する。
    entry = {};

    // 新しいモデル参照を保存する。
    entry.modelRef = model;

    // モデルが無いならここで終了する。
    if (!model) {
        return;
    }

    // ルートノード index を仮設定する。
    // 必要に応じて後でより厳密な取得方法へ差し替える余地がある。
    entry.rootNodeIndex = 1;

    // よく使う主要ボーンの index を名前検索で取得する。
    entry.pelvisNodeIndex = model->GetNodeIndex("pelvis");
    entry.spineNodeIndex = model->GetNodeIndex("spine_01");

    // ノード数を取得する。
    const size_t poseCount = model->GetNodes().size();

    // base layer 用 pose バッファを確保する。
    entry.basePoses.resize(poseCount);

    // action layer 用 pose バッファを確保する。
    entry.actionPoses.resize(poseCount);

    // 一時計算用 pose バッファを確保する。
    entry.tempPoses.resize(poseCount);

    // 最終出力 pose バッファを確保する。
    entry.finalPoses.resize(poseCount);

    // ブレンド補間用のオフセット配列を確保する。
    entry.blendOffsets.resize(poseCount);

    // finalPoses を bind pose 相当の初期姿勢で埋める。
    FillBindPose(entry.finalPoses, model);

    // spine 以下を upper body として扱うマスクを構築する。
    BuildBoneMask(entry);
}