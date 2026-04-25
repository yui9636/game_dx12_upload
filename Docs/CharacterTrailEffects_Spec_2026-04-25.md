# CharacterTrailEffects Spec (2026-04-25)

Player / Enemy / NPC のキャラクターに対し、武器・電撃・残像など「動きに追従する trail / 残像エフェクト」を ECS + Effect ランタイム上で標準化するための仕様書。

最終的に **Sword Trail / Lightning Trail / Afterimage Trail** の 3 テンプレートを EffectEditor から発行可能にすることをゴールにする。

---

## 1. Goals

| # | Goal |
|---|------|
| G1 | エネミーや NPC を含む任意のキャラクターに、エフェクトが追従する手段を統一する (現状は `PlayerTagComponent` のみで識別する状態)。 |
| G2 | キャラクターのワールド速度を見て spawn rate / lifetime / width を変調する仕組みを `EffectAttachmentComponent` 系に組み込む。 |
| G3 | 3 種のテンプレートを `EffectEditorPanel.cpp` の Template メニューから発行できる: **Sword Trail**, **Lightning Trail**, **Afterimage Trail**。 |
| G4 | テンプレートはすべて Effect ランタイム (SoA v3 / Mesh Particle Phase A) 上で動作し、別系統の D3D11 旧 SwordTrail.h などには依存しない。 |
| G5 | 既存の `PlayerTagComponent` 利用箇所 (7 ファイル) を破壊しないこと。 |

## 2. Non-goals

- **真の Skinned-mesh 残像 (HologramManager 相当) は今回の範囲外。** Afterimage Trail は剛体メッシュパーティクル + 半透明 + 後追い遅延での近似表現として実装する。後日 Skinned 版を別 spec で扱う。
- 旧 `Source/.../SwordTrail.h` (D3D11 系) のクリーンアップは対象外。新規実装は Effect ランタイムでのみ行い、旧パスはそのまま残す。
- 物理ベースの破片飛散など、Effect とは独立した GameplayFX 系も対象外。
- ボーン名の自動マッピング (auto socket detection) は対象外。socket 名は明示指定。

## 3. Scope (Reuse / New)

### Reuse (既存をそのまま活用)
- `EffectAttachmentComponent` (parentEntity / socketName / offsetLocal / offsetRotDeg / offsetScale) — 追従本体
- `EffectAttachmentSystem::Update()` (EffectSystems.cpp:272-) — 親 entity → socket → world 変換
- `NodeAttachmentUtils::TryGetSocketWorldMatrix()` — ボーン socket 解決
- `EffectGraphNode` SpriteRenderer の汎用 slot (vectorValue5/6/7, stringValue2) — Mesh particle 用に既に拡張済み (Phase 3.1 で `MeshParticleSoA_Spec_2026-04-21` 準拠)
- `TrailComponent` (Component/TrailComponent.h) — Sword Trail はこの ECS 系を採用するか Effect Ribbon を採用するかを §8 で決める

### New (今回追加)
- `ActorTypeComponent` (新規, Player / Enemy / NPC / Neutral を識別。`PlayerTagComponent` は破棄せずそのまま残す)
- `EffectAttachmentComponent` への velocity 連動フィールド (spawn rate / width / alpha 倍率)
- 3 つの Template 関数 (`EffectEditorPanel.cpp::applyTemplate` 系)
- 必要に応じて socket 名規約 (`"WeaponTip"`, `"Hand_R"`, `"Spine"` 等) を Docs に書き起こす

## 4. Tag System Refactor — `ActorTypeComponent`

### 4.1 動機
現状エネミーや NPC を識別するコンポーネントが存在せず、`PlayerTagComponent` (`uint8_t playerId`) しかない。
今後 Effect 側から「Player に追従するか / Enemy に追従するか」を切り替えたいケース、ターゲット選定 (homing 系) で Tag が要るケースが増える。

### 4.2 デザイン方針: **追加 (Add) であって置換 (Replace) ではない**
`PlayerTagComponent` は以下 7 ファイルで使用されており、置換すると整合維持コストが高い:

```
Source/Asset/PrefabSystem.cpp                ← serialize / deserialize
Source/Generated/ComponentMeta.generated.h   ← reflection
Source/Hierarchy/HierarchyECSUI.cpp          ← Hierarchy 表示
Source/Inspector/InspectorECSUI.cpp          ← Inspector 編集
Source/Gameplay/PlayerInputSystem.cpp        ← クエリ条件
Source/Gameplay/PlayerRuntimeSetup.cpp       ← 生成・収集
Source/Gameplay/PlayerTagComponent.h         ← 定義
```

→ `PlayerTagComponent` は **playerId = 入力デバイス対応用** として温存し、汎用識別は新規 `ActorTypeComponent` に切り出す。

### 4.3 `ActorTypeComponent` 定義案

```cpp
// Source/Component/ActorTypeComponent.h
#pragma once

enum class ActorType : uint8_t {
    None = 0,
    Player,
    Enemy,
    NPC,
    Neutral,    // 商人・置物等
};

struct ActorTypeComponent {
    ActorType type = ActorType::None;
    uint16_t  factionId = 0;   // 派閥 (味方/敵以外の細分化に余地)
};
```

### 4.4 移行プラン
1. `ActorTypeComponent` 新規追加 (header + ComponentMeta 生成)
2. `PlayerRuntimeSetup.cpp::EnsurePlayerComponents` で `ActorTypeComponent{ Player, 0 }` を併置
3. Enemy 側の生成 (もしあれば) に `ActorTypeComponent{ Enemy, ... }` を併置
4. Inspector / Hierarchy / Prefab 側は `ComponentMeta.generated.h` の更新で自動反映
5. `PlayerTagComponent` はそのまま残し、入力受付クエリ (PlayerInputSystem) は引き続きこちらを使用

### 4.5 Effect 連携
`EffectAttachmentComponent` の親 entity が `ActorTypeComponent` を持っていれば、Template 側から「Player のみ」「Enemy のみ」「全 Actor」とフィルタできるが、本 spec の範囲では **フィルタ機能は実装せず、フィールドのみ用意**。実利用 spec で適宜参照。

## 5. `EffectAttachmentComponent` 拡張 — Velocity Modulation

### 5.1 拡張フィールド

```cpp
struct EffectAttachmentComponent
{
    // 既存
    EntityID parentEntity = Entity::NULL_ID;
    std::string socketName;
    DirectX::XMFLOAT3 offsetLocal   = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg  = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetScale   = { 1.0f, 1.0f, 1.0f };

    // ====== Phase: VelocityModulation ======
    bool   velocityModulateEnabled  = false;

    // 基準速度 (engine unit/sec)。
    // LocomotionStateComponent::runMaxSpeed と揃える運用を推奨 (現状 5.8)。
    // 1 world unit = 1m の前提なので m/s と読み替えて差し支えないが、
    // 単位名を明示しないことでワールドスケール変更時の追従ミスを防ぐ。
    float  velocitySpeedRef         = 5.8f;

    // 各パラメータは「(velocitySpeedRef での速度時に) base に加算される増分」を意味する加算式。
    // 乗算式だと spawnRate=0 の Template が永遠出ないため。
    float  velocitySpawnRateAdd     = 0.0f;   // +N spawn/sec  at |v| == ref
    float  velocityWidthAdd         = 0.0f;   // +ΔWidth       at |v| == ref
    float  velocityAlphaAdd         = 0.0f;   // +Δalpha (0..1) at |v| == ref
    float  velocityModulatorMax     = 3.0f;   // |v|/ref の clamp 上限。暴走防止。

    // ====== Runtime cache (system が書き込む) ======
    bool              velocityInitialized = false;     // 初回フレーム検出用
    DirectX::XMFLOAT3 prevWorldPos        = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 worldVelocity       = { 0.0f, 0.0f, 0.0f };
    float             worldSpeed          = 0.0f;
};
```

### 5.2 計算箇所
`EffectAttachmentSystem::Update()` (EffectSystems.cpp:272) は **dt を受け取る signature に変更が必要**。
現状 `GameLayer.cpp:159` で `EffectAttachmentSystem::Update(m_registry);` と引数なし呼び出しになっているため、ここも合わせて修正する。

```cpp
// EffectSystems.h
class EffectAttachmentSystem {
public:
    static void Update(Registry& registry, float dt);   // dt 追加
};

// GameLayer.cpp:159
EffectAttachmentSystem::Update(m_registry, time.dt);   // dt 渡す
```

実装手順:

```cpp
// socket world matrix から worldPos を取得した直後
if (!attachment.velocityInitialized || dt <= 0.0f) {
    // 初回フレームは速度ゼロ扱い (prevWorldPos が {0,0,0} のままだと
    // 巨大な初速が立つため必ずスキップする)
    attachment.worldVelocity      = { 0.0f, 0.0f, 0.0f };
    attachment.worldSpeed         = 0.0f;
    attachment.velocityInitialized = true;
} else {
    const DirectX::XMFLOAT3 dp = {
        worldPos.x - attachment.prevWorldPos.x,
        worldPos.y - attachment.prevWorldPos.y,
        worldPos.z - attachment.prevWorldPos.z
    };
    attachment.worldVelocity = { dp.x / dt, dp.y / dt, dp.z / dt };
    attachment.worldSpeed    = std::sqrt(
        attachment.worldVelocity.x * attachment.worldVelocity.x +
        attachment.worldVelocity.y * attachment.worldVelocity.y +
        attachment.worldVelocity.z * attachment.worldVelocity.z);
}
attachment.prevWorldPos = worldPos;
```

> 注: parentEntity が瞬間移動 (テレポート) した場合に巨大な dp が計算されてしまう問題は、Locomotion の clamp 同様に `velocityModulatorMax` で頭打ちにする (§5.3)。

### 5.3 Effect ランタイム側の参照
`EffectParticlePass::makeSimulationConstants` 内で modulator を計算 (**加算式**)。乗算式は spawnRate=0 を発射不能にするため不採用:

```cpp
if (attachment.velocityModulateEnabled) {
    const float ref       = std::max(attachment.velocitySpeedRef, 0.001f);
    const float modulator = std::clamp(attachment.worldSpeed / ref,
                                       0.0f, attachment.velocityModulatorMax);
    // 加算式: 元が 0 でも歩行で立ち上がる
    packet.spawnRate += attachment.velocitySpawnRateAdd * modulator;
    packet.startSize += attachment.velocityWidthAdd     * modulator;
    packet.tint.w     = std::clamp(packet.tint.w
                                   + attachment.velocityAlphaAdd * modulator,
                                   0.0f, 1.0f);
}
```

> 設計意図: Afterimage のように「静止時は 0、走り出すと出る」を実現するには
> `spawnRate = 0`, `velocitySpawnRateAdd = 80.0`, `velocitySpeedRef = 5.8` のように設定する。
> 走行時 (|v| ≒ 5.8) で +80 spawn/sec、停止時に 0、と狙い通り動く。

## 6. Three Templates — Detailed Design

各テンプレートは `EffectEditorPanel.cpp` の `Template` メニューに ImGui::MenuItem として追加し、内部では既存 `applyTemplate` / `applyMeshParticleTemplate` を呼ぶ。
**socket 名は Player FBX 想定 (`Hand_R`, `Spine`, `WeaponTip`)** — Enemy 側は別 socket で運用するためテンプレート発行後に EffectAttachmentComponent.socketName を編集する想定。

### 6.1 Sword Trail (剣の軌跡)

