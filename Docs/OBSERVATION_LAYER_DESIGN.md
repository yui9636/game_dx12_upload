# Observation Layer Design — Godot 級

## 目的

AI エージェントが「現在エンジンで起きていることを完全に問い合わせられる」状態を作る。
スクリーンショット＋目視ループを根絶し、決定論的な状態遷移検証を可能にする。

## 設計原則

1. **READ は常に request-response で完結する**（ブロードキャスト依存禁止）
2. **クエリは型安全な構造化 JSON を返す**（自然言語の note フィールドに頼らない）
3. **重いクエリはフィルタ／cursor／diff をサポートする**（全件ダンプ禁止）
4. **すべての観測 API は副作用なし**（state を変えるなら別 namespace）
5. **`*.list` / `*.get` / `*.diff` / `*.watch.pull` を最大4面で揃える**（一貫性）
6. **イベント系は cursor pull 方式**（broadcast 専用にしない）

## ネームスペース設計

```
ecs.*           — エンティティ／コンポーネント生レベル
entity.*        — 単一エンティティへの操作（既存 add_collider_element 等を再分類）
bone.*          — スケルトン／ボーン世界座標
animator.*      — 再生中のアニメーション状態
collision.*     — ヒットボックス衝突イベント／レイキャスト
input.*         — 解決済み入力アクション／軸
gameflow.*      — GameFlow ランタイム状態
log.*           — エンジンログ読出
asset.*         — リソース読込状態
render.*        — レンダーキュー／パスタイミング
editor.*        — エディタ UI 状態（選択／フォーカス／ツール）
ai_session.*    — 既存セッション管理
```

## カテゴリ別 API 設計

### 1. ecs.* — 基盤ECS観測

既存：`ecs.query` / `ecs.hierarchy` / `ecs.diff` / `ecs.watch`

**追加が必要：**

#### `ecs.field.get`
単一フィールドだけ取る。重い entity 全体 dump を避ける。
```jsonc
// in
{ "entity": "73014444032", "component": "HealthComponent", "field": "health" }
// out
{ "entity": "73014444032", "component": "HealthComponent", "field": "health",
  "value": 7, "type": "int", "timestamp": "..." }
```

#### `ecs.field.watch.pull`
特定フィールドの変更履歴を cursor で取る。
```jsonc
// in
{ "entity": "73014444032", "component": "HealthComponent", "field": "health",
  "cursorName": "player_hp_watch" }
// out
{ "events": [
    { "frame": 12450, "before": 10, "after": 8, "timestamp": "..." },
    { "frame": 12612, "before": 8, "after": 6, "timestamp": "..." }
  ], "currentValue": 6 }
```

#### `ecs.query` の拡張
- `filter` 引数で componentName.field=value 条件を追加
- `projection` 引数で返すフィールドを限定
```jsonc
{ "include": ["PlayerTagComponent"],
  "filter": { "HealthComponent.health": { "lt": 5 } },
  "projection": ["NameComponent.name", "HealthComponent.health"] }
```

### 2. bone.* — 骨格世界座標

これが無いから Attack コライダー位置を目視で合わせた。最重要。

#### `bone.list`
```jsonc
// in
{ "entity": "73014444032" }
// out
{ "entity": "...", "modelPath": "Data/Model/Actor/A5.gltf",
  "bones": [
    { "index": 0, "name": "root", "parentIndex": -1 },
    { "index": 34, "name": "hand_r", "parentIndex": 33 },
    ...
  ] }
```

#### `bone.get_world`
```jsonc
// in
{ "entity": "73014444032", "boneIndex": 34, "boneName": "hand_r" }
// out
{ "boneIndex": 34, "boneName": "hand_r",
  "worldPosition": [1.2, 1.1, 0.3],
  "worldRotation": [0.0, 0.7, 0.0, 0.7],
  "localPosition": [...],
  "modelSpacePosition": [...] }
```

#### `bone.get_world_batch`
複数ボーン一括取得（毎フレーム個別呼出を避ける）。

### 3. animator.* — アニメーション状態

#### `animator.get_state`
```jsonc
// in
{ "entity": "73014444032" }
// out
{ "entity": "...",
  "currentAnimationIndex": 2,
  "currentAnimationName": "Attack01",
  "currentTime": 0.34,
  "currentFrame": 20,
  "totalFrames": 60,
  "isPlaying": true,
  "isLooping": false,
  "speed": 1.0,
  "blendTargets": [ { "animIndex": 3, "weight": 0.2 } ] }
```

#### `animator.list_animations`
モデルにある全アニメ一覧（既存の player_editor.get_animations を非エディタ化）。

#### `animator.watch.pull`
アニメーション切替イベントを取る。
```jsonc
// out events: { animChanged: { from: "Idle", to: "Attack01" }, frame: 12450 }
```

### 4. collision.* — 衝突観測

