# Cinematic Sequencer ECS 仕様書

作成日: 2026-04-09  
対象: `game_dx12_upload`

---

## 1. 目的

本仕様書は、`Level Editor` 内で使用する `Sequencer` ツールを新設し、旧 `Actor / Component / CinematicSequencerComponent` 基盤を参照用ソースとして退避したうえで、ECS 駆動の新しいシネマティック編集・再生系を定義する。

今回の `Sequencer` は `Player Editor` や `Effect Editor` のような独立ワークスペースではない。  
`Level Editor` を構成するツールパネルの一つとして、**SceneView を見ながら下部の Sequencer Timeline で編集する** 形を採用する。

---

## 2. 最重要方針

### 2.1 ツール位置

- `Window -> Sequencer` を追加する
- ON のときのみ `Level Editor` 下部に `Sequencer Timeline` を表示する
- OFF のときは下部パネルを閉じる
- `Scene View / Hierarchy / Inspector / Asset Browser / Console` はそのまま維持する
- `WorkspaceTab` は増やさない
- 画面全体を `Cinematic Editor` に切り替えない

### 2.2 編集の主役

- 主役は **SceneView + 下部 Sequencer Timeline**
- 編集者は SceneView で配置対象を見る
- Timeline で key / section / track を操作する
- 右 Inspector は選択中 track/section/key の詳細編集専用
- シーケンサーのための独立 preview 画面は作らない

### 2.3 ECS 前提

シネマティックで制御する対象はすべて ECS ベースとする。

- Camera
- Transform
- Animator
- Effect
- Audio
- Event
- Camera Shake

旧 `Actor / Component` 系を新規 runtime に持ち込まない。

---

## 3. 想定 UX

### 3.1 基本操作

1. `Level Editor` でシーンを開く
2. `Window -> Sequencer` を押す
3. 下部に `Sequencer Timeline` が開く
4. Hierarchy で entity を選ぶ
5. Sequencer に binding / track を追加する
6. SceneView でカメラや対象位置を見ながら key を打つ
7. 再生・スクラブして ECS runtime に即反映する

### 3.2 カメラ編集

- シーン内にカメラ entity を置く
- Sequencer から camera binding を行う
- transform / fov / focus / roll を key 化する
- SceneView 上でカメラの位置関係を確認する
- カメラ遷移は cut だけでなく easing / blend を持てる
- UE Sequencer に近い「シーンの中でカメラを動かす」編集感を目指す

### 3.3 旧 Cinematic との違い

- 旧 `CinematicSequencerComponent` のような独立 UI にはしない
- `ghost camera` や `ActorManager` ベースの参照は持ち込まない
- Timeline は `Level Editor` の常設ドックパネルとして扱う
- runtime は `CinematicService + ECS systems` で評価する

---

## 4. UI 構成

### 4.1 Level Editor 内の配置

- 上: 既存 toolbar / menu
- 中央: 既存 SceneView / GameView / Hierarchy / Inspector / Asset Browser
- 下: `Sequencer Timeline`

### 4.2 Sequencer 表示時の下部構成

下部 Sequencer は UE Sequencer を参考に、最低限次の 4 面で構成する。

- `Track Outliner`
  - Binding 一覧
  - Track 一覧
  - mute / lock / solo
- `Timeline Header`
  - frame marker
  - second marker
  - playhead
  - play range
  - work range
- `Section Area`
  - section bar
  - keyframe
  - event marker
  - blend/ease handle
- `Details`
  - 選択中 key/section/track の詳細

### 4.3 SceneView との関係

- Sequencer は SceneView と同時に使う
- camera track の key 打ち時は SceneView 操作を使う
- transform track の key 打ち時も SceneView gizmo を使う
- SceneView 内に camera icon / path / selected key の補助表示を追加してよい
- ただし Sequencer 専用の巨大オーバーレイ UI は作らない

### 4.4 Timeline のスケーリング要件

- track list は virtualized list を採用する
- section area も visible row / visible frame 範囲だけ描画する
- ズーム量に応じて key / section / label の LOD を切り替える
- 遠景ズームでは key 個別描画を省略し、section block と代表 marker のみを描画してよい
- 100 track / 1000 section を超えてもスクロールとズームが破綻しないことを必須要件とする

