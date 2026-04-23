#pragma once

class Registry;
struct RenderContext;
class RenderQueue;

// EffectSpawnRequestComponent や autoPlay 状態を見て、
// 実際の runtime instance を生成するシステム。
class EffectSpawnSystem
{
public:
    // effect spawn 要求を処理する。
    static void Update(Registry& registry, float dt);
};

// 再生中 effect の currentTime 進行を担当するシステム。
class EffectPlaybackSystem
{
public:
    // playback を dt だけ進める。
    static void Update(Registry& registry, float dt);
};

// 親 entity や socket へ追従する effect の transform を更新するシステム。
class EffectAttachmentSystem
{
public:
    // attachment 情報をもとに world transform を更新する。
    static void Update(Registry& registry);
};

// effect のシミュレーション補助更新を行うシステム。
// 現状は lifetime fade などの補助値更新を担当する。
class EffectSimulationSystem
{
public:
    // シミュレーション関連の状態を更新する。
    static void Update(Registry& registry, float dt);
};

// 停止要求や寿命終了を見て、effect の終了確定を行うシステム。
class EffectLifetimeSystem
{
public:
    // effect の寿命判定と停止確定を行う。
    static void Update(Registry& registry, float dt);
};

// プレビュー専用 effect を更新するシステム。
// エディタ確認向けの特別な loop 挙動などを持つ。
class EffectPreviewSystem
{
public:
    // preview effect の playback を更新する。
    static void Update(Registry& registry, float dt);
};

// 再生中 effect から描画用 packet を抽出して RenderQueue へ積むシステム。
class EffectExtractSystem
{
public:
    // mesh effect / particle effect の描画 packet を抽出する。
    static void Extract(Registry& registry, RenderContext& rc, RenderQueue& queue);
};