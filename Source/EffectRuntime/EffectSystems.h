#pragma once

class Registry;
struct RenderContext;
class RenderQueue;

// EffectSpawnRequestComponent の autoPlay 状態を見て、runtime instance を生成するシステム。
// 実際の runtime instance を生成する。
class EffectSpawnSystem
{
public:
    // effect spawn 要求を処理する。
    static void Update(Registry& registry, float dt);
};

// 再生中 effect の currentTime を進めるシステム。
class EffectPlaybackSystem
{
public:
    // playback を dt 分だけ進める。
    static void Update(Registry& registry, float dt);
};

// 各 entity の socket に追従する effect の transform を更新するシステム。
class EffectAttachmentSystem
{
public:
    // attachment の world transform と world velocity を更新する。
    static void Update(Registry& registry, float dt);
};

// effect のシミュレーションと補助値更新を行うシステム。
// 揺れや lifetime fade などもここで更新する。
class EffectSimulationSystem
{
public:
    // シミュレーション関連の状態を更新する。
    static void Update(Registry& registry, float dt);
};

// 停止要求を処理し、effect の終了確認を行うシステム。
class EffectLifetimeSystem
{
public:
    // effect の寿命切れと停止を確認する。
    static void Update(Registry& registry, float dt);
};

// プレビュー用 effect を更新するシステム。
// エディタ確認用の特殊な loop 処理などを持つ。
class EffectPreviewSystem
{
public:
    // preview effect の playback を更新する。
    static void Update(Registry& registry, float dt);
};

// 再生中 effect から描画用 packet を抽出して RenderQueue に積むシステム。
class EffectExtractSystem
{
public:
    // mesh effect / particle effect の描画 packet を抽出する。
    static void Extract(Registry& registry, RenderContext& rc, RenderQueue& queue);
};