---

## 5. Asset / Data Model

### 5.1 新アセット

新しいアセットを定義する。

`CinematicSequenceAsset`

保持項目:

- `name`
- `frameRate`
- `durationFrames`
- `playRangeStart`
- `playRangeEnd`
- `workRangeStart`
- `workRangeEnd`
- `bindings`
- `masterTracks`
- `folders`
- `viewSettings`

### 5.2 Binding

`CinematicBinding`

- `bindingId`
- `displayName`
- `bindingKind`
  - `Entity`
  - `Spawnable`
  - `PreviewOnly`
- `targetEntity`
- `spawnPrefabPath`
- `tracks`

`bindingId` は永続 GUID で管理する。  
runtime の entity id を保存キーにしない。

### 5.3 Track Type

`CinematicTrackType`

- `Transform`
- `Camera`
- `Animation`
- `Effect`
- `Audio`
- `Event`
- `CameraShake`
- `Bool`
- `Float`

### 5.4 Master Track

master track には binding を持たない全体制御を置く。

- `Camera Cut`
- `Global Event`
- `Global Audio`

### 5.5 Binding Track

binding track には対象 entity 依存の制御を置く。

- `Transform`
- `Camera`
- `Animation`
- `Effect`
- `Audio`
- `Event`
- `CameraShake`

### 5.6 Section

`CinematicSection`

- `sectionId`
- `trackType`
- `startFrame`
- `endFrame`
- `rowIndex`
- `muted`
- `locked`
- `evalPolicy`
- `seekPolicy`
- `payload`

payload は track type ごとに分岐する。

`evalPolicy`:

- `Static`
- `Animated`
- `TriggerOnly`

`seekPolicy`:

- `TriggerOnSeek`
- `SkipOnSeek`
- `EvaluateRangeOnSeek`

---

## 6. Track Payload

### 6.1 Transform

`CinematicTransformSection`

- `positionKeys`
- `rotationKeys`
- `scaleKeys`
- `easing`
- `space`
  - `World`
  - `Local`

### 6.2 Camera

`CinematicCameraSection`

- `cameraMode`
  - `FreeCamera`
  - `LookAtCamera`
- `transformKeys`
- `eyeKeys`
- `targetKeys`
- `fovKeys`
- `rollKeys`
- `focusDistanceKeys`
- `apertureKeys`
- `blendType`
- `blendEaseIn`
- `blendEaseOut`

`CameraTrack` は camera binding に対する transform を内包する。  
同一 binding 上で `CameraTrack` と `TransformTrack` が両方存在する場合、camera binding の transform 最終値は `CameraTrack` を優先する。  
`TransformTrack` は camera entity 以外、または camera binding の補助対象にのみ使用する。

`cameraMode` の規則:

- `FreeCamera`
  - `transformKeys` を使用する
  - `eyeKeys / targetKeys` は使用しない
- `LookAtCamera`
  - `eyeKeys / targetKeys` を使用する
  - `transformKeys` は使用しない

同一 section 内で `transformKeys` と `eyeKeys / targetKeys` を同時に評価しない。

### 6.3 Animation

`CinematicAnimationSection`

- `animationIndex`
- `animationName`
- `loop`
- `playRate`
- `blendInFrames`
- `blendOutFrames`
- `slot`

### 6.4 Effect

`CinematicEffectSection`

- `effectAssetPath`
- `socketName`
- `offsetPosition`
- `offsetRotation`
- `offsetScale`
- `seed`
- `loop`
- `fireOnEnterOnly`
- `stopOnExit`
- `retriggerPolicy`
- `assetOverrides`
- `runtimeOverrideBufferSlot`
- `editorPreviewOverrides`
- `instanceHandleSlot`
- `scalarOverrides`
- `colorOverrides`

`retriggerPolicy`:

- `RestartIfActive`
- `IgnoreIfActive`
- `LayerIfAllowed`

override の意味:

- `assetOverrides`
  - section asset に保存される永続 override 値
- `runtimeOverrideBufferSlot`
  - runtime 再生時に組み立てる override buffer の参照先
- `editorPreviewOverrides`
  - editor preview 中だけ適用する一時 override