これが無いから Battle 自動 Victory が動かないか分からなかった。

#### `collision.events.pull`
直近の衝突イベントを cursor で取得。
```jsonc
// in
{ "cursorName": "battle_watch", "maxEvents": 100 }
// out
{ "events": [
    { "frame": 12450,
      "attackerEntity": "73014444032", "attackerName": "Player",
      "attackerColliderIdx": 1, "attackerAttribute": "Attack",
      "victimEntity": "77309411328", "victimName": "Enemy",
      "victimColliderIdx": 0, "victimAttribute": "Body",
      "contactPoint": [1.5, 1.0, 0.2],
      "damageDealt": 1
    }
  ] }
```

#### `collision.raycast`
任意のレイで何にぶつかるか。
```jsonc
// in
{ "origin": [0,1,0], "direction": [0,0,1], "maxDistance": 10, "mask": "Enemy" }
// out
{ "hit": true, "entity": "...", "boneIndex": -1,
  "point": [...], "normal": [...], "distance": 4.2 }
```

#### `collision.overlap_sphere`
特定球と重なる全 collider を取る（Attack コライダーが今フレーム何に重なっているか）。

### 5. input.* — 入力解決

#### `input.get_resolved_state`
```jsonc
// in
{ "entity": "73014444032" }
// out
{ "actions": [
    { "name": "Attack", "pressed": true, "framePressed": 12450 },
    { "name": "Dodge",  "pressed": false }
  ],
  "axes": [
    { "name": "MoveX", "value": 0.7 },
    { "name": "MoveY", "value": -0.3 }
  ] }
```

#### `input.events.pull`
直近 N フレームの入力イベントを取る。
```jsonc
// out
{ "events": [
    { "frame": 12450, "type": "key.down", "scancode": 29, "keyName": "Z" },
    { "frame": 12450, "type": "action.pressed", "owner": "...", "actionName": "Attack" }
  ] }
```

### 6. gameflow.* — GameFlow ランタイム

これが無いから「なぜ遷移しない」を screenshot で目視するしかなかった。

#### `gameflow.get_runtime_state`
```jsonc
{ "isActive": true,
  "currentNodeId": 4, "currentNodeName": "Battle", "currentNodeType": "Battle",
  "currentScenePath": "Data/Scene/ActionGame_Play.scene",
  "nodeTimer": 3.4,
  "previousNodeId": 1,
  "sceneTransitionRequested": false,
  "waitingSceneLoad": false,
  "flags": { "boss_defeated": false },
  "pendingActions": [],
  "pendingTransitions": [
    { "transitionId": 5, "fromNodeId": 4, "toNodeId": 3,
      "conditions": [
        { "type": "BattleResult", "value": "victory",
          "satisfied": false, "lastEvalFrame": 12450 }
      ] }
  ] }
```

#### `gameflow.events.pull`
GameFlow が発火した event log を cursor で取る。
```jsonc
{ "events": [
    { "frame": 12450, "kind": "transition", "from": 1, "to": 4, "name": "Title_to_Battle" },
    { "frame": 12612, "kind": "action_executed", "type": "StartBattleFlow", "target": "default" },
    { "frame": 13000, "kind": "condition_satisfied", "transitionId": 5, "conditionIndex": 0 }
  ] }
```

#### `gameflow.eval_conditions`
今すぐ全 transition の条件を評価し結果を返す（デバッグ用）。

### 7. log.* — エンジンログ

#### `log.tail`
最新 N 行を取る。
```jsonc
// in
{ "lines": 50, "minSeverity": "warn", "filterRegex": "Camera|Battle" }
// out
{ "lines": [
    { "timestamp": "...", "severity": "WARN", "category": "Camera2D",
      "message": "Multiple active Camera2DMainTagComponent..." }
  ] }
```

#### `log.pull`
前回 pull 以降の新着ログを cursor で取る。

#### `log.clear`
ログバッファクリア（テストの begin で叩く）。

### 8. asset.* — リソース読込状態

#### `asset.status`
```jsonc
// in
{ "path": "Data/Model/Actor/A5.gltf" }
// out
{ "path": "...", "loaded": true, "kind": "Model",
  "loadDurationMs": 142, "lastError": null,
  "gpuMemoryBytes": 1234567 }
```

#### `asset.list_loaded`
現在ロード済みのアセット一覧。

### 9. render.* — レンダーキュー観測

#### `render.queue.snapshot`
今フレームの draw packet サマリ。
```jsonc
{ "frame": 12450,
  "opaquePackets": 12,
  "transparentPackets": 3,
  "effectParticles": 5,
  "uiPackets": 8,
  "totalDrawCalls": 28,
  "culledPackets": 4 }
```

#### `render.pass_timings`
パス別タイミング（既存 Profiler を構造化して返す）。

### 10. editor.* — エディタ UI 状態

