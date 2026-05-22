# AI Automation Interface Specification

Version: 0.1  
Target: MyEngine DX12 Editor / Runtime  
Date: 2026-05-21

## 目的

この仕様書は、自作DX12エンジンにAI操作用インターフェースを追加し、AIがエディタ画面を確認しながら、シーン、Entity、Component、再生状態、保存状態を安定して操作できるようにするための共通ビジョンを定義する。

目標は「AIが人間の代わりにマウス座標を曖昧に操作する」ことではない。AIが画面を見て判断し、エンジン内部の明確なAPIを通して正確に編集する状態を作る。

## ビジョン

AIは次のループを回せる存在として扱う。

1. 現在のシーン状態を取得する。
2. 必要ならゲーム画面またはScene Viewのスクリーンショットを取得する。
3. EntityやComponentをAPI経由で編集する。
4. Play / Pause / Stepを制御して挙動を確認する。
5. 結果を読み取り、必要なら再調整する。
6. シーン、Prefab、設定ファイルを保存する。

このループにより、AIは「コードだけを書く補助者」から「エンジン内でゲームを組み立てる共同制作者」へ進む。

## 操作対象スコープ

AI Automation Interfaceの操作対象はLevel Editorだけに限定しない。このエンジンに存在する複数の専門エディターを、同じ自動化レイヤーから操作できるようにする。

対象に含めるエディター:

- Level Editor
  - Scene View、Game View、Hierarchy、Inspector、Asset Browser、Console、Lighting、Audio、Render Passes。
- Player Editor
  - プレイヤー/敵キャラクターのモデルプレビュー、Skeleton、State Machine、Timeline、Hitbox、VFX、Audio、Camera Shake、Projectileなど。
- Effect Editor
  - Effect graph、Timeline、Runtime override、Preview context、Effect asset picker。
- UI Editor
  - UI prefab、Canvas item、RectTransform、Sprite、Text、HP gauge template、UI binding。
- GameLoop Editor
  - GameFlow node graph、シーン遷移、入力イベント、UIボタンイベント、ランタイム登録。
- Sequencer
  - Cinematic sequence、Entity binding、Camera、Track、Keyframe、Preview。
- Terrain Editor
  - Terrain asset、brush、layer、height/splat編集、grass/vegetation設定。
- Asset Browser / Model Serializer
  - Asset selection、Prefab instantiate、Model serialization、import補助。

このスコープは重要である。AIがゲームを作るには、Entityを配置するだけでは足りない。入力、遷移、演出、UI、キャラクター挙動、エフェクト、地形、カメラ、保存単位まで編集できる必要がある。

したがって本仕様では、各専門エディターを「UI画面」ではなく「編集ドメイン」として扱う。AIは必要に応じてエディター画面を開くが、実際の変更は可能な限り各ドメインの内部データ/APIを通して行う。

## 既存エンジンの利用方針

調査時点で、以下の既存構造をAPIの土台として使う。

- `EngineKernel`
  - エンジン全体の状態、Play / Stop / Pause / Step、GameLayer / EditorLayerの保持。
- `GameLayer`
  - 実シーンの `Registry` を保持。
- `Registry`
  - `CreateEntity`, `DestroyEntity`, `AddComponent`, `GetComponent` によるECS操作。
- `EditorLayer`
  - Scene View、Game View、Editor Camera、Scene load/save、選択状態、各エディタパネル。
- `PlayerEditorPanel`
  - State Machine、Timeline、Skeleton、Preview、Input Mappingなどのキャラクター編集。
- `EffectEditorPanel`
  - Effect graph、Preview、Timeline、Runtime overrideなどのエフェクト編集。
- `UIEditorPanel`
  - UI prefab、RectTransform、Canvas item、HP gauge templateなどのUI編集。
- `GameLoopEditorPanel`
  - GameFlow asset、node graph、scene transition、runtime registration。
- `SequencerPanel`
  - Cinematic sequence、entity binding、track/keyframe編集。
- `TerrainEditorPanel`
  - Terrain assetとbrush編集。
- `PrefabSystem`
  - Scene / Prefabの保存、読込、インスタンス化。
- `EditorSelection`
  - 選択中Entity / Assetの共有状態。
- `UndoSystem`
  - AI操作も通常のエディタ操作と同じくUndo対象にする。
- `ComponentMeta.generated.h`
  - Component名、Field名を利用した将来的な汎用Component編集APIの基盤。

## 基本設計

最初の実装は、ファイルベースのJSONコマンドキューとする。

理由:

- Winsock / HTTPサーバーを最初から持ち込まずに済む。
- デバッグしやすい。
- AI、外部スクリプト、人間が同じコマンドファイルを書ける。
- エンジンのメインスレッド上で安全にECSを触れる。
- ネットワークセキュリティ問題を後回しにできる。

将来、同じコマンドモデルをHTTP / WebSocket / 名前付きパイプへ拡張する。

## ディレクトリ構成

```text
Saved/AI/
  commands/
    *.json
  processing/
    *.json
  results/
    *.json
  screenshots/
    *.bmp
  state/
    latest_editor_state.json
  sessions/
    <session-id>/
      session.json
      events.jsonl
      screenshots/
```

`commands` に置かれたJSONをエンジンが読み、処理中は `processing` へ移動し、完了後に `results` へ結果JSONを書く。

## コマンド共通形式

```json
{
  "version": 1,
  "id": "cmd-0001",
  "command": "ping",
  "params": {}
}
```

結果形式:

```json
{
  "version": 1,
  "id": "cmd-0001",
  "ok": true,
  "command": "ping",
  "result": {
    "message": "pong"
  },
  "error": null
}
```

失敗時:

```json
{
  "version": 1,
  "id": "cmd-0001",
  "ok": false,
  "command": "set_transform",
  "result": null,
  "error": {
    "code": "entity_not_found",
    "message": "Entity is not alive.",
    "details": {
      "entity": 123
    }
  }
}
```

## MVPコマンド

### ping

エンジンが応答可能か確認する。

```json
{
  "version": 1,
  "id": "cmd-ping",
  "command": "ping",
  "params": {}
}
```

### get_engine_state

現在の実行モード、現在シーン、選択Entity、Scene View情報を返す。

返す情報:

- mode: `Editor`, `Play`, `Pause`
- currentScenePath
- selectedEntities
- primarySelectedEntity
- sceneViewMode
- sceneViewRect
- gameViewRect
- editorCamera

### list_entities

現在のRegistry内のEntity一覧を返す。

最小フィールド:

- entity
- name
- parent
- children
- components
- active

### get_entity

指定Entityの詳細を返す。

```json
{
  "version": 1,
  "id": "cmd-get-entity",
  "command": "get_entity",
  "params": {
    "entity": 4294967296
  }
}
```

### select_entity

EditorSelectionを更新する。

```json
{
  "version": 1,
  "id": "cmd-select",
  "command": "select_entity",
  "params": {
    "entity": 4294967296
  }
}
```

### create_empty

空のEntityを作成する。

```json
{
  "version": 1,
  "id": "cmd-create-empty",
  "command": "create_empty",
  "params": {
    "name": "Empty",
    "parent": null,
    "select": true
  }
}
```

作成するComponent:

- `NameComponent`
- `TransformComponent`
- `HierarchyComponent`

### create_model_entity

モデル付きEntityを作成する。

```json
{
  "version": 1,
  "id": "cmd-create-model",
  "command": "create_model_entity",
  "params": {
    "name": "Cube",
    "modelFilePath": "Data/Model/Cube/Cube.fbx",
    "position": [0.0, 1.0, 0.0],
    "rotation": [0.0, 0.0, 0.0, 1.0],
    "scale": [1.0, 1.0, 1.0],
    "parent": null,
    "select": true
  }
}
```

作成するComponent:

- `NameComponent`
- `TransformComponent`
- `HierarchyComponent`
- `MeshComponent`

`MeshComponent.modelFilePath` を設定し、ResourceManager経由でモデルインスタンスを生成する。

### set_transform

EntityのTransformを変更する。

```json
{
  "version": 1,
  "id": "cmd-set-transform",
  "command": "set_transform",
  "params": {
    "entity": 4294967296,
    "position": [0.0, 2.0, 0.0],
    "rotation": [0.0, 0.0, 0.0, 1.0],
    "scale": [1.0, 1.0, 1.0],
    "recordUndo": true
  }
}
```

未指定フィールドは維持する。

### delete_entity

EntityまたはSubtreeを削除する。

```json
{
  "version": 1,
  "id": "cmd-delete",
  "command": "delete_entity",
  "params": {
    "entity": 4294967296,
    "subtree": true,
    "recordUndo": true
  }
}
```

Prefab制約は `PrefabSystem::CanDelete` を通す。

### save_scene

現在のシーンを保存する。

```json
{
  "version": 1,
  "id": "cmd-save",
  "command": "save_scene",
  "params": {
    "path": "Data/Scene/EditorScene.scene"
  }
}
```

`path` 未指定時はEditorLayerの現在シーンパスを使う。

### load_scene

シーンを読み込む。

```json
{
  "version": 1,
  "id": "cmd-load",
  "command": "load_scene",
  "params": {
    "path": "Data/Scene/test.scene"
  }
}
```

### play / stop / pause / step

`EngineKernel` の再生制御を呼ぶ。

```json
{
  "version": 1,
  "id": "cmd-play",
  "command": "play",
  "params": {}
}
```

### set_editor_camera

Scene Viewカメラを指定する。

```json
{
  "version": 1,
  "id": "cmd-camera",
  "command": "set_editor_camera",
  "params": {
    "position": [0.0, 8.0, -12.0],
    "target": [0.0, 1.0, 0.0],
    "fovY": 0.785398
  }
}
```

### capture_screenshot

AIが画面確認に使う画像を書き出す。

```json
{
  "version": 1,
  "id": "cmd-shot",
  "command": "capture_screenshot",
  "params": {
    "target": "scene_view",
    "path": "Saved/AI/screenshots/scene_view_0001.bmp"
  }
}
```

Phase 2実装ではDX11/DX12 back buffer readbackを優先し、失敗時のみWin32 client captureにフォールバックする。
エンジンウィンドウ全体から必要な領域を切り出してBMPを書き出す。
`target` は `window`, `display`, `client`, `scene_view`, `game_view`, `effect_editor` を指定できる。
`path` を省略した場合は `Saved/AI/screenshots/<command id>.bmp` に保存する。

返す情報:

- path
- target
- width
- height

### get_visual_state

AIがスクリーンショットを読む前後で、画面上の対象位置を数値として取得する。

```json
{
  "version": 1,
  "id": "cmd-visual",
  "command": "get_visual_state",
  "params": {}
}
```

返す情報:

- sceneViewRect
- gameViewRect
- sceneViewMode
- editorCamera
- selectedVisuals
  - entity
  - name
  - localPosition
  - worldPosition
  - sceneScreenPosition
  - mesh

## エディター共通コマンド

専門エディターは、まず共通の窓口で状態取得、表示切替、保存、フォーカスを扱う。

### list_editors

利用可能なエディターと現在の表示状態を返す。

返す情報:

- editorId
- displayName
- visible
- activeWorkspace
- dirty
- currentAssetPath
- selectedContext
- supportedCommands

editorId例:

- `level`
- `player`
- `effect`
- `ui`
- `gameloop`
- `sequencer`
- `terrain`
- `asset_browser`
- `model_serializer`

### open_editor

指定エディターを開く。

```json
{
  "version": 1,
  "id": "cmd-open-player-editor",
  "command": "open_editor",
  "params": {
    "editor": "player",
    "focus": true
  }
}
```

### close_editor

指定エディターを閉じる。dirtyな編集がある場合は保存方針を指定する。

```json
{
  "version": 1,
  "id": "cmd-close-effect-editor",
  "command": "close_editor",
  "params": {
    "editor": "effect",
    "savePolicy": "ask"
  }
}
```

`savePolicy`:

- `ask`
- `save`
- `discard`
- `cancel_if_dirty`

### focus_editor

指定エディターまたはパネルにフォーカスする。

```json
{
  "version": 1,
  "id": "cmd-focus-ui-editor",
  "command": "focus_editor",
  "params": {
    "editor": "ui",
    "panel": "inspector"
  }
}
```

### get_editor_state

指定エディターのドメイン状態を取得する。

```json
{
  "version": 1,
  "id": "cmd-get-player-editor-state",
  "command": "get_editor_state",
  "params": {
    "editor": "player"
  }
}
```

### save_editor_asset

指定エディターが編集中のAssetを保存する。

```json
{
  "version": 1,
  "id": "cmd-save-effect",
  "command": "save_editor_asset",
  "params": {
    "editor": "effect",
    "path": "Data/Effect/VFX/Slash1.json"
  }
}
```

## 専門エディターAPI

以下はMVP後に段階的に追加する。重要度は高く、Level Editor APIと並行して育てる。

### Player Editor API

目的:

- キャラクターのゲームプレイ挙動をAIが編集できるようにする。
- State Machine、Timeline、Hitbox、VFX、Audioをゲームとして成立する単位で接続する。

主要コマンド:

- `player_editor.open_asset`
- `player_editor.set_preview_model`
- `player_editor.list_animations`
- `player_editor.create_state`
- `player_editor.delete_state`
- `player_editor.set_state_animation`
- `player_editor.create_transition`
- `player_editor.set_transition_condition`
- `player_editor.add_timeline_track`
- `player_editor.add_timeline_item`
- `player_editor.set_timeline_item`
- `player_editor.add_hitbox`
- `player_editor.add_vfx_event`
- `player_editor.add_audio_event`
- `player_editor.add_camera_shake`
- `player_editor.preview_play`
- `player_editor.preview_stop`
- `player_editor.save`

例:

```json
{
  "version": 1,
  "id": "cmd-add-hitbox",
  "command": "player_editor.add_hitbox",
  "params": {
    "track": "Attack",
    "startFrame": 12,
    "endFrame": 18,
    "bone": "RightHand",
    "shape": "sphere",
    "radius": 0.35,
    "damage": 20
  }
}
```

### Effect Editor API

目的:

- AIが攻撃、回避、被弾、UI演出に必要なVFXを編集・プレビューできるようにする。

主要コマンド:

- `effect_editor.open_asset`
- `effect_editor.create_graph`
- `effect_editor.add_node`
- `effect_editor.connect_node`
- `effect_editor.set_node_param`
- `effect_editor.delete_node`
- `effect_editor.set_timeline`
- `effect_editor.set_preview_mesh`
- `effect_editor.set_runtime_override`
- `effect_editor.preview_play`
- `effect_editor.preview_stop`
- `effect_editor.save`

例:

```json
{
  "version": 1,
  "id": "cmd-effect-param",
  "command": "effect_editor.set_node_param",
  "params": {
    "nodeId": 42,
    "param": "color",
    "value": [1.0, 0.25, 0.1, 1.0]
  }
}
```

### UI Editor API

目的:

- タイトル、HUD、HPゲージ、ボタン、ダメージ表示などをAIが作れるようにする。

主要コマンド:

- `ui_editor.open_prefab`
- `ui_editor.create_canvas`
- `ui_editor.create_sprite`
- `ui_editor.create_text`
- `ui_editor.create_button`
- `ui_editor.create_hp_gauge`
- `ui_editor.set_rect_transform`
- `ui_editor.set_canvas_item`
- `ui_editor.set_sprite`
- `ui_editor.set_text`
- `ui_editor.bind_hp_target`
- `ui_editor.preview_resolution`
- `ui_editor.save_prefab`

例:

```json
{
  "version": 1,
  "id": "cmd-create-hp-gauge",
  "command": "ui_editor.create_hp_gauge",
  "params": {
    "name": "BossHP",
    "position": [0.0, 420.0],
    "size": [960.0, 42.0],
    "binding": {
      "targetMode": "Boss"
    }
  }
}
```

### GameLoop Editor API

目的:

- AIがゲームの流れを作れるようにする。タイトル、ロード、バトル、リザルト、リトライ、遷移条件を編集対象に含める。

主要コマンド:

- `gameloop_editor.open_asset`
- `gameloop_editor.create_node`
- `gameloop_editor.delete_node`
- `gameloop_editor.connect`
- `gameloop_editor.set_start_node`
- `gameloop_editor.set_node_scene`
- `gameloop_editor.set_transition_condition`
- `gameloop_editor.register_runtime`
- `gameloop_editor.preview_event`
- `gameloop_editor.save`

例:

```json
{
  "version": 1,
  "id": "cmd-gameflow-node",
  "command": "gameloop_editor.create_node",
  "params": {
    "name": "Battle",
    "scenePath": "Data/Scene/test.scene",
    "position": [320.0, 140.0]
  }
}
```

### Sequencer API

目的:

- AIがカットシーン、カメラワーク、演出タイミングを編集できるようにする。

主要コマンド:

- `sequencer.open_asset`
- `sequencer.bind_entity`
- `sequencer.add_track`
- `sequencer.add_keyframe`
- `sequencer.set_keyframe`
- `sequencer.set_camera_cut`
- `sequencer.preview_play`
- `sequencer.preview_stop`
- `sequencer.save`

### Terrain Editor API

目的:

- AIが地形と戦闘フィールドを作れるようにする。

主要コマンド:

- `terrain_editor.create_terrain`
- `terrain_editor.open_terrain`
- `terrain_editor.set_brush`
- `terrain_editor.apply_brush_stamp`
- `terrain_editor.paint_layer`
- `terrain_editor.smooth`
- `terrain_editor.set_grass_layer`
- `terrain_editor.rebuild`
- `terrain_editor.save`

### Asset Browser API

目的:

- AIが使える素材を探し、配置や編集に接続できるようにする。

主要コマンド:

- `asset_browser.search`
- `asset_browser.list_folder`
- `asset_browser.select`
- `asset_browser.instantiate_prefab`
- `asset_browser.create_model_entity_from_asset`
- `asset_browser.assign_texture_to_selected`
- `asset_browser.import_external`

## エディター操作の設計原則

専門エディターの自動化では、次の順序でAPI化する。

1. ドメインデータを直接編集する。
2. 既存パネルのpublicメソッドを追加して呼ぶ。
3. 既存UI操作と同じUndo / Dirty処理を共通化する。
4. どうしても必要な場合だけImGui操作を補助的に使う。

AIがエディターを「開く」ことは視覚確認と人間への透明性のために重要である。一方で、AIが値を変える主経路は、ImGui widgetのクリックではなく、State Machine asset、Timeline asset、Effect graph、UI prefab、GameLoop assetなどのデータ構造への明示コマンドとする。

各専門エディターは最終的に以下の共通機能を持つ。

- open/load
- get_state
- select item
- create item
- update item
- delete item
- preview
- save
- dirty query
- undo/redo integration

## 汎用Component編集API

Phase 3で追加済み。

### get_component_schema

`ComponentMeta.generated.h` からComponent名、typeId、Field名、型、編集可否を返す。
`component` を省略すると全Component schemaを返す。

```json
{
  "version": 1,
  "id": "cmd-schema",
  "command": "get_component_schema",
  "params": {
    "component": "HealthComponent"
  }
}
```

### get_component

Entityが持つComponentの現在値を返す。

```json
{
  "version": 1,
  "id": "cmd-get-component",
  "command": "get_component",
  "params": {
    "entity": 4294967296,
    "component": "HealthComponent"
  }
}
```

### add_component

EntityへComponentを追加する。`fields` に指定した値は追加直後のComponentへ反映される。

```json
{
  "version": 1,
  "id": "cmd-add-health",
  "command": "add_component",
  "params": {
    "entity": 4294967296,
    "component": "HealthComponent",
    "fields": {
      "health": 150,
      "maxHealth": 150
    },
    "recordUndo": true
  }
}
```

### set_component_fields

Component名とField名で値を設定する。

```json
{
  "version": 1,
  "id": "cmd-set-light",
  "command": "set_component_fields",
  "params": {
    "entity": 4294967296,
    "component": "LightComponent",
    "fields": {
      "intensity": 3.5,
      "color": [1.0, 0.92, 0.78]
    },
    "recordUndo": true
  }
}
```

### remove_component

EntityからComponentを削除する。

```json
{
  "version": 1,
  "id": "cmd-remove-health",
  "command": "remove_component",
  "params": {
    "entity": 4294967296,
    "component": "HealthComponent",
    "recordUndo": true
  }
}
```

注意:

- `std::string`, `bool`, `int`, `float`, enum, `XMFLOAT2/3/4` を優先対応する。
- `std::vector`, `shared_ptr`, runtime-only field などはschema上で `editable:false` として返し、編集要求は構造化エラーにする。
- Runtime fieldは原則AIから直接編集しない。
- 変更はUndo対象にできる。`recordUndo:false` の場合だけ直接適用する。
- Transform系の変更ではDirtyを立て、編集後はHierarchy dirtyとPrefab overrideを更新する。