### 6.5 Audio

`CinematicAudioSection`

- `audioAssetPath`
- `is3D`
- `attachSocket`
- `volume`
- `pitch`
- `loop`
- `startOffsetSec`
- `stopOnExit`
- `retriggerPolicy`
- `instanceHandleSlot`

### 6.6 Event

`CinematicEventSection`

- `eventName`
- `eventCategory`
- `payloadType`
- `payloadJson`
- `fireOnce`

### 6.7 Camera Shake

`CinematicCameraShakeSection`

- `duration`
- `amplitude`
- `frequency`
- `decay`
- `hitStopDuration`
- `timeScale`

---

## 7. ECS Runtime 設計

### 7.1 外部 API

新しい runtime 入口は `CinematicService` とする。

最低限必要な API:

- `PlaySequence(assetPath, bindingContext)`
- `StopSequence(handle)`
- `PauseSequence(handle, paused)`
- `SeekSequence(handle, frame)`
- `SetPlaybackRate(handle, rate)`
- `BindEntity(handle, bindingId, entity)`

### 7.2 ECS Component

`CinematicSequenceComponent`

- `assetPath`
- `currentFrame`
- `playbackRate`
- `loop`
- `playing`
- `paused`

`CinematicBindingContextComponent`

- `bindingOverrides`

`CinematicRuntimeStateComponent`

- `evaluatedFrame`
- `lastDispatchFrame`
- `runtimeFlags`
- `dirtyFlags`
- `lastRangeStart`
- `lastRangeEnd`

`CinematicSectionStateComponent`

- `activeSections`
- `enteredThisFrame`
- `exitedThisFrame`
- `triggeredSections`
- `effectInstanceHandles`
- `audioInstanceHandles`

実装では section 状態、trigger 履歴、runtime handle 群を内部的に分離してよい。  
`CinematicSectionStateComponent` は概念上の代表名であり、1 コンポーネントへ全責務を直積みすることを要求しない。

### 7.3 評価原則

- 毎フレームすべての track / section / curve を全評価しない
- 評価は `Dirty評価` を原則とする
- playhead が進行した範囲だけを評価する
- seek 時は `EvaluateRange(startFrame, endFrame)` を実行する
- section 範囲外は即スキップする
- `Static` 区間はキャッシュを再利用する
- `Animated` 区間のみ補間評価する
- timeline 編集直後は該当 binding / track / section だけを dirty にする
- `TriggerOnly` は `EventTrack` と trigger 型 `EffectTrack` にのみ許可する
- `TransformTrack / CameraTrack / AnimationTrack / AudioTrack` に `TriggerOnly` を使用しない

### 7.4 EvaluateRange

`EvaluateRange(startFrame, endFrame)` を標準 API とする。

用途:

- 再生中の通常進行
- seek
- loop wrap
- editor での playhead drag

評価対象:

- section enter
- section exit
- trigger event
- animated channel 補間
- effect/audio の active state 遷移

loop wrap 規則:

- loop wrap は `endFrame -> startFrame` をまたぐ 2 区間評価として扱う
- `fireOnce` は 1 playback span 内で 1 回を意味する
- loop により新しい playback span に入った場合、`fireOnce` event は再発火可能とする

### 7.5 Systems

- `CinematicSpawnSystem`
  - sequence instance entity の生成
- `CinematicPlaybackSystem`
  - frame 進行
- `CinematicDirtyBuildSystem`
  - dirty track / section / binding の収集
- `CinematicEvaluateSystem`
  - dirty / range 単位で track 評価
- `CinematicSectionStateSystem`
  - enter / active / exit 状態更新
- `CinematicApplySystem`
  - ECS 対象へ反映
- `CinematicEventDispatchSystem`
  - event section を MessageData 互換形式で配信

### 7.6 適用先

- Transform: `TransformComponent`
- Camera: `CameraLensComponent`, `CameraMatricesComponent` 相当
- Animation: `AnimatorService`
- Effect: `EffectService`
- Audio: `TimelineAudioSystem` または新 Audio service
- Shake: `TimelineShakeSystem` 系

### 7.7 Effect / Audio 状態管理

`EffectSection` と `AudioSection` は瞬間評価ではなく状態保持型とする。

