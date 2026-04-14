# Player Editor / Runtime / Prefab Integration Spec

Date: 2026-04-11  
Workspace: `game_dx12_upload`

---

## 1. 目的

本仕様書の目的は、`PlayerEditor` を単なる試作 UI ではなく、`authoring -> prefab -> placed entity -> GameLayer` の導線を持つ実用ツールにすることである。

最低条件は以下。

- `Timeline` で設定した `Hitbox / VFX / Audio / Shake` が、editor preview と `GameLayer` で同じ frame、同じ位置、同じ見た目で出る
- `StateMachine` で設定した state / transition / input 条件が、そのまま `GameLayer` の player 操作へ接続される
- `PlayerEditor` で保存した prefab が、そのまま Level Editor に置けて、そのまま player として起動する
- `Hierarchy > Gameplay > Player` と `PlayerEditor > Save Prefab` が別々の player 構成を作らない

この文書は UI 改修仕様書ではない。  
`PlayerEditor`、`HierarchyECSUI`、`PrefabSystem`、`StateMachineSystem`、`Timeline runtime`、`Input runtime` を一体として扱う authoring-to-runtime 統合仕様書である。

---

## 2. 対象

対象ファイル:

- `Source/PlayerEditor/PlayerEditorPanel.cpp`
- `Source/PlayerEditor/PreviewState.cpp`
- `Source/PlayerEditor/TimelineDriver.cpp`
- `Source/PlayerEditor/TimelineAsset.h`
- `Source/PlayerEditor/TimelineAssetSerializer.cpp`
- `Source/PlayerEditor/StateMachineAsset.h`
- `Source/PlayerEditor/StateMachineAssetSerializer.cpp`
- `Source/PlayerEditor/InputMappingTab.cpp`
- `Source/Hierarchy/HierarchyECSUI.cpp`
- `Source/Asset/PrefabSystem.cpp`
- `Source/Gameplay/StateMachineSystem.cpp`
- `Source/Gameplay/TimelineSystem.cpp`
- `Source/Gameplay/TimelineRuntimeSystem.cpp`
- `Source/Gameplay/TimelineHitboxSystem.cpp`
- `Source/Gameplay/TimelineVFXSystem.cpp`
- `Source/Gameplay/TimelineAudioSystem.cpp`
- `Source/Input/InputResolveSystem.cpp`
- `Source/Animator/AnimatorService.cpp`

対象外:

- timeline graph の見た目 polish
- Niagara / Unreal 風の見た目模倣
- camera track / event track の全面改修
- 汎用 prefab system の抜本改造

---

## 3. 現状の問題

### 3.1 PlayerEditor と runtime が同じ経路を使っていない

`PlayerEditorPanel.cpp` の timeline 再生は `PreviewState + TimelineDriver` による animation time override が主であり、`GameLayer` 側の `TimelineItemBuffer` 実行系と同一経路ではない。  
結果として「editor で見えた timeline」と「実ゲームで出る timeline」が一致する保証がない。

関連:

- `Source/PlayerEditor/PlayerEditorPanel.cpp`
- `Source/PlayerEditor/TimelineDriver.cpp`
- `Source/Gameplay/TimelineSystem.cpp`
- `Source/Gameplay/TimelineRuntimeSystem.cpp`

### 3.2 StateMachineSystem が animation 再生と timeline 変換を雑に抱え込んでいる

`StateMachineSystem.cpp` は state 遷移時に animation を `AnimatorService::PlayBase()` で再生し、同時に `ApplyTimelineStateAsset()` で `TimelineAsset` を `TimelineItemBuffer` に手変換している。

問題は以下。

- timeline runtime 変換責務が `StateMachineSystem.cpp` に埋まっている
- `TimelineAsset` と `GESequencerItem` の対応が一か所で定義されていない
- preview 側が同じ evaluator を使っていない
- `AnimatorComponent` が存在する時だけ `PlayBase()` を呼ぶ分岐があり、起動条件が不安定

### 3.3 Hierarchy の Player 生成と PlayerEditor 保存 prefab が別物

`HierarchyECSUI.cpp` の `Gameplay > Player` は component を手打ち追加しているが、`InputBindingComponent`、`InputUserComponent`、`StateMachineParamsComponent`、`NodeSocketComponent` が無い。  
つまり「Player」を作る導線なのに、player authored data が揃わない。

一方 `PlayerEditorPanel.cpp` の `SavePrefabDocument()` は preview entity をそのまま prefab 化するだけで、Hierarchy 側の player 構成と整合しない。

### 3.4 PrefabSystem が player authored data を保存しきれていない

`PrefabSystem.cpp` は manual serializer である。  
現状、保存できる player 関連 component と、保存できない component が混在している。

保存できるもの:

- `MeshComponent`
- `NodeSocketComponent`
- `InputBindingComponent`
- `InputContextComponent`
- `InputUserComponent`
- `CharacterPhysicsComponent`
- `PlayerTagComponent`
- `StateMachineParamsComponent`

保存できないもの:

- `HealthComponent`
- `StaminaComponent`
- `ActionDatabaseComponent`
- `PlaybackComponent`
- `TimelineComponent`
- `TimelineItemBuffer`
- `LocomotionStateComponent`
- `ActionStateComponent`
- `DodgeStateComponent`
- `HitboxTrackingComponent`
- `AnimatorComponent`

このままでは、保存した prefab が player として完結した authored data を持たない。

### 3.5 Authored data と runtime state の切り分けが無い

現状は以下が曖昧に混ざっている。

- 保存すべき authored data
- instantiate 時に補完すべき runtime component
- 毎回 reset すべき transient state

その結果、`Hierarchy`、`PlayerEditor`、`PrefabSystem`、`GameLayer` の各所で「必要そうな component を適当に足す」実装になっている。

---

## 4. 設計原則

### 4.1 Authoring と runtime の接続は 1 経路に寄せる

`TimelineAsset` を runtime 実行形式へ変換する責務は 1 か所に置く。  
preview でも `GameLayer` でも同じ builder / 同じ runtime data を使う。

### 4.2 Player 構成は 1 か所で定義する

`Hierarchy > Gameplay > Player`、`PlayerEditor > Save Prefab`、prefab instantiate 後補完の 3 箇所で、同じ player 構成ルールを使う。

### 4.3 Prefab には authored data を保存し、runtime state は補完する

保存対象と非保存対象を明示し、instantiate 時の補完ルールを固定する。

### 4.4 Preview は runtime builder / runtime data を共有する

preview は runtime の簡易模倣ではなく runtime 利用者とする。  
ただし editor 固有の操作性を殺してはならない。

---

## 5. 中核構造

### 5.1 PlayerRuntimeSetup

新規追加:

- `Source/Gameplay/PlayerRuntimeSetup.h`
- `Source/Gameplay/PlayerRuntimeSetup.cpp`

公開 API:

```cpp
namespace PlayerRuntimeSetup
{
    void EnsurePlayerPersistentComponents(Registry& registry, EntityID entity);
    void EnsurePlayerRuntimeComponents(Registry& registry, EntityID entity);
    void ResetPlayerRuntimeState(Registry& registry, EntityID entity);
    bool HasMinimumPlayerAuthoringComponents(Registry& registry, EntityID entity);
}
```

責務:

- player 用 persistent component を保証する
- player 用 runtime component を保証する
- transient state を reset する
- `Hierarchy` / `PlayerEditor` / `Prefab instantiate` から共通利用される

重要設計:

- `PlayerRuntimeSetup` は public API としては 3 関数で良い
- ただし内部実装は必ず以下の policy に分割する
- `authored policy`
- `runtime policy`
- `reset policy`
- `PlayerRuntimeSetup.cpp` を「何でも屋」にしないため、component ごとの処理は policy 単位で閉じる

非責務:

- file dialog を開かない
- asset path を決めない
- editor selection を触らない
- timeline asset / state machine asset / input asset を load しない
- UI warning text を組み立てない
- save path や dirty state を管理しない

### 5.2 TimelineAssetRuntimeBuilder

新規追加:

- `Source/Gameplay/TimelineAssetRuntimeBuilder.h`
- `Source/Gameplay/TimelineAssetRuntimeBuilder.cpp`

公開 API:

```cpp
struct TimelineRuntimeBuildResult
{
    bool success = false;
    bool partialBuild = false;
    uint32_t warningCount = 0;
    uint32_t unsupportedTrackMask = 0;
};

namespace TimelineAssetRuntimeBuilder
{
    TimelineRuntimeBuildResult Build(const TimelineAsset& asset,
                                     int animationIndex,
                                     TimelineComponent& outTimeline,
                                     TimelineItemBuffer& outBuffer);
}
```

責務:

- `TimelineAsset` から `TimelineComponent + TimelineItemBuffer` を構築する
- `StateMachineSystem` 内の `ApplyTimelineStateAsset()` を置き換える
- preview entity と game entity の両方に対して同じ runtime data を構築する

`BuildResult` を `bool` にしない理由:

- runtime 非対応 track を editor 側 warning に接続するため
- partial build を許すかどうかを明示するため
- build failure 時の out 値の扱いを仕様化するため

### 5.3 PlayerEditor の責務

`PlayerEditorPanel.cpp` の責務は以下に限定する。

- model / timeline / state machine / input / socket の編集
- preview entity への authored data 適用
- prefab 保存前の `PlayerRuntimeSetup` 呼び出し
- validation warning 表示

以下は持たせない。

- player bundle 定義
- timeline runtime 変換
- prefab serializer policy

---

## 6. Component Policy

### 6.1 Prefab に保存する persistent component

- `NameComponent`
- `TransformComponent`
- `HierarchyComponent`
- `MeshComponent`
- `NodeSocketComponent`
- `PlayerTagComponent`
- `CharacterPhysicsComponent`
- `HealthComponent`
- `StaminaComponent`
- `ActionDatabaseComponent`
- `InputUserComponent`
- `InputContextComponent`
- `InputBindingComponent`
- `StateMachineParamsComponent`

備考:

- `ActionDatabaseComponent` は authored combo 定義なので保存対象
- `HealthComponent` / `StaminaComponent` は現行設計では initial value container と current runtime value を兼ねているため、v1 は保存対象にする
- ただしこれは危うい暫定策である
- 将来的には `authored initial value` と `runtime current value` を分離し、`InitialStatsAsset` または `InitialStatsComponent` に逃がす前提を仕様として固定する
- v1 の保存対象指定は「今の component 設計の歪みを肯定する」意味ではなく、「導線を成立させるための暫定措置」である

### 6.2 Prefab に保存しない runtime component

- `ResolvedInputStateComponent`
- `PlaybackComponent`
- `TimelineComponent`
- `TimelineItemBuffer`
- `HitboxTrackingComponent`
- `LocomotionStateComponent`
- `ActionStateComponent`
- `DodgeStateComponent`
- `AnimatorComponent`

`AnimatorComponent` の扱い:

- preview 開始時には必要なら補完される
- prefab 保存前には reset 済みの runtime component として扱う
- scene reopen / prefab instantiate 後には authored data から runtime 側で再生成される
- authored component として保存しない

### 6.3 Reset 規則

`ResetPlayerRuntimeState()` は最低限以下を reset する。

- `PlaybackComponent.currentSeconds = 0`
- `PlaybackComponent.playing = false`
- `PlaybackComponent.finished = false`
- `TimelineComponent.currentFrame = 0`
- `TimelineComponent.playing = false`
- `TimelineItemBuffer.items.clear()`
- `HitboxTrackingComponent.ClearHitList()`
- `ActionStateComponent.state = CharacterState::Locomotion`
- `ActionStateComponent.currentNodeIndex = -1`
- `DodgeStateComponent.dodgeTimer = 0`
- `DodgeStateComponent.dodgeTriggered = false`
- `ResolvedInputStateComponent` の actions / axes / text state を初期化

---