## Undo / Dirty / Prefab方針

AI操作も通常のエディタ操作と同じ編集履歴に入れる。

基本ルール:

- Transform変更は `ComponentUndoAction<TransformComponent>` を使う。
- Entity作成、削除、複製は `EntitySnapshot` ベースのUndo Actionを使う。
- Prefab配下の変更は `PrefabSystem::MarkPrefabOverride` を呼ぶ。
- Scene保存後はEditorLayer側の保存済みRevisionを更新する。
- Play中の編集は原則制限する。許可する場合はRuntime操作として明示する。

## メインスレッド安全性

ECS、Render resource、Editor UI状態はメインスレッドで触る。

ファイル監視やコマンド読込は以下のどちらかにする。

- MVP: `EngineKernel::Update()` の冒頭または末尾で同期的に処理する。
- 将来: 別スレッドでファイル検出だけ行い、実行キューをメインスレッドで処理する。

## 実装配置案

```text
Source/Automation/
  AIAutomationService.h
  AIAutomationService.cpp
  AIAutomationCommands.h
  AIAutomationJson.h
```

`EngineKernel` にメンバを追加する。

```cpp
std::unique_ptr<AIAutomationService> m_aiAutomationService;
```

ライフサイクル:

- `EngineKernel::Initialize()` で生成
- `EngineKernel::Update()` で `ProcessPendingCommands(*this)` を呼ぶ
- `EngineKernel::Finalize()` で破棄

## パスとセキュリティ

AI APIが扱えるファイルパスは原則プロジェクトルート配下に制限する。

許可する主なルート:

- `Data/`
- `Saved/AI/`
- `Saved/Logs/`

禁止:

- 絶対パスでプロジェクト外を書き換える
- `..` でプロジェクト外へ抜ける
- 任意実行コマンド
- 任意DLLロード

ネットワークAPIへ拡張する場合は、初期状態では `127.0.0.1` のみListenし、明示的に有効化されたときだけ起動する。

## エラーコード

代表的なエラーコード:

- `invalid_json`
- `unsupported_version`
- `unknown_command`
- `missing_param`
- `invalid_param`
- `entity_not_found`
- `component_not_found`
- `component_field_not_supported`
- `path_not_allowed`
- `file_not_found`
- `scene_load_failed`
- `scene_save_failed`
- `operation_not_allowed_in_play_mode`
- `internal_error`

## 実装ロードマップ

### Phase 1: File Command MVP

- `AIAutomationService` 追加
- `ping`
- `get_engine_state`
- `list_entities`
- `get_entity`
- `select_entity`
- `create_empty`
- `create_model_entity`
- `set_transform`
- `delete_entity`
- `save_scene`
- `load_scene`
- `play`, `stop`, `pause`, `step`
- path validation
- result filename sanitization
- sample command files

Phase 1のサンプルコマンドは `Docs/AI_COMMAND_SAMPLES/*.json` に置く。実行時は対象JSONを `Saved/AI/commands/` にコピーする。

Phase 1ではファイル操作を以下に制限する。

- Asset read: `Data/`
- Scene read/write: `Data/Scene/`, `Saved/AI/`
- Automation output: `Saved/AI/`, `Saved/Logs/`

Phase 1完了条件:

- エンジン起動時に `Saved/AI/commands`, `processing`, `results`, `state` が作成される。
- `commands/*.json` がメインスレッドで処理される。
- 成功/失敗が `results/<id>.json` に返る。
- `state/latest_editor_state.json` が更新される。
- Entity作成、取得、選択、Transform変更、削除がAPI経由でできる。
- Scene保存/読込、Play/Stop/Pause/StepがAPI経由でできる。
- 不正JSON、不明コマンド、不正Entity、不許可pathが構造化エラーで返る。
- `Game.vcxproj` の Debug x64 ビルドが通る。

### Phase 2: Visual Feedback

- `capture_screenshot`
- Scene View / Game View / Displayの保存
- `latest_editor_state.json` の常時更新
- `get_visual_state`
- `get_engine_state.visualState`
- 選択Entityのスクリーン座標、可視性の取得

Phase 2完了条件:

- `Saved/AI/screenshots` がエンジン起動時に作成される。
- `capture_screenshot` が `window`, `scene_view`, `game_view` のBMPを書き出せる。
- 書き出し先pathが `Saved/AI` または `Saved/Logs` 外の場合、構造化エラーで拒否される。
- `get_visual_state` がScene/Game View矩形、Editor Camera、選択Entityの画面座標を返す。
- `get_engine_state` と `state/latest_editor_state.json` に `visualState` が含まれる。
- `Game.vcxproj` の Debug x64 ビルドが通る。
- 実行中エンジンにコマンドを投入し、スクリーンショットファイルと結果JSONを確認できる。

### Phase 3: Generic Component API

- `get_component_schema`
- `get_component`
- `add_component`
- `remove_component`
- `set_component_fields`
- 型ごとのJSON変換
- Runtime-only fieldの編集制限

Phase 3完了条件:

- `get_component_schema` が単一Componentと全Component一覧のschemaを返す。
- schemaはfield名、型、編集可否を含む。
- `get_component` がEntity上のComponent値をJSONで返す。
- `add_component` が初期field指定、Undo統合、Dirty/Prefab更新込みで動作する。
- `set_component_fields` が対応型のfieldを更新し、未知fieldと非対応型を構造化エラーで拒否する。
- `remove_component` がUndo統合込みで動作する。
- `std::string`, `bool`, 整数, float, enum, `XMFLOAT2/3/4` をJSON変換できる。
- `Game.vcxproj` の Debug x64 ビルドが通る。
- 実行中エンジンにコマンドを投入し、追加、取得、更新、削除、エラー応答を確認できる。

### Phase 4: Higher Level Editing

- `instantiate_prefab`
- `duplicate_entity`
- `reparent_entity`
- `focus_entity`
- `frame_selection`
- `raycast_scene_view`
- `place_asset_at_cursor`
- Terrain / UI / Effect / PlayerEditor専用API

Phase 4で追加する高レベル編集コマンド:

### duplicate_entity

Entity subtreeを複製し、元Entityと同じ親の子として復元する。Prefab制約を確認し、Undoに登録する。

```json
{
  "version": 1,
  "id": "cmd-duplicate",
  "command": "duplicate_entity",
  "params": {
    "entity": 4294967296,
    "nameSuffix": " (Clone)",
    "select": true,
    "recordUndo": true
  }
}
```

### reparent_entity

Entityの親子関係を変更する。循環参照とPrefab制約を拒否し、必要ならworld transformを維持する。

```json
{
  "version": 1,
  "id": "cmd-reparent",
  "command": "reparent_entity",
  "params": {
    "entity": 4294967296,
    "parent": 8589934592,
    "keepWorldTransform": true,
    "recordUndo": true
  }
}
```