| 項目 | 値 / 採用方針 |
|------|---------------|
| 描画モード | **Effect Ribbon (Trail)** with velocity stretch — `TrailComponent` ではなく Effect 側で統一する (理由: §8.1) |
| Drawing | `EffectParticleDrawMode::Ribbon` |
| socket | `WeaponTip` (剣先) |
| color | start = (0.6, 0.85, 1.0, 1.0) cyan-white, end = (0.6, 0.85, 1.0, 0.0) |
| width | start = 0.18, ribbon velocity stretch = 0.4 |
| lifetime | 0.25s (短く、振り抜きの瞬間) |
| spawnRate | base 80 / sec, velocityModulate ON (`velocitySpeedRef = 5.8`, `velocitySpawnRateAdd = 60`) |
| blend | Additive |
| noise | curlNoise weak (strength 0.06, scale 0.3) — 残光のゆらぎ程度 |
| activation | 通常時は Disabled、攻撃ステートでのみ Enabled (StateMachine 側のフックは別 spec) |

### 6.2 Lightning Trail (電撃エフェクト)

| 項目 | 値 / 採用方針 |
|------|---------------|
| 描画モード | **Sprite (Billboard)** + 高頻度 spawn + curl noise |
| Drawing | `EffectParticleDrawMode::Billboard` |
| socket | `Hand_R` (主に右手) |
| color | start = (1.0, 0.9, 0.3, 1.0) 黄白, mid = (0.3, 0.6, 1.0, 0.8) 青, end = (0.0, 0.0, 0.5, 0.0) |
| size | start 0.10 → end 0.02 |
| lifetime | 0.18s (パチパチと弾ける感) |
| spawnRate | base 220 / sec, burstCount 24 (アクション開始時) |
| speed | 0.8 m/s (短い飛距離) |
| acceleration | (0, 0.4, 0) 軽く上昇 |
| blend | Additive |
| noise | curlNoise strong (strength 0.55, scale 0.18, scrollSpeed 1.6) |
| sub UV | columns/rows = 4×4 のフレーム連番 (テクスチャ追加が必要) |

### 6.3 Afterimage Trail (残像)

| 項目 | 値 / 採用方針 |
|------|---------------|
| 描画モード | **Mesh Particle** (Phase A 拡張で実装済みの SoA v3 mesh) — 剛体スナップショット表現 |
| Drawing | `EffectParticleDrawMode::Mesh` |
| meshPath | キャラクター本体の low-poly 代用メッシュ (`Asset/Models/Player_HoloProxy.fbx` 想定 — 後で TBD) |
| socket | `Spine` (体の中心) |
| color | start = (0.5, 0.8, 1.0, 0.55), mid = (0.5, 0.8, 1.0, 0.30), end = (0.5, 0.8, 1.0, 0.0) |
| meshScale | (1.0, 1.0, 1.0), random ±0.0 |
| meshAngularSpeed | 0.0 (回らない) |
| lifetime | 0.55s (ふわっと消える) |
| spawnRate | base 0 / sec (静止時は出ない), **velocityModulate ON** で歩行時のみ rate が立ち上がる |
| velocityModulate | `enabled = true`, `velocitySpeedRef = 5.8` (= runMaxSpeed), `velocitySpawnRateAdd = 80.0`, `velocityModulatorMax = 2.0`。走行時 |v|≈5.8 で 80 spawn/sec、ダッシュ (|v|>5.8) でも上限 160 spawn/sec で頭打ち。 |
| blend | Premultiplied Alpha |
| 注意 | 真の Skinned 残像ではないため、ポーズは固定。最初のリリースでは「半透明剛体スナップショット」で受け入れる (§2 Non-goals)。Skinned 版は将来 spec。 |

## 7. Implementation Phases