## 7. Hierarchy Integration Spec

### 7.1 Current Problem

`HierarchyECSUI.cpp` の `Gameplay > Player` は `AddComponent()` の手打ちで構成されている。  
この方式では、PlayerEditor 保存 prefab との整合を保てない。

### 7.2 Required Change

`Gameplay > Player` は以下のみに置き換える。

1. entity 作成
2. `NameComponent`, `TransformComponent`, `HierarchyComponent` を追加
3. `PlayerRuntimeSetup::EnsurePlayerPersistentComponents()` を呼ぶ
4. `PlayerRuntimeSetup::EnsurePlayerRuntimeComponents()` を呼ぶ
5. `PlayerRuntimeSetup::ResetPlayerRuntimeState()` を呼ぶ

### 7.3 禁止事項

Hierarchy 側では以下を禁止する。

- asset path を設定しない
- Player authored data の入力 UI を持たない
- `HierarchyECSUI.cpp` に「ついでにこれも設定」の実装を増やさない
- timeline asset を開かない
- state machine asset を編集しない

つまり Hierarchy 側の役割は「空の player authoring/entity を作る」までであり、authoring data の編集は `PlayerEditor` に固定する。

---

## 8. Prefab Save / Load Spec

### 8.1 Save Prefab の前処理

`PlayerEditorPanel::SavePrefabDocument()` の前に必ず以下を通す。

1. `ApplyEditorBindingsToPreviewEntity()`
2. `PlayerRuntimeSetup::EnsurePlayerPersistentComponents()`
3. `PlayerRuntimeSetup::EnsurePlayerRuntimeComponents()`
4. `PlayerRuntimeSetup::ResetPlayerRuntimeState()`
5. `PrefabSystem::SaveEntityToPrefabPath()`

### 8.2 Instantiate 後補完

`PrefabSystem::InstantiatePrefab()` または prefab restore 後に、root entity が `PlayerTagComponent` または `StateMachineParamsComponent` を持つ場合は以下を行う。

1. `PlayerRuntimeSetup::EnsurePlayerRuntimeComponents()`
2. `PlayerRuntimeSetup::ResetPlayerRuntimeState()`

### 8.3 PrefabSystem 拡張

`PrefabSystem.cpp` に以下の serialize / deserialize を追加する。

- `HealthComponent`
- `StaminaComponent`
- `ActionDatabaseComponent`

これを追加しない限り、player prefab は authored gameplay data を持ち帰れない。

---

## 9. Timeline Runtime Spec

### 9.1 Authoring Data

`TimelineAsset` は editor の正規 authored data とする。  
runtime は直接 `TimelineAsset` を解釈しない。必ず `TimelineAssetRuntimeBuilder` を経由して `TimelineComponent + TimelineItemBuffer` に落とす。

### 9.2 Build Rules

`TimelineAssetRuntimeBuilder::Build()` は以下を構築する。

- `TimelineComponent.fps`
- `TimelineComponent.frameMin = 0`
- `TimelineComponent.frameMax = asset.GetFrameCount()`
- `TimelineComponent.animationIndex = requestedAnimationIndex`
- `TimelineComponent.clipLengthSec = asset.duration`
- `TimelineItemBuffer.items`

v1 で対応する track:

- `Hitbox`
- `VFX`
- `Audio`
- `CameraShake`

v1 で非対応の track:

- `Animation`
- `Camera`
- `Event`
- `Custom`

### 9.3 BuildResult 規則

- `success = true, partialBuild = false`
  - 全 track が runtime 対応済み
- `success = true, partialBuild = true`
  - 一部 track は非対応だが、対応済み track の build は成功
- `success = false`
  - asset 不正、必須値欠落、または build 継続不能

失敗時の出力:

- `outTimeline` は初期状態へ reset
- `outBuffer` は empty

warning 収集規則:

- builder 自身は warning text を保持しない
- `warningCount` と `unsupportedTrackMask` を返す
- warning text の組み立てと UI 表示は `PlayerEditorPanel` 側で行う

### 9.4 Shared Evaluation