### instantiate_prefab

Prefab assetをSceneへ配置する。`Data/` 配下の `.prefab` のみ許可する。

```json
{
  "version": 1,
  "id": "cmd-prefab",
  "command": "instantiate_prefab",
  "params": {
    "path": "Data/Prefabs/Example.prefab",
    "position": [0.0, 0.0, 0.0],
    "parent": null,
    "select": true
  }
}
```

### focus_entity / frame_selection

Entityまたは現在選択中Entityのboundsを計算し、Scene View cameraを対象へ向ける。

### raycast_scene_view

Scene View座標からworld rayを作り、renderable meshとground planeへのhitを返す。

```json
{
  "version": 1,
  "id": "cmd-raycast",
  "command": "raycast_scene_view",
  "params": {
    "normalizedPosition": [0.5, 0.5],
    "includeGroundPlane": true,
    "maxDistance": 100000.0
  }
}
```

### place_asset_at_cursor

Scene View rayがground planeに当たった位置へModelまたはPrefabを配置する。Modelは `create_model_entity` と同じ経路で作る。

Phase 4完了条件:

- `duplicate_entity` がsubtree複製、選択更新、Undo統合込みで動作する。
- `reparent_entity` が親変更、world維持、循環拒否、Prefab制約、Undo統合込みで動作する。
- `focus_entity` と `frame_selection` がEditor cameraを対象へ向ける。
- `raycast_scene_view` がScene View座標からray、object hit、ground hitを返す。
- `place_asset_at_cursor` がScene View座標または明示positionからModel entityを配置できる。
- `instantiate_prefab` が許可pathのPrefabを配置し、不正pathや非Prefab拡張子を拒否する。
- `Game.vcxproj` の Debug x64 ビルドが通る。
- 実行中エンジンにコマンドを投入し、複製、親子付け、Focus、Raycast、配置、エラー応答を確認できる。

### Phase 4B: Level Editor Complete API

Level Editorの主要サブシステムを専用API化する。これによりAIはInspectorの汎用Component編集だけに頼らず、制作意図に近いコマンドでLevelを組める。

Asset Browser:

- `asset_browser.list`
- `asset_browser.search`
- `asset_browser.create_folder`
- `asset_browser.copy`
- `asset_browser.move`
- `asset_browser.rename`
- `asset_browser.delete`

Prefab:

- `prefab.save`
- `prefab.apply`
- `prefab.unpack`
- `instantiate_prefab`

Material:

- `material.create`
- `material.get`
- `material.set`
- `material.assign`

Lighting:

- `light.create`
- `set_component_fields` for `LightComponent`

Camera:

- `camera.create`
- `focus_entity`
- `frame_selection`
- `set_component_fields` for `CameraLensComponent`

Terrain:

- `terrain.create`
- `terrain.list`
- `terrain.apply_brush`
- `terrain.save`
- `terrain.load`

Phase 4B完了条件:

- Asset BrowserがData配下の列挙、検索、作成、コピー、移動、リネーム、削除をAPIで扱える。
- 削除は既定で `Data/.ai_trash` へ移動し、`permanent:true` のときだけ実削除する。
- Prefab保存、適用、Unpack、配置がAPIで扱える。
- Material assetを作成、取得、更新し、Entityへ割り当てられる。
- Light/Cameraを専用コマンドで作成でき、細かな値はComponent APIで編集できる。
- Terrainを作成、一覧取得、brush適用、保存、読込できる。
- すべてのファイル操作は `Data/` 配下へ制限され、プロジェクト外pathは拒否される。
- `Game.vcxproj` の Debug x64 ビルドが通る。
- 実行中エンジンに代表コマンドを投入し、成功/失敗JSONを確認できる。

### Phase 4C: Effect Editor Operation API

Effect Editorの操作は、EffectGraph assetをAIが直接編集し、人間のEffect Editorが同じassetを開ける形で保存する。対象ファイルは `Data/` 配下の `.effectgraph.json` に限定する。

大原則として、Effect Editor操作は人間が画面上で確認できるようにする。変更系コマンドは既定で `showWorkspace: true` として扱い、Effect Editorワークスペースを開き、対象アセットをロードし直して変更結果を表示する。大量生成などで明示的に裏側処理したい場合だけ `showWorkspace: false` を指定する。

Commands:

- `effect_editor.list_node_types`
- `effect_editor.create_asset`
- `effect_editor.apply_preset`
- `effect_editor.open_workspace`
- `effect_editor.set_preview_view`
- `effect_editor.get_state`
- `effect_editor.timeline_play`
- `effect_editor.timeline_seek`
- `effect_editor.timeline_step`
- `effect_editor.timeline_stop`
- `effect_editor.select_node`
- `effect_editor.focus_node`
- `effect_editor.assert_preview_visible`
- `effect_editor.capture_review_set`
- `effect_editor.capture_multi_time_review`
- `effect_editor.get_asset`
- `effect_editor.set_asset`
- `effect_editor.set_semantic_params`
- `effect_editor.add_node`
- `effect_editor.set_node`
- `effect_editor.delete_node`
- `effect_editor.connect`
- `effect_editor.disconnect`
- `effect_editor.compile`
- `effect_editor.preview_spawn`
- `visual.evaluate_capture`

Node types:

- `Output`
- `Spawn`
- `Lifetime`
- `MeshSource`
- `MeshRenderer`
- `ParticleEmitter`
- `SpriteRenderer`
- `Float`
- `Vec3`
- `Color`

`effect_editor.create_asset`:

```json
{
  "version": 1,
  "id": "cmd-effect-create",
  "command": "effect_editor.create_asset",
  "params": {
    "path": "Data/EffectGraph/AI/Slash.effectgraph.json",
    "name": "AI Slash",
    "graphId": "ai_slash",
    "previewDefaults": {
      "duration": 1.2,
      "seed": 7,
      "previewMeshPath": "Data/Model/Cube/Cube.fbx",
      "previewMaterialPath": "Data/Material/Default.material"
    }
  }
}
```

`effect_editor.add_node`:

```json
{
  "version": 1,
  "id": "cmd-effect-add-color",
  "command": "effect_editor.add_node",
  "params": {
    "path": "Data/EffectGraph/AI/Slash.effectgraph.json",
    "type": "Color",
    "position": [430.0, 340.0],
    "fields": {
      "title": "Slash Tint",
      "vectorValue": [1.0, 0.25, 0.08, 1.0],
      "vectorValue2": [1.0, 0.05, 0.0, 0.0]
    }
  }
}
```

`effect_editor.connect` は `startPinId` / `endPinId` を直接指定できる。指定しない場合は `fromNodeId`, `toNodeId`, `valueType`, `fromPin`, `toPin` からピンを解決する。接続制約はEffect Editor本体と同じで、Output -> Input、型一致、Input 1本、重複禁止。