| Phase | 内容 | 主要触れる場所 |
|-------|------|----------------|
| **Phase 1** | `ActorTypeComponent` 追加 + ComponentMeta 生成 + Player 生成箇所に併置 | `Source/Component/ActorTypeComponent.h`, `Source/Generated/ComponentMeta.generated.h`, `Source/Gameplay/PlayerRuntimeSetup.cpp` |
| **Phase 2** | `EffectAttachmentComponent` velocity 拡張 + `EffectAttachmentSystem::Update(Registry&, float dt)` への signature 変更 + 呼び出し側 (`GameLayer.cpp:159`) 修正 + Effect 側 modulator 適用 (加算式) | `Source/Component/EffectAttachmentComponent.h`, `Source/EffectRuntime/EffectSystems.h`, `Source/EffectRuntime/EffectSystems.cpp`, `Source/Layer/GameLayer.cpp`, `Source/RenderPass/EffectParticlePass.cpp` |
| **Phase 3a** | Sword Trail Template (Ribbon) | `Source/EffectEditor/EffectEditorPanel.cpp` |
| **Phase 3b** | Lightning Trail Template (Billboard + curl) — sub UV テクスチャ追加 | 同上 + `Asset/Textures/lightning_subuv.png` |
| **Phase 3c** | Afterimage Trail Template (Mesh Particle + velocity modulate) — proxy mesh 追加 | 同上 + `Asset/Models/Player_HoloProxy.fbx` |
| **Phase 4** | Player / Enemy 双方で動作確認、socket 名差し替えテスト、PIX 計測 | テスト |

各 Phase はそれぞれ build verify + commit 単位で進める。

## 8. Risks & Open Questions

### 8.1 TrailComponent (ECS) vs Effect Ribbon (Effect ランタイム) — 剣 trail
- 既存 `TrailComponent` は ECS 上で完結し、`TrailExtractSystem` / `TrailSystem` で動く。
- Effect Ribbon は Effect ランタイム + GPU compute particle 上で動く。
- **採用案: Effect Ribbon に統一する。** 理由:
  - テンプレート 3 種を同じパイプライン (Effect Editor で発行できる) に揃えたい
  - velocity modulation も Effect 側で一元管理できる
  - `TrailComponent` は将来別用途 (敵攻撃の trace、移動経路の可視化等) に温存できる
- **Open**: 既に `TrailComponent` を使っている箇所があれば破棄 / 移行は別 spec で扱う。

### 8.2 Skinned-mesh Afterimage との折り合い
- 真の HologramManager 相当 (skinned mesh のスナップショットを焼き直して半透明描画) は本 spec ではやらない。
- Mesh Particle で代用する場合、Player 本体の skinned 形状とは見た目がズレるため、**proxy 用低ポリ mesh** を別途用意する必要あり。
- **Open**: proxy mesh の制作は誰が担当するか / 暫定で球状 placeholder にするか。

### 8.3 socket 名規約
- 現状 socket 名はキャラクター FBX のボーン名に依存。Enemy / NPC 側で同名のボーンが無いと Template がそのままでは動かない。
- **対応案**: Template 発行時に `socketName` をデフォルト `"Hand_R"` 等で埋め、ユーザがインスペクタで上書きする運用。
- **Open**: socket 命名規則 (`Hand_R` / `LeftHand` / `RightWeapon` 等のプロジェクト方言) を Docs にまとめる必要があるが本 spec の範囲外。

### 8.4 PlayerTagComponent との併置による重複
- `ActorTypeComponent::Player` と `PlayerTagComponent` が常に同一エンティティに乗ることになり、長期的には冗長。
- **承認方針**: 短期は併置を許容、`PlayerTagComponent` を `ActorTypeComponent::Player` 派生 + playerId 専用に縮退させるのは別 spec。

### 8.5 velocity modulation の数値安全性 (重要)
レビュー指摘で判明した 4 つの落とし穴と対応:

1. **乗算式は spawnRate=0 を永久に殺す** → §5.3 で **加算式** に変更済 (`spawnRate += add * modulator`)。Afterimage のように「静止時 0」設計が成立する。
2. **dt が現状 `EffectAttachmentSystem::Update()` に渡っていない** (`GameLayer.cpp:159` で引数なし呼び出し)
   → Phase 2 で signature を `Update(Registry&, float dt)` に変更し、`time.dt` を渡す。
3. **初回フレームの巨大初速** (`prevWorldPos = {0,0,0}` のまま走ると socket 位置 (例 {100, 50, 20}) との差分が dt で割られて爆速扱い)
   → `velocityInitialized` フラグを導入し、初回は `worldSpeed = 0` を返してから `prevWorldPos` を埋める (§5.2)。