以下の system は `TimelineItemBuffer` を唯一の runtime 入力として継続利用する。

- `TimelineHitboxSystem`
- `TimelineVFXSystem`
- `TimelineAudioSystem`
- `TimelineShakeSystem`

つまり「editor で置いた item が GameLayer で同じように出る」条件は、builder を共通化することで満たす。

---

## 10. StateMachine Runtime Spec

### 10.1 起動条件

`StateMachineParamsComponent.assetPath` が設定されている entity は、`StateMachineSystem` によって起動できなければならない。  
起動条件を `AnimatorComponent` の有無に依存させてはならない。

### 10.2 Required Change

`StateMachineSystem.cpp` は以下へ変更する。

- `AnimatorComponent` 存在チェック付き `PlayBase()` 呼び出しを削除
- state entry / transition 時に常に `AnimatorService::PlayBase()` を呼ぶ
- animation 再生と timeline build を同時に適用する

### 10.3 State 遷移時の処理

state 遷移時に行う処理:

1. `currentStateId` 更新
2. `stateTimer` reset
3. `AnimatorService::PlayBase()` 呼び出し
4. `TimelineAssetRuntimeBuilder::Build()` 実行
5. `PlaybackComponent` reset
6. `TimelineComponent` reset

### 10.4 Input 条件

`TransitionCondition.param` は action 名を正規入力とする。  
数値 index 指定は互換用として残してもよいが、editor UI の主導線にしてはならない。

---

## 11. Preview Spec

### 11.1 目的

preview は「animation が動く窓」ではなく、runtime 接続の簡易検証環境とする。

### 11.2 共有するもの

preview は以下を runtime と共用する。

- `TimelineAssetRuntimeBuilder`
- `TimelineComponent`
- `TimelineItemBuffer`
- `StateMachine` authored data
- `InputBinding` authored data

### 11.3 editor 専用で許容するもの

preview と runtime を完全同一にしすぎると editor 側の操作性が死ぬ。  
そのため、以下の editor 専用制御は許容する。

- scrub
- single frame evaluate
- preview-only camera
- forced state playback
- editor overlay / gizmo / debug draw

つまり「runtime builder / runtime data は共用するが、時間制御と可視化制御は editor 専用 overlay を許す」という方針にする。

### 11.4 Preview-only State

preview 専用で持ってよいもの:

- camera yaw / pitch / dist
- preview scale
- preview render target
- preview entity
- editor selection state

prefab や runtime に反映してはいけないもの:

- preview scale
- preview camera
- preview time / current frame

---

## 12. UI / Workflow Spec

### 12.1 Main Flow

1. `Open Model`
2. `Open State`
3. `Open Input`
4. state から `timelineAssetPath` を設定
5. timeline 上で `Hitbox / VFX / Audio / Shake` を配置
6. preview で state を再生し、発火を確認
7. `Save Prefab`
8. Level Editor に prefab を配置
9. `GameLayer` でそのまま操作確認

### 12.2 Validation Warning

toolbar か status area に以下の warning を出す。

- `StateMachine asset path missing`
- `Input action map missing`
- `Player prefab missing required components`
- `Timeline track type not runtime-supported`

### 12.3 Save Prefab の意味

`Save Prefab` は単なる snapshot 保存ではない。以下を含む。

- current model 反映
- socket authored data 反映
- state machine asset path 反映
- input action map path 反映
- player bundle 補完
- runtime state reset 後の prefab 保存

---

## 13. 実装フェーズ

### Phase 1: Player Bundle Unification

対象:

- `HierarchyECSUI.cpp`
- `PlayerEditorPanel.cpp`
- `PrefabSystem.cpp`
- 新規 `PlayerRuntimeSetup.*`

内容:

- player bundle 定義の一本化
- `Gameplay > Player` の共通 API 化
- `Save Prefab` 前補完
- instantiate 後 runtime 補完

### Phase 2: Prefab Authoring Data Completion

対象:

- `PrefabSystem.cpp`

内容:

- `HealthComponent`
- `StaminaComponent`
- `ActionDatabaseComponent`