- section 進入時に enter 処理
- active 区間中は handle を保持
- section 離脱時に exit 処理
- seek 時は `seekPolicy` と `retriggerPolicy` に従って再評価する
- 同一 section をフレームごとに再発火しない

`EffectSection` は `EffectService` に対して instance handle を保持し、parameter override も section 単位で適用できるようにする。  
`AudioSection` も同様に runtime handle を保持する。

---

## 8. Event System

### 8.1 目的

Sequencer から gameplay event を発火できるようにする。  
既存 `MessageData` 系の考え方は流用するが、旧 Actor 前提に戻さない。

### 8.2 方針

- event payload は sequence asset に保存する
- runtime で `MessageData` 互換データへ変換して dispatch する
- payload の受け口は ECS / gameplay systems 側に置く
- Actor 参照を payload に直接持たない

### 8.3 推奨 payload

- `Bool`
- `Int`
- `Float`
- `String`
- `EntityRef`
- `Vector3`
- `Color`
- `JsonObject`

### 8.4 dispatch モデル

- section の開始 frame で fire
- `fireOnce=true` の場合、同一 play 範囲で再発火しない
- seek で飛び越えた場合は `EvaluateRange` で対象範囲を走査する
- seek 時の発火規則は section の `seekPolicy` に従う
- 0 → 100 frame のような大きいジャンプでも、範囲内 event を正しく処理できることを必須とする

---

## 9. Editor 機能要件

### 9.1 必須

- `Window -> Sequencer`
- 下部パネルの開閉
- track outliner
- key/section 作成
- scrub / play / pause / stop
- frame / seconds 表示切替
- camera cut
- animation/effect/audio/event track
- selected section details
- virtualized track list
- zoom LOD
- dirty evaluation
- section state management

### 9.2 早期対応

- SceneView 上の camera path 表示
- easing handle
- multi-row section
- folder / group
- ripple edit
- snapping
- selection sync

### 9.3 後続

- shot track
- sub-sequence
- marker / bookmark
- curve editor 強化
- camera rail / target rig

---

## 10. Level Editor 統合

### 10.1 メニュー

`Window(W)` に `Sequencer` を追加する。

### 10.2 パネル表示フラグ

`EditorLayer` に `m_showSequencer` を追加する。

### 10.3 ドッキング

- 下部ドックに `Sequencer` を配置する
- `Console` と同居または隣接させる
- 初期表示では `Console` と上下/タブ共存してよい
- 下部全幅を占有してよい

### 10.4 WorkspaceTab との関係

- `WorkspaceTab::CinematicEditor` は作らない
- `LevelEditor` の内部ツールとしてのみ存在する

---

## 11. 実装フェーズ

### Phase 1

- 仕様書修正
- `Window -> Sequencer` 追加
- `m_showSequencer` 追加
- 下部 Sequencer パネル骨組み
- `CinematicSequenceAsset` 雛形

### Phase 2

- track outliner
- playhead / scrub
- section bar / key 表示
- details panel

### Phase 3

- ECS playback
- transform/camera track 実適用
- SceneView key 打ち

### Phase 4

- animation/effect/audio/event/shake 統合
- `CinematicService`
- event dispatch

### Phase 5

- easing
- blend
- camera path overlay
- shot workflow

---

## 12. 旧 Cinematic の扱い

- `Source/Cinematic/` の旧コードは参考用ソースとして保持してよい
- 新 runtime の依存先にはしない
- `Actor / Component / CameraController` は復活させない
- 新設計は完全に ECS 基盤へ寄せる

---

## 13. 完了条件

- `Level Editor` を開いたまま `Sequencer` を下部表示できる
- SceneView を見ながら camera/transform key を編集できる
- animation/effect/audio/event/camera shake を ECS runtime に流せる
- event payload が旧 Actor 参照なしで dispatch できる
- 旧 `CinematicSequencerComponent` を実行基盤として使わない
- `EvaluateSystem` が dirty/range 評価で動作し、毎フル評価を前提にしない
- `EffectSection / AudioSection / EventSection` が enter/active/exit と seek 範囲評価を持つ
- timeline が大量 track / section でも virtualized 描画で破綻しない