```json
{
  "version": 1,
  "id": "cmd-effect-connect",
  "command": "effect_editor.connect",
  "params": {
    "path": "Data/EffectGraph/AI/Slash.effectgraph.json",
    "fromNodeId": 6,
    "toNodeId": 4,
    "valueType": "Color",
    "toPin": "Color"
  }
}
```

`effect_editor.preview_spawn` はコンパイル成功したEffectGraphをSceneへプレビューEntityとして生成する。付与される主なComponentは `NameComponent`, `TransformComponent`, `HierarchyComponent`, `EffectAssetComponent`, `EffectPlaybackComponent`, `EffectSpawnRequestComponent`、既定で `EffectPreviewTagComponent` も付与する。

`effect_editor.timeline_play` はEffect Editor画面を開き、対象アセットをロードし、Effect Editor内部のCompile/Preview/Timeline経路で再生を開始する。Timelineタブを選択し、プレビューEntityをEffect Editorの管理下に置くため、人間が画面上で再生状態を確認できる。

```json
{
  "version": 1,
  "id": "cmd-effect-timeline-play",
  "command": "effect_editor.timeline_play",
  "params": {
    "path": "Data/EffectGraph/AI/Slash.effectgraph.json",
    "startTime": 0.0,
    "paused": false
  }
}
```

`effect_editor.get_state` は現在のEffect Editor状態を返す。AIはこのコマンドで、開いているアセット、選択ノード、Stack/Nodeモード、compile dirty、compile結果、preview entity、timeline playbackを確認する。

```json
{
  "version": 1,
  "id": "cmd-effect-state",
  "command": "effect_editor.get_state",
  "params": {
    "path": "Data/EffectGraph/AI/Slash.effectgraph.json",
    "compile": true,
    "includeGraph": true
  }
}
```

`effect_editor.apply_preset` は参考画像制作の初期値を高速に作る高水準API。`spark`, `smoke`, `magic`, `slash` を指定でき、`semantic` で上書きできる。

```json
{
  "version": 1,
  "id": "cmd-effect-preset",
  "command": "effect_editor.apply_preset",
  "params": {
    "path": "Data/EffectGraph/AI/ReferenceMatch.effectgraph.json",
    "preset": "slash",
    "semantic": {
      "startColor": [0.2, 0.85, 1.0, 1.0],
      "endColor": [0.05, 0.15, 1.0, 0.0],
      "ribbonWidth": 0.16,
      "vortexStrength": 2.4
    }
  }
}
```

`effect_editor.set_semantic_params` は低レベルの `vectorValueN` を直接触らず、意味名で調整する。代表キーは `duration`, `spawnRate`, `burstCount`, `particleLifetime`, `startSize`, `endSize`, `speed`, `acceleration`, `drag`, `shape`, `shapeParams`, `spinRate`, `curlNoiseStrength`, `curlNoiseScale`, `curlNoiseScroll`, `vortexStrength`, `startColor`, `endColor`, `texture`, `drawMode`, `ribbonWidth`, `ribbonStretch`, `alphaScale`, `flipbookFps`, `sizeCurveBias`, `alphaCurveBias`, `subUvColumns`, `subUvRows`。

```json
{
  "version": 1,
  "id": "cmd-effect-semantic",
  "command": "effect_editor.set_semantic_params",
  "params": {
    "path": "Data/EffectGraph/AI/ReferenceMatch.effectgraph.json",
    "semantic": {
      "spawnRate": 65000.0,
      "particleLifetime": 0.8,
      "startSize": 0.22,
      "endSize": 0.02,
      "texture": "Data/Effect/particle/magic_03.png"
    }
  }
}
```

`effect_editor.set_preview_view` は比較条件を固定する。参考画像と比較する時は、カメラ距離、背景色、skyboxの有無を固定してから撮る。

```json
{
  "version": 1,
  "id": "cmd-effect-preview-view",
  "command": "effect_editor.set_preview_view",
  "params": {
    "target": [10000.0, 1.25, 10000.0],
    "yaw": 0.85,
    "pitch": -0.18,
    "distance": 4.5,
    "fovY": 0.785398,
    "clearColor": [0.04, 0.045, 0.055, 1.0],
    "useSkybox": false
  }
}
```

`effect_editor.timeline_seek` と `effect_editor.timeline_step` は、再生確認を時間軸で固定する。参考画像に近い一瞬を作る場合は、0.0秒、0.25秒、0.5秒、0.75秒のようにseekして撮影する。

```json
{
  "version": 1,
  "id": "cmd-effect-seek",
  "command": "effect_editor.timeline_seek",
  "params": {
    "path": "Data/EffectGraph/AI/Slash.effectgraph.json",
    "time": 0.35,
    "paused": true
  }
}
```

`effect_editor.select_node` はDetailsに対象ノードを出し、`effect_editor.focus_node` はNodeモードへ切り替えてノード確認を優先する。AIが内部編集したノードを人間が見られる状態にするため、変更後は該当ノードを選択する。

```json
{
  "version": 1,
  "id": "cmd-effect-focus-node",
  "command": "effect_editor.focus_node",
  "params": {
    "nodeId": 4
  }
}
```

`effect_editor.assert_preview_visible` は、Effect Editor管理下のpreview entity、compile結果、render descriptor、playback状態を確認する。必要な場合だけ `assertSceneVisible: true` を指定し、Scene View上の投影確認も組み合わせる。

```json
{
  "version": 1,
  "id": "cmd-effect-assert",
  "command": "effect_editor.assert_preview_visible",
  "params": {
    "requireRenderable": true,
    "requirePlayback": true
  }
}
```

`effect_editor.capture_review_set` はEffect Editor専用の視覚レビュー束を作る。対象アセットを開き、compileし、timelineを指定時刻へseekし、Effect Editor全体とwindowを撮影し、preview assertionと状態JSONを返す。

```json
{
  "version": 1,
  "id": "cmd-effect-review",
  "command": "effect_editor.capture_review_set",
  "params": {
    "path": "Data/EffectGraph/AI/Slash.effectgraph.json",
    "time": 0.35,
    "paused": true,
    "stem": "slash_iter_001",
    "dir": "Saved/AI/screenshots/effect_review",
    "targets": ["effect_editor", "window"],
    "assertPreview": true
  }
}
```

`effect_editor.capture_multi_time_review` は開始、ピーク、減衰の複数時刻をまとめて撮影し、各フレームの画像メトリクスを返す。

```json
{
  "version": 1,
  "id": "cmd-effect-multi-review",
  "command": "effect_editor.capture_multi_time_review",
  "params": {
    "path": "Data/EffectGraph/AI/ReferenceMatch.effectgraph.json",
    "times": [0.0, 0.25, 0.5, 0.9],
    "target": "effect_editor",
    "settleFrames": 2,
    "stem": "reference_match_v003",
    "dir": "Saved/AI/screenshots/effect_review"
  }
}
```

When called through WebSocket, `effect_editor.capture_multi_time_review` runs as a frame-crossing review job: it seeks the timeline, waits at least two rendered frames (`settleFrames`, minimum 2; the initial focus/open pass waits one extra frame), captures, then advances to the next requested time. This keeps the human-visible Effect Editor tab, timeline, and screenshot output in sync instead of reading the same stale backbuffer multiple times.