の serialize / deserialize 追加

### Phase 3: Timeline Runtime Builder

対象:

- 新規 `TimelineAssetRuntimeBuilder.*`
- `StateMachineSystem.cpp`
- `PlayerEditorPanel.cpp`

内容:

- `ApplyTimelineStateAsset()` の分離
- preview / runtime builder 共通化
- runtime 非対応 track の warning 化

### Phase 4: StateMachine Startup Reliability

対象:

- `StateMachineSystem.cpp`
- `AnimatorService.cpp`

内容:

- `AnimatorComponent` gate 撤去
- default state 起動保証
- transition 時の animation / timeline reset 安定化

### Phase 5: Preview Verification Loop

対象:

- `PlayerEditorPanel.cpp`
- `InputMappingTab.cpp`

内容:

- state preview
- input live test
- timeline item preview
- save-before-play validation

---

## 14. 受け入れ条件

### 14.1 Prefab / Placement

- `PlayerEditor` から保存した prefab を scene に置いた時、`PlayerTagComponent`、`CharacterPhysicsComponent`、`InputBindingComponent`、`StateMachineParamsComponent`、`NodeSocketComponent` が揃っている
- prefab 再読み込み後も `HealthComponent`、`StaminaComponent`、`ActionDatabaseComponent` が欠落しない

### 14.2 Runtime Start

- prefab を置いた直後に `GameLayer` で入力を受けられる
- `StateMachineSystem` が default state へ入る
- input 条件を満たすと transition が発火する

### 14.3 Timeline Consistency

- state entry で指定 timeline が runtime buffer に変換される
- `Hitbox / VFX / Audio / Shake` が GameLayer で発火する
- preview と GameLayer で frame 単位の発火順が一致する

### 14.4 Tooling Consistency

- `Gameplay > Player` で作った entity と `PlayerEditor` 保存 prefab の最小構成差分が無い
- `PlayerEditor > Save Prefab` が `PlayerRuntimeSetup` を経由する
- Hierarchy 側が player authored data 入力 UI を持たない

---

## 15. 最初に着手すべき実装

1. `PlayerRuntimeSetup.h/.cpp` を追加する  
2. `HierarchyECSUI.cpp` の `Gameplay > Player` を `PlayerRuntimeSetup` 呼び出しへ置換する  
3. `PlayerEditorPanel.cpp` の `SavePrefabDocument()` 前に `PlayerRuntimeSetup` を通す  
4. `PrefabSystem.cpp` に `HealthComponent` / `StaminaComponent` / `ActionDatabaseComponent` の serialize / deserialize を追加する  
5. `TimelineAssetRuntimeBuilder.h/.cpp` を追加する  
6. `StateMachineSystem.cpp` の `ApplyTimelineStateAsset()` を builder 呼び出しへ置換する  
7. `StateMachineSystem.cpp` の `AnimatorComponent` gate を削除する  
8. preview entity に builder ベースの timeline runtime 構築を入れる  
9. runtime 非対応 track の warning 表示を追加する  
10. prefab 配置後の smoke test を追加する  

---

## 16. 最終評価

この改修の本質は UI 増築ではない。  
`PlayerEditor`、`Prefab`、`Hierarchy`、`GameLayer` が別々の player 構成を持っている現状をやめ、authoring-to-runtime の一本線を作ることにある。

見た目の pane や button を増やすだけでは解決しない。  
本当に危ないのは以下。

- save した prefab が player として起動しない
- timeline の見た目と runtime の発火がズレる
- Hierarchy の Player と PlayerEditor 保存 prefab が別物

この仕様書はそこを直すためのものとする。

実装時の最大注意点は次の 2 つ。

- `PlayerRuntimeSetup.cpp` を新しい巨大ゴミ箱にしないこと
- `HealthComponent / StaminaComponent` の暫定 persistent 化を恒久設計だと誤解しないこと

つまりこの仕様の肝は、`PlayerEditor` を UI 改修対象ではなく runtime 統合対象として扱うことにある。