4. **engine unit の単位ミス** (`m/s` 表記で 4.0 を仮置きしていたが、Locomotion 値は 1.6 / 3.2 / 5.8)
   → 実装上は `1 unit ≒ 1m` で整合 (`PlayerRuntimeSetup.cpp:212-218` の clamp `>20` で異常扱いも m/s 想定)。
   → ただし spec では `velocitySpeedRef` (単位 = engine unit/sec) と表記し、デフォルトを **`runMaxSpeed = 5.8` と揃える** ことを推奨。
5. **テレポート / 親エンティティ瞬間移動への耐性** → `velocityModulatorMax` (default 3.0) で `worldSpeed / ref` を頭打ち。爆速での spawn rate 暴走を防ぐ。

## 9. Future Work / Out of scope

- **真 Skinned Hologram 残像**: 専用 RenderPass + skinning matrix snapshot ring buffer を伴う作業。別 spec で `SkinnedHologramTrail_Spec.md` として扱う。
- **socket 自動マッピング**: ボーン階層から `Hand_R` 系の名前を heuristic 解決する仕組み。
- **TrailComponent 廃止 or 統合**: Effect Ribbon に統一する場合の旧 ECS 系の整理。
- **エネミー専用テンプレート**: 例えば `EnemySlash` / `BossAura` 等は本 spec の 3 テンプレートが安定したあとで派生させる。

---

## Appendix A: 影響ファイル一覧

| 種別 | ファイル |
|------|----------|
| 新規 | `Source/Component/ActorTypeComponent.h` |
| 編集 | `Source/Generated/ComponentMeta.generated.h` (再生成) |
| 編集 | `Source/Component/EffectAttachmentComponent.h` (velocity フィールド追加) |
| 編集 | `Source/EffectRuntime/EffectSystems.h` (Update signature: `(Registry&, float dt)`) |
| 編集 | `Source/EffectRuntime/EffectSystems.cpp` (velocity 計算 + initial frame skip) |
| 編集 | `Source/Layer/GameLayer.cpp` (L159: `Update(m_registry, time.dt)` に変更) |
| 編集 | `Source/RenderPass/EffectParticlePass.cpp` (加算式 modulator 適用) |
| 編集 | `Source/EffectEditor/EffectEditorPanel.cpp` (Template 3 種追加) |
| 編集 | `Source/Gameplay/PlayerRuntimeSetup.cpp` (ActorType 併置) |
| アセット | `Asset/Textures/lightning_subuv.png` (要追加) |
| アセット | `Asset/Models/Player_HoloProxy.fbx` (要追加 or 暫定) |

## Appendix B: 既存コードからの根拠

- `EffectAttachmentComponent` 既存定義 → `Source/Component/EffectAttachmentComponent.h:7-14`
- `EffectAttachmentSystem::Update` ループ → `Source/EffectRuntime/EffectSystems.cpp:272-337`
- `PlayerTagComponent` 利用箇所 → `Source/Asset/PrefabSystem.cpp:528-530, 1099-1102`, `Source/Gameplay/PlayerRuntimeSetup.cpp:327, 481, 508`, `Source/Gameplay/PlayerInputSystem.cpp:38`, `Source/Inspector/InspectorECSUI.cpp:2556`, `Source/Hierarchy/HierarchyECSUI.cpp:22`, `Source/Generated/ComponentMeta.generated.h:839-842, 984`
- Mesh Particle (Phase 3.1 templates) → `Source/EffectEditor/EffectEditorPanel.cpp` (commit `d2db4dd`)
- Mesh particle SoA v3 仕様 → `Docs/MeshParticleSoA_Spec_2026-04-21.md`

---

> 次ステップ: 本 spec の方針 (特に §8.1 Effect Ribbon 統一、§4.2 ActorType 追加方式、§6.3 Mesh proxy 採用) について承認を得たうえで Phase 1 実装に着手する。