`visual.evaluate_capture` は任意の撮影対象を画像解析し、平均明度、彩度、発光/明部比率、背景差分から推定したeffect bounds、dominant colorを返す。参考画像との完全比較そのものはAIの視覚判断で行うが、この数値は「暗すぎる」「画面を埋めすぎる」「色が違いすぎる」を検出する補助になる。

```json
{
  "version": 1,
  "id": "cmd-eval-effect",
  "command": "visual.evaluate_capture",
  "params": {
    "target": "effect_editor",
    "save": true,
    "path": "Saved/AI/screenshots/effect_review/eval.bmp"
  }
}
```

正確に目視レビューする場合は、dock tabが前面化する1フレームを待つ。推奨ループは `effect_editor.open_workspace` または `effect_editor.timeline_seek`、`editor.focus_panel(effect_editor)`、1フレーム待機、`capture_screenshot(target:"effect_editor")`、画像確認、修正、再seek、再撮影である。

参考画像からAIがエフェクトを作る時の制作ループ:

1. 参考画像を観察し、形状、色、発光、粒子密度、動き、寿命、カメラ距離を言語化する。
2. EffectGraphを作成または更新し、主要ノードを追加する。
3. `effect_editor.focus_node` で編集箇所を人間に見える状態にする。
4. `effect_editor.timeline_seek` で代表時刻へ固定する。
5. `effect_editor.assert_preview_visible` と `capture_screenshot(target:"effect_editor")` を実行する。
6. スクショを参考画像と比較し、色、サイズ、発生位置、速度、寿命、レンダラー設定を反復調整する。

Phase 4C完了条件:

- AIがEffectGraph assetを作成、読み取り、更新、保存できる。
- AIがノード追加、ノード値変更、ノード削除、リンク接続、リンク解除を行える。
- AIがEffectCompilerでコンパイル結果、warnings、errors、execution plan、mesh/particle descriptor概要を取得できる。
- AIがコンパイル済みEffectGraphをプレビューEntityとしてSceneに生成できる。
- AIがEffect Editorの状態、選択ノード、Timeline時刻、Preview再生状態を取得できる。
- AIがEffect Editor画面を前面化し、Timelineタブとプレビューをスクショで確認できる。
- AIがpreview entity、compile結果、render descriptor、playback状態をassertできる。
- AIがプリセットと意味パラメータAPIで、参考画像に近い初期案を低レベルfield名なしに生成できる。
- AIが複数時刻レビューと画像メトリクスで、開始、ピーク、減衰を反復確認できる。
- AIがPreviewカメラ、背景、skybox条件を固定し、比較のブレを抑えられる。
- すべてのEffectGraphファイル操作は `Data/` 配下に制限され、パストラバーサルを拒否する。
- `Game.vcxproj` の Debug x64 ビルドが通り、実行中エンジンで代表コマンドの成功/失敗JSONを確認できる。

### AI Observation Windows: ECS / Visual Verification

These commands add AI-facing observation windows on top of the generic Entity/Component API.

#### ecs.query

Filter ECS entities without pulling the whole scene.

```json
{
  "version": 1,
  "id": "cmd-ecs-query",
  "command": "ecs.query",
  "params": {
    "hasComponents": ["TransformComponent", "MeshComponent"],
    "missingComponents": ["PrefabInstanceComponent"],
    "nameContains": "Player",
    "activeOnly": true,
    "rootsOnly": false,
    "includeDetails": false,
    "limit": 32
  }
}
```

Returns `entities`, `count`, and `truncated`. Set `includeDetails` to include reflected component field data.

#### ecs.hierarchy

Return a relationship tree for the whole scene or a specific root.

```json
{
  "version": 1,
  "id": "cmd-ecs-hierarchy",
  "command": "ecs.hierarchy",
  "params": {
    "root": null,
    "maxDepth": 8,
    "includeComponents": true,
    "includeReferences": true
  }
}
```

Returns `roots` plus `references` for assets and authoring links such as mesh, material, prefab, effect, and light data.

#### ecs.diff

Compare the current ECS snapshot against the previous call.

```json
{
  "version": 1,
  "id": "cmd-ecs-diff",
  "command": "ecs.diff",
  "params": {
    "reset": false,
    "includeBeforeAfter": true
  }
}
```

The first call or `reset: true` stores a baseline. Later calls return `added`, `removed`, and `changed` entities.

#### visual.verify_entity

Connect ECS identity to what the AI can verify in Scene View.

```json
{
  "version": 1,
  "id": "cmd-visual-verify",
  "command": "visual.verify_entity",
  "params": {
    "entity": "38654705664"
  }
}
```

Returns bounds, renderable flags, selected state, references, and Scene View screen projection. `sceneView.visibleInSceneView` tells whether the entity center is inside the current Scene View.

#### gameplay.get_state

Return a gameplay-focused observation window for action-game debugging. This is not a raw ECS dump; it summarizes actors, battle flow, projectiles, and recent damage events in the shape an AI needs for playability checks.

```json
{
  "version": 1,
  "id": "cmd-gameplay-state",
  "command": "gameplay.get_state",
  "params": {
    "includeVisual": true,
    "includeInput": true,
    "includeDamageEvents": true,
    "eventLimit": 32
  }
}
```

The response includes:

- `actors`: player/enemy/gameplay entities with HP, stamina, team, transform, action state, locomotion, physics, animator, timeline/playback, state machine, lock-on, hitbox, input, and optional visual verification.
- `battle.flows`: BattleFlow phase, timer, player/boss/arena links, battle id, encounter radius, intro duration.
- `battle.rules`: BattleRules authoring settings.
- `projectiles`: active projectile owner, target side, damage, radius, lifetime, velocity, and position.
- `damageEvents`: recent damage event data from legacy DamageEventComponent and the runtime damage queue.

#### game.input.press / release / tap

Inject virtual input into the same `InputEventQueue` used by SDL input. Prefer action names when a player/input entity has an `InputActionMapComponent`; raw scancode/mouse/gamepad inputs are also supported.

```json
{
  "version": 1,
  "id": "cmd-input-attack",
  "command": "game.input.tap",
  "params": {
    "playerId": 0,
    "action": "Attack",
    "holdFrames": 1
  }
}
```

Raw keyboard example:

```json
{
  "version": 1,
  "id": "cmd-input-key",
  "command": "game.input.press",
  "params": {
    "scancode": 4
  }
}
```

`tap` injects press now and schedules release on a later frame. `press` and `release` can also target `mouseButton`, `gamepadButton`, or an `action`.

#### game.input.axis / mouse_move

Inject analog axis or mouse motion.

```json
{
  "version": 1,
  "id": "cmd-input-move",
  "command": "game.input.axis",
  "params": {
    "playerId": 0,
    "axis": "MoveX",
    "value": 1.0
  }
}
```