#### `editor.get_focus`
```jsonc
{ "activePanel": "game_view",
  "selectedEntities": ["73014444032"],
  "selectedAsset": "Data/Prefabs/ActionGame_Player.prefab",
  "activeTool": "translate",
  "gizmoSpace": "world" }
```

#### `editor.get_hierarchy_selection`
hierarchy パネルで何が選ばれているか。

### 11. visual.* — 可視性検証（既存拡張）

既存：`visual.verify_entity` / `visual.verify_entity_game_view` / `visual.assert_entities_visible`

**追加：**

#### `visual.get_pixel_at_screen`
画面の指定座標の RGB を取る（visual.evaluate_capture より軽い）。

#### `visual.find_text`
画面上で指定テキストが描画されているか（OCR or text packet match）。
```jsonc
// in
{ "text": "ACTION GAME", "target": "game_view" }
// out
{ "found": true, "screenRect": [400, 100, 480, 80], "fontSize": 80 }
```

#### `visual.compare_capture`
2つの capture を pixel-by-pixel 比較（regression test 用）。

### 12. session.* — テスト・検証フロー支援

#### `session.assert_invariant`
複数条件を一括検証。
```jsonc
{ "checks": [
    { "kind": "entity_exists", "entity": "...", "name": "Player" },
    { "kind": "component_field", "entity": "...",
      "component": "HealthComponent", "field": "health", "op": "gt", "value": 0 },
    { "kind": "gameflow_node", "expectedName": "Battle" }
  ] }
// out: { "allPassed": false, "failures": [...] }
```

#### `session.record_macro` / `session.replay_macro`
一連のコマンドをマクロ化（再現性ある回帰テスト）。

## 優先度（実装順）

### Phase 1 — 進行中の action game build を解放する最低限（4件）

これが無いと現状の build が完成しない：

1. **`gameflow.get_runtime_state`** — BattleFlow 自動 Victory のデバッグに必須
2. **`collision.events.pull`** — Attack hit 検証に必須
3. **`bone.get_world`** — 剣先端の Attack コライダー位置検証に必須
4. **`log.tail`** — runtime.log 手動 Read 廃止

### Phase 2 — Godot 級操作の中核（5件）

5. **`animator.get_state`** — アクションゲーム制御の基礎
6. **`input.get_resolved_state` / `input.events.pull`** — 入力検証
7. **`ecs.field.get` / `ecs.field.watch.pull`** — 軽量フィールド観測
8. **`session.assert_invariant`** — 一括検証で screenshot 目視を排除
9. **`gameflow.events.pull` / `gameflow.eval_conditions`** — GameFlow 完全把握

### Phase 3 — 仕上げ（残り）

10. `collision.raycast` / `collision.overlap_sphere`
11. `editor.get_focus` / `editor.get_hierarchy_selection`
12. `visual.find_text` / `visual.compare_capture`
13. `asset.status` / `asset.list_loaded`
14. `render.queue.snapshot` / `render.pass_timings`
15. `session.record_macro` / `session.replay_macro`

## 実装ガイドライン

### コマンド登録手順（4箇所）

1. **C++ 側**: `Source/Automation/AIAutomationService.cpp`
   - `Handle*` 関数追加
   - `DispatchCommand` の `if (name == "...")` 分岐追加
2. **Python 側**: `Scripts/AIAutomationSDK/engine_client.py`
   - `COMMANDS` リストに追加
3. **必要なら**: `Scripts/AIAutomationSDK/llm_bridge.py` のポリシー deny に追加
4. **テスト**: 追加直後に1コマンド最低1回呼んで成功確認

### 返却型のお約束

すべての応答は最低でも以下を含む：
- 操作対象の identifier（entity / path / cursor name）
- 結果フィールド（要求に応じた構造化データ）
- `timestamp` または `frame`（時間軸が意味を持つ場合）

エラー時は既存 `MakeError` 形式（`code`, `message`, `details`）。

### cursor 系の規約

`*.pull` 系コマンドは：
- `cursorName` パラメータでチャネル分離
- レスポンスに `previousCursor` と `currentCursor` を返す
- バッファあふれ時は `truncated: true` と `droppedCount` を返す

### パフォーマンス制約

- 単一フィールド get は **1ms 以下**を目標
- entity 全 dump は禁止（filter / projection 必須化）
- collision/input イベントは **直近 60 フレーム** までバッファ

## 想定 LOC

| Phase | 推定追加行数 | 期間 |
|---|---|---|
| Phase 1 (4件) | 800〜1200 | 1セッション |
| Phase 2 (5件) | 1500〜2000 | 2セッション |
| Phase 3 (6件) | 2000〜2500 | 2セッション |

## 次のステップ

1. このドキュメントをレビューしてもらう
2. **Phase 1 から着手** — 4件実装＆リビルド＆動作確認
3. Phase 1 完了時点で action game build を再開
4. ボトルネックが出るたび Phase 2/3 から優先実装