If the named axis has a gamepad binding, a virtual gamepad-axis event is injected. If the axis is keyboard-backed, the positive/negative key is pressed and the opposite key is released. Raw `gamepadAxis` is also supported.

#### game.play / pause / stop / step_frames / set_time_scale

Control Play mode and deterministic frame stepping for AI gameplay tests.

```json
{
  "version": 1,
  "id": "cmd-play",
  "command": "game.play",
  "params": {}
}
```

```json
{
  "version": 1,
  "id": "cmd-step",
  "command": "game.step_frames",
  "params": {
    "frames": 30
  }
}
```

`game.step_frames` schedules N paused-frame advances. The response includes `mode`, `frameCount`, `timeScale`, and `pendingStepFrames`.

```json
{
  "version": 1,
  "id": "cmd-timescale",
  "command": "game.set_time_scale",
  "params": {
    "timeScale": 0.5
  }
}
```

#### visual.verify_entity_game_view

Verify whether an ECS entity is visible in the actual Game View camera projection.

```json
{
  "version": 1,
  "id": "cmd-gameview-verify",
  "command": "visual.verify_entity_game_view",
  "params": {
    "entity": "38654705664"
  }
}
```

The command returns `gameViewRect`, camera kind/entity, screen position, NDC, and `visibleInGameView`. It supports 3D `CameraMainTagComponent` cameras and 2D `Camera2DComponent` projections.

#### scene_view.frame_entities / frame_all

Move the visible Scene View camera before taking review screenshots. This is the first guard against the AI judging a level from a stale or useless camera angle.

```json
{
  "version": 1,
  "id": "cmd-frame-scene",
  "command": "scene_view.frame_entities",
  "params": {
    "entities": ["38654705664", "38654705665"],
    "yawDegrees": 35.0,
    "pitchDegrees": 28.0,
    "padding": 1.35
  }
}
```

`scene_view.frame_all` collects renderable/transform/terrain entities and frames the whole authored level.

#### camera.frame_entities

Move the actual main 3D game camera so Game View can be visually reviewed from the intended gameplay angle.

```json
{
  "version": 1,
  "id": "cmd-frame-game-camera",
  "command": "camera.frame_entities",
  "params": {
    "entities": ["38654705664"],
    "camera": null,
    "yawDegrees": 0.0,
    "pitchDegrees": 16.0,
    "padding": 1.35
  }
}
```

If `camera` is omitted, the command uses the first live `CameraMainTagComponent` entity with a `TransformComponent`.

#### visual.assert_entities_visible

Batch-check that selected or gameplay entities are visible in `scene_view` or `game_view`.

```json
{
  "version": 1,
  "id": "cmd-assert-visible",
  "command": "visual.assert_entities_visible",
  "params": {
    "view": "game_view",
    "entities": ["38654705664"],
    "requireAll": true,
    "minVisibleRatio": 1.0,
    "requireBoundsFullyVisible": true,
    "minMarginPixels": 8.0,
    "maxFillRatio": 0.88
  }
}
```

Returns `ok`, `visibleCount`, `total`, `visibleRatio`, and per-entity projection details. By default the assertion requires the entity bounds, not only the center point, to fit inside the reviewed view with margin.

#### visual.capture_review_set

Capture the AI review bundle after optionally framing Scene View and the gameplay camera. This is the preferred command for visual QA loops because it returns screenshots, visibility assertions, engine state, and gameplay state in one response.

```json
{
  "version": 1,
  "id": "cmd-review",
  "command": "visual.capture_review_set",
  "params": {
    "stem": "arena_iteration_001",
    "dir": "Saved/AI/screenshots/review",
    "format": "bmp",
    "targets": ["scene_view", "game_view", "window"],
    "entities": ["38654705664"],
    "frameSceneView": true,
    "frameGameCamera": true,
    "assertVisible": true,
    "assertView": "game_view"
  }
}
```

The loop rule is: edit through normal editor/API commands, frame the review cameras, focus the exact panel to be reviewed with `editor.focus_panel`, wait at least one rendered frame, capture `scene_view` or `game_view`, assert visibility, inspect the screenshots, then adjust placement/camera/UI until both the numeric assertions and visual review pass.

For exact screenshot review, do not assume a docked tab is active. Use this sequence:

```json
{ "version": 1, "id": "focus-game", "command": "editor.focus_panel", "params": { "panel": "game_view" } }
```

Wait one rendered frame, then:

```json
{
  "version": 1,
  "id": "capture-focused-game",
  "command": "capture_screenshot",
  "params": {
    "target": "game_view",
    "path": "Saved/AI/screenshots/review/focused_game_view.bmp",
    "format": "bmp"
  }
}
```

Repeat the same pattern for `scene_view`. This avoids reviewing a stale dock tab or the wrong panel contents.

#### gameplay.get_events / clear_events

Read recent gameplay events for AI test assertions.

```json
{
  "version": 1,
  "id": "cmd-gameplay-events",
  "command": "gameplay.get_events",
  "params": {
    "includeFlow": true,
    "includeDamage": true,
    "limit": 64,
    "clear": false
  }
}
```

Returns `flowEvents` from the GameLoop flow event history and `damageEvents` from recent runtime damage events. `gameplay.clear_events` clears the retained damage event history.

### Phase 5: Live Protocol

- 名前付きパイプ、HTTP、またはWebSocketに対応
- 外部AIツールから低遅延操作
- コマンドストリームとイベント購読

## 成功条件

MVPの成功条件:

- エンジン起動中に外部からJSONコマンドを投げられる。
- AIがEntity一覧を取得できる。
- AIがEntityを作成し、Transformを変更し、選択できる。
- AIがシーンを保存、ロードできる。
- AIがPlay / Stop / Stepで挙動確認できる。
- すべての編集がクラッシュせず、Undoまたは明確な復元手段を持つ。

最終的な成功条件:

- AIが画面確認とAPI操作を組み合わせ、レベル配置、カメラ調整、UI調整、ゲームプレイ調整を反復できる。
- 人間はAIの操作結果をエディタ上で普通に確認し、必要なら手作業で続きから編集できる。
- AIの操作は通常のエディタ操作と同じ保存形式に落ちる。

## 非目標

初期段階では以下を目標にしない。

- 任意Windowsアプリの完全なマウス自動操作
- 外部ネットワーク公開API
- すべてのComponent fieldの完全自動編集
- Play中Runtime stateの完全な永続化
- Unreal / Unity互換API

## 共有したい判断基準

このAPIは「AI専用の裏口」ではなく、「エディタが本来持つべき自動化レイヤー」として設計する。

そのため、実装判断では次を優先する。

- 画面座標よりEntity ID
- マウス操作より明示コマンド
- 一時的なRuntime変更よりSceneに保存されるAuthoring変更
- 特殊ケースの即席実装よりUndo / Prefab / Dirtyと整合する編集
- AIだけが使える機能より、人間のツールやCIからも使える機能

この方向で作ると、AIは単なる外部操作者ではなく、このエンジンの編集ワークフローに自然に参加できる。
