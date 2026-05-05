# New Scene 2D/3D Mode Selection Spec

作成日: 2026-05-05
対象:
- `File > New Scene` メニュー、Mode 選択 modal、`EditorLayer::NewScene`、`CreateDefaultSceneEntities`
- `Camera2DComponent`（フィールド拡張）、`Camera2DMainTagComponent`（新規）、`Camera2DUtils`（新規）
- `Source/Engine/EngineKernel.cpp` / `Source/Layer/EditorLayerSceneView.cpp` の 2D 投影計算と Camera2D 選択
- `Source/Asset/PrefabSystem.cpp` の Camera2D シリアライズ
- `Source/Inspector/InspectorECSUI.cpp` の Camera2D 専用 Inspector UI
- `Source/Hierarchy/HierarchyECSUI.cpp` の `BuildCamera2DSnapshot()`
- Scene View grid のモード別永続化 (`m_showSceneGrid` 分割)

---

## 1. 背景

現状の `New Scene` は 3D 前提で作られており、2D シーン（タイトル画面、UI 画面、HUD レイアウト等）を作るユーザーから見ると「3D シーンを作ってから手作業で 3D 用 entity を消し、Camera2D を追加し、View を 2D に切り替える」という不自然な手順を踏まされる。

現状確認:

- `EditorLayer::NewScene()` ([Source/Layer/EditorLayerSceneIO.cpp:221](Source/Layer/EditorLayerSceneIO.cpp:221)) は無条件で 3D シーンを生成する。
  - `ClearRegistryEntities(registry)` 後、`CreateDefaultSceneEntities(registry)` を呼ぶ。
  - 末尾で `m_sceneViewMode = SceneViewMode::Mode3D` を強制する。
- `CreateDefaultSceneEntities()` ([Source/Layer/EditorLayerInternal.h:1171](Source/Layer/EditorLayerInternal.h:1171)) は 3D 専用 entity を生成する。
  - `Main Camera` (`CameraFreeControlComponent` + `CameraLensComponent` + `CameraMatricesComponent` + `CameraMainTagComponent` + `AudioListenerComponent`、位置 `{0, 2, -10}`)
  - `Directional Light` (`LightType::Directional`、45° 回転)
  - `Reflection Probe` (`needsBake = true`、半径 20m)
  - `Environment` (`EnvironmentComponent`)
  - `Audio Settings` (`AudioSettingsComponent`)
- 2D 用の既定 entity は一切生成されない。`Camera2DComponent` を持つ entity がないので Game View は `TryBuildGameView2DViewProjection()` ([Source/Layer/EditorLayerSceneView.cpp:535](Source/Layer/EditorLayerSceneView.cpp:535)) が false を返す状態のままになる。
- 2D Camera 自体の生成は既に存在する (`BuildCamera2DSnapshot()` [Source/Hierarchy/HierarchyECSUI.cpp:489](Source/Hierarchy/HierarchyECSUI.cpp:489)) が、`New Scene` から呼ばれない。Hierarchy の右クリック `Create 2D Camera` 経由でしか作れない。
- メニューは `File > New Scene` ([Source/Layer/EditorLayerMenu.cpp:560](Source/Layer/EditorLayerMenu.cpp:560)) が `m_requestNewScene = true` を立てるだけで、ダイアログは存在しない。`View > 3D Mode / 2D Mode` トグル ([Source/Layer/EditorLayerMenu.cpp:624-629](Source/Layer/EditorLayerMenu.cpp:624)) は新規シーン生成と完全に独立している。
- シーン保存側は既に `SceneFileMetadata::sceneViewMode` ([Source/Asset/PrefabSystem.h:15-20](Source/Asset/PrefabSystem.h:15)) に `"2D"` / `"3D"` を文字列で保持しており、`SceneViewModeFromString()` ([Source/Layer/EditorLayerInternal.h:466](Source/Layer/EditorLayerInternal.h:466)) で復元している。**ファイル形式は 2D に対応済みで、入力 UI と既定 entity 生成だけが欠けている**。

そのため、ユーザーから見ると「2D はおまけ。タイトル画面のような 2D 構成は作りにくい」状態になっている。

---

## 2. 最優先ゴール

1. `File > New Scene` 実行時に **Mode 選択ダイアログ** が出る。`2D` / `3D` のどちらかを必ず選んでから新規シーンが作られる。
2. `2D` を選んだ場合、生成直後の registry に Camera2D を含む 2D 既定 entity 一式があり、Scene View が `Mode2D`、Game View が即座に `2D camera 経由の orthographic 投影` で表示される。
3. `3D` を選んだ場合、現状と同じ 3D 既定 entity 構成（Main Camera / Directional Light / Reflection Probe / Environment / Audio Settings）になり、Scene View が `Mode3D` になる。
4. ダイアログで選んだモードは `SceneFileMetadata::sceneViewMode` に保存され、再読込時に Scene View モードが正しく復元される（保存・復元の経路は既存のものを使う）。
5. Hierarchy / Inspector / Game View / Scene View の各 panel は、シーン生成直後から「タイトル作りに必要な操作（Sprite, Text, UI Button の Create と編集）」がそのまま使える状態になる。
6. 既存の 3D シーンの読込・保存・autosave 動作は変更しない。

---

## 3. 非ゴール

今回の Phase では以下を必須にしない。

- Scene View / Game View / Inspector の 2D 編集体験そのものの再設計（既存 spec `Sprite_2DUI_Improvement_Spec_2026-05-02.md`、`CreateText_2DUI_Improvement_Spec_2026-05-03.md`、`UIButton_2DUI_Improvement_Spec_2026-05-04.md` の責務）。
- 2D 専用の新規 panel・新規 window の追加。
- Scene 内で 2D / 3D を**混在**させるための機能（カメラ切替・レイヤ分離など）。Mode は scene 単位の属性として扱う。
- New Scene ダイアログでの解像度・アスペクト比指定（Game View 側の resolution preset を流用するため、ダイアログでは扱わない）。
- Template / Preset の永続化（カスタム既定 entity セットをユーザーが保存する機能）。
- Unsaved changes ポップアップの再設計。Mode 選択ダイアログは「保存破棄確認 → Mode 選択 → 生成」の順で出す。
- カメラ追従 (`followTarget`)、カメラシェイク、ダンピング、ビューポート分割（split-screen）、レイヤカリングマスク。これらは Phase 3 以降の課題とする。

---

## 4. UX 原則

### 4.1 Mode は New Scene の入口で必ず選ぶ

`File > New Scene` を選ぶと、まず Mode 選択ダイアログが出る。この選択を経ないと新規シーンは作られない。

許可:

- ダイアログで `2D` / `3D` のラジオを選び、`Create` ボタンで生成。
- `Cancel` でダイアログを閉じ、現在のシーンをそのまま維持。
- Unsaved changes 時は既存の保存破棄確認ポップアップを先に通す。確認後に Mode 選択ダイアログを開く。
- キーボード操作: `←` `→` で Mode 切替、`Enter` で `Create`、`Esc` で `Cancel`。

禁止:

- ダイアログを経由しないで `New Scene` を実行する（旧来の 3D 直接生成）。
- ダイアログで `2D` を選んだのに 3D Camera が混入する、もしくはその逆。
- `New Scene` 実行時に View モードと既定 entity が乖離する。

### 4.2 Mode 選択は scene 単位

Mode は scene ファイルに属する。同じ scene を別エディタセッションで開いても、保存された `sceneViewMode` に従って Scene View モードと初期表示が決まる。

許可:

- 既存 scene を開いた後、ユーザーが `View > 2D Mode / 3D Mode` トグルで一時的に表示を切り替える（既存挙動を維持）。
- View トグルでの切替は `m_sceneViewMode` のみを更新する。registry の entity 構成は変えない。

禁止:

- Mode 選択ダイアログから既存 scene の Mode を「変換」する操作。これは別 Phase の課題とする。
- View トグルでの切替を「scene の Mode を書き換えた」と誤認させる UI。

### 4.3 タイトル作りに必要な最小構成を即座に提供する

`2D` を選んだ直後、ユーザーが Hierarchy の右クリック → `Create Sprite` / `Create Text` / `Create UI Button` を呼べば、何も追加準備せずに Game View で見える entity が増える状態にする。

許可:

- 2D 既定 entity に `Camera 2D` を含める（Game View の 2D 投影ソース）。Main 2D Camera タグ付きにする（後述 §6.5）。
- 2D 既定 entity に `Audio Settings` を含める（音量等の global 設定。3D と同じ）。
- 2D シーンを生成した直後の Scene View grid は **OFF** にする。タイトル / UI 作りでは grid が視界を邪魔するため。ユーザーは `View > Overlays > Show Grid` で随時 ON にできる。

禁止:

- 2D 既定 entity に Directional Light / Reflection Probe / Environment を入れる。これらは 3D 専用で、2D シーンでは Inspector に余計な選択肢を出すだけ。
- 2D 既定 entity に Main Camera (3D) を残す。
- 2D シーン生成直後に grid が出る挙動。3D は従来通り grid ON で開始する。

---

## 5. New Scene ダイアログ仕様

### 5.1 起動経路

1. `File > New Scene` ([Source/Layer/EditorLayerMenu.cpp:560](Source/Layer/EditorLayerMenu.cpp:560)) のメニュー項目押下。
2. `Ctrl + N` ショートカット。
3. Scene View 上の `New Scene` トースト/ヒント経路 ([Source/Layer/EditorLayerSceneView.cpp:190](Source/Layer/EditorLayerSceneView.cpp:190))。

これらはいずれも従来通り `m_requestNewScene = true` を立てる。`ProcessDeferredEditorActions()` ([Source/Layer/EditorLayerSceneIO.cpp:81](Source/Layer/EditorLayerSceneIO.cpp:81)) 内で `RequestSceneAction(PendingSceneAction::NewScene)` を呼ぶ部分は維持し、後段の `ExecutePendingSceneAction()` ([Source/Layer/EditorLayerSceneIO.cpp:190](Source/Layer/EditorLayerSceneIO.cpp:190)) の `case PendingSceneAction::NewScene` の中で **直接 `NewScene()` を呼ぶ前に Mode 選択ダイアログを開く** ように差し替える。

### 5.2 ダイアログの状態機械

```
                +-------------------+
                |   (Idle)          |
                +-------------------+
                          |
                  request New Scene
                          |
                          v
                +-------------------+
                | Unsaved? -- yes --+--> 保存破棄確認 popup（既存）
                +-------------------+         |
                          | no                | "Discard"
                          v                   v
                +-------------------+   +-------------------+
                | NewSceneModeDialog|<--+ open dialog        |
                +-------------------+
                  |        |       |
              Cancel    2D Create  3D Create
                  |        |       |
                  v        v       v
                Idle   NewScene2D  NewScene3D
```

`Save` を選んだ場合は保存後にダイアログへ遷移する。`Cancel` を選んだ場合は `m_pendingSceneAction` をクリアして Idle に戻す。これは既存の保存破棄ポップアップ処理と同じ流れ。

### 5.3 ダイアログのレイアウト

ImGui modal で実装する。サイズは固定で良い（だいたい 480 x 280 px）。中央寄せ。

要素:

- タイトル: `New Scene`
- サブタイトル: `Choose a scene mode.`
- ラジオ 2 つ:
  - `[ ] 3D Scene` — 説明: `3D camera, directional light, reflection probe, environment.`
  - `[ ] 2D Scene` — 説明: `2D camera, orthographic view, grid for UI / title screens.`
- 既定選択: 直近に作成または開いた scene の `sceneViewMode`（無ければ `3D`）。
- 補足チェックボックス（Phase 1 では不要、Phase 2 候補）: `Set Scene View grid to match mode`
- ボタン:
  - `Create`（primary、Enter で発火）
  - `Cancel`（Esc で発火）

ダイアログは `ImGui::OpenPopup("New Scene Mode")` + `ImGui::BeginPopupModal("New Scene Mode", ...)` で出す。`m_openNewSceneModePopup` フラグを `EditorLayer` に追加する。

### 5.4 ダイアログ確定時の処理

`Create` が押されたとき、選択された Mode を `EditorLayer::NewScene(SceneViewMode mode)` に渡して呼び出す。

```cpp
void EditorLayer::NewScene(SceneViewMode mode);
```

既存の引数なし `NewScene()` は **削除**（呼び出し元はダイアログ経由の `NewScene(mode)` のみになる）。

---

## 6. NewScene 実装変更

### 6.1 シグネチャ変更

`EditorLayer::NewScene()` を `EditorLayer::NewScene(SceneViewMode mode)` に変更する。

```cpp
// EditorLayer.h（差分イメージ）
- void NewScene();
+ void NewScene(SceneViewMode mode);
```

呼び出し元:

- `ExecutePendingSceneAction()` の `case PendingSceneAction::NewScene` 内では呼ばない。代わりに **Mode 選択ダイアログを開く要求フラグ** をセットする。
- ダイアログの `Create` ハンドラから `NewScene(selectedMode)` を呼ぶ。

### 6.2 既定 entity 生成の Mode 分岐

`CreateDefaultSceneEntities(registry)` を 2 つに分ける（または mode を引数に取る）。実装案:

```cpp
// EditorLayerInternal.h（差分イメージ）
void CreateDefaultSceneEntities3D(Registry& registry); // 既存実装をそのまま
void CreateDefaultSceneEntities2D(Registry& registry); // 新規

void CreateDefaultSceneEntities(Registry& registry, EditorLayer::SceneViewMode mode)
{
    if (mode == EditorLayer::SceneViewMode::Mode2D) {
        CreateDefaultSceneEntities2D(registry);
    } else {
        CreateDefaultSceneEntities3D(registry);
    }
}
```

#### 6.2.1 `CreateDefaultSceneEntities3D`

[Source/Layer/EditorLayerInternal.h:1171](Source/Layer/EditorLayerInternal.h:1171) の現行実装をそのまま維持する:

- `Main Camera`(`TransformComponent { 0, 2, -10 }` + `CameraFreeControlComponent` + `CameraLensComponent` + `CameraMatricesComponent` + `CameraMainTagComponent` + `AudioListenerComponent`)
- `Directional Light`(`LightType::Directional`、45°/45°/0°、強度 1.0、白色)
- `Reflection Probe`(中心 `{ 0, 1.5, 0 }`、半径 20m、`needsBake = true`)
- `Environment`(`EnvironmentComponent`)
- `Audio Settings`(`AudioSettingsComponent`)

#### 6.2.2 `CreateDefaultSceneEntities2D`

新規実装。タイトル / UI 制作向きの既定値で `Camera 2D` を生成する。

```cpp
void CreateDefaultSceneEntities2D(Registry& registry)
{
    EntityID camera2DEntity = registry.CreateEntity();
    registry.AddComponent(camera2DEntity, NameComponent{ "Camera 2D" });

    TransformComponent camTrans;
    camTrans.localPosition = { 0.0f, 0.0f, -100.0f };
    camTrans.localScale    = { 1.0f, 1.0f, 1.0f };
    camTrans.isDirty       = true;
    registry.AddComponent(camera2DEntity, camTrans);

    registry.AddComponent(camera2DEntity, HierarchyComponent{});

    Camera2DComponent camera2D{};
    camera2D.referenceResolution = { 1920, 1080 };
    camera2D.aspectPolicy        = Camera2DComponent::AspectPolicy::Fit;
    camera2D.letterboxColor      = { 0.0f, 0.0f, 0.0f, 1.0f };
    camera2D.pixelSnap           = true;
    camera2D.clearMode           = Camera2DComponent::ClearMode::SolidColor;
    camera2D.priority            = 0;
    registry.AddComponent(camera2DEntity, camera2D);

    registry.AddComponent(camera2DEntity, Camera2DMainTagComponent{});
    registry.AddComponent(camera2DEntity, AudioListenerComponent{});

    EntityID audioSettingsEntity = registry.CreateEntity();
    registry.AddComponent(audioSettingsEntity, NameComponent{ "Audio Settings" });
    registry.AddComponent(audioSettingsEntity, AudioSettingsComponent{});
}
```

`AudioListenerComponent` は 2D Camera にも付ける（3D Main Camera と挙動を揃える）。3D 用の `CameraFreeControlComponent` / `CameraLensComponent` / `CameraMatricesComponent` / `CameraMainTagComponent` は付けない。

`Directional Light` / `Reflection Probe` / `Environment` は 2D シーンには **生成しない**。Sprite / UI 描画は lighting に依存しないため。

なお、Hierarchy 経由の `Create 2D Camera` (`BuildCamera2DSnapshot()` [Source/Hierarchy/HierarchyECSUI.cpp:489](Source/Hierarchy/HierarchyECSUI.cpp:489)) は Phase 1 で同様に更新し、新フィールドを反映した既定値で生成する（ただし `Camera2DMainTagComponent` は付けない。Hierarchy で追加されるカメラは sub-camera 想定）。

### 6.3 NewScene 本体の差分

```cpp
void EditorLayer::NewScene(SceneViewMode mode)
{
    if (!m_gameLayer) {
        return;
    }

    EngineKernel::Instance().ResetRenderStateForSceneChange();

    Registry& registry = m_gameLayer->GetRegistry();
    EditorSelection::Instance().Clear();
    UndoSystem::Instance().ClearECSHistory();
    m_scenePickPending = false;
    // ... 中略（既存のフラグクリア群はそのまま） ...

    ClearRegistryEntities(registry);
    CreateDefaultSceneEntities(registry, mode);

    m_sceneViewMode  = mode;
    m_sceneSavePath  = kDefaultSceneSavePath;
    MarkSceneSaved();
    LOG_INFO("[Editor] New scene created. mode=%s",
        SceneViewModeToString(mode));
}
```

ポイント:

- `m_sceneViewMode = SceneViewMode::Mode3D;` のハードコードを `m_sceneViewMode = mode;` に変える。
- ログに mode を出す。
- それ以外のリセット処理は **一切変更しない**（autosave、recovery、selection、undo、gizmo の各フラグ）。

### 6.4 保存・読込の整合

既存の `BuildSceneMetadata(m_sceneViewMode)` ([Source/Layer/EditorLayerInternal.h:473](Source/Layer/EditorLayerInternal.h:473))、`SceneViewModeFromString()` ([Source/Layer/EditorLayerInternal.h:466](Source/Layer/EditorLayerInternal.h:466))、`LoadSceneFromPath()` ([Source/Layer/EditorLayerSceneIO.cpp:254](Source/Layer/EditorLayerSceneIO.cpp:254)) はそのまま使える。`SceneFileMetadata` への mode 追加は不要。Camera2DComponent への新規フィールド追加は §6.5 に記す（PrefabSystem の serialize / deserialize は更新する）。

### 6.5 Camera2D 機能拡張

現状の `Camera2DComponent` ([Source/Component/Camera2DComponent.h:5-12](Source/Component/Camera2DComponent.h:5)) は最小構成（`orthographicSize` / `zoom` / `nearZ` / `farZ` / `backgroundColor`）しかなく、タイトル / UI 制作時に以下の不足が出る:

- **設計解像度がない**: title screen を「1920x1080 で作る」と固定できない。Game View のアスペクトが変わるとレイアウトが崩れる。
- **Pixel snap がない**: Text や 1px ライン UI が滲む。
- **クリアモードが固定**: 常に `backgroundColor` で塗る。3D の上に 2D UI overlay を重ねるユースケースで使えない。
- **複数 Camera2D の優先度がない**: `TryBuildGameView2DViewProjection()` ([Source/Layer/EditorLayerSceneView.cpp:535](Source/Layer/EditorLayerSceneView.cpp:535)) は最初に見つかった有効 Camera2D を採用する。Gameplay 用 2D Camera と UI overlay 用 2D Camera が共存できない。

この §6.5 で Camera2D を拡張する。実装規模を抑えるため Phase 1 / Phase 2 に分ける。

#### 6.5.1 Phase 1（本 spec の必須範囲）

`Camera2DComponent` に以下を追加する:

```cpp
// Source/Component/Camera2DComponent.h（差分イメージ）
struct Camera2DComponent
{
    float orthographicSize = 10.0f;
    float zoom             = 1.0f;
    float nearZ            = 0.1f;
    float farZ             = 1000.0f;
    DirectX::XMFLOAT4 backgroundColor = { 0.15f, 0.15f, 0.15f, 1.0f };

    // 追加: 設計解像度。0 のときは Game View 解像度をそのまま使う（後方互換）。
    DirectX::XMUINT2 referenceResolution = { 1920, 1080 };

    // 追加: アスペクト不一致時の挙動。
    // Disabled: 何もしない（既存挙動。Game View アスペクトに従う）。
    // Fit:      reference 比を維持して内側にフィット。余白は letterboxColor で埋める。
    // Fill:     reference 比を維持して外側に拡大。はみ出し部分はクロップされる。
    enum class AspectPolicy : uint8_t { Disabled, Fit, Fill };
    AspectPolicy aspectPolicy = AspectPolicy::Disabled;

    DirectX::XMFLOAT4 letterboxColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    // 追加: ピクセルスナップ。true のとき world->screen 変換後に floor して描画する。
    bool pixelSnap = false;

    // 追加: 描画時の clear 挙動。
    // SolidColor: backgroundColor で塗る（既存）。
    // DepthOnly:  color buffer は clear しない。3D シーンの上に重ねる UI overlay 用。
    // DontClear:  どちらも clear しない。
    enum class ClearMode : uint8_t { SolidColor, DepthOnly, DontClear };
    ClearMode clearMode = ClearMode::SolidColor;

    // 追加: 複数 Camera2D 共存時の優先度。大きい方が active。同値なら priority 安定→
    // entity 作成順で安定的にひとつを選ぶ。
    int priority = 0;
};
```

既定値の互換性:

- `referenceResolution = { 1920, 1080 }` だが `aspectPolicy = Disabled` なので、既存 scene は挙動を維持する。古いシーンを読み込むと `aspectPolicy = Disabled`、`pixelSnap = false`、`clearMode = SolidColor`、`priority = 0` になり、表示は変わらない。
- 新規 2D シーンの `Camera 2D` は `referenceResolution = { 1920, 1080 }` `aspectPolicy = Fit` `pixelSnap = true` `clearMode = SolidColor` `priority = 0` で生成する（タイトル制作向きの既定）。

##### 投影行列の更新

`EngineKernel.cpp` ([Source/Engine/EngineKernel.cpp:138-148](Source/Engine/EngineKernel.cpp:138)) と `EditorLayerSceneView.cpp` ([Source/Layer/EditorLayerSceneView.cpp:589-599](Source/Layer/EditorLayerSceneView.cpp:589)) の 2 箇所にある orthographic 投影計算は、ほぼ同一コードなので **`Camera2DUtils::BuildViewProjection(camera2D, transform, viewport, outView, outProj)`** に共通化する（新規ヘッダ `Source/Camera/Camera2DUtils.h`）。AspectPolicy をこの関数で適用する:

```cpp
// 擬似コード
const float viewportAspect = viewport.w / viewport.h;
const float refAspect = referenceResolution.w / referenceResolution.h;

float halfWidth, halfHeight;
switch (aspectPolicy) {
case Disabled:
    halfHeight = orthographicSize / zoom;
    halfWidth  = halfHeight * viewportAspect;
    break;
case Fit:
    if (viewportAspect >= refAspect) {
        halfHeight = orthographicSize / zoom;
        halfWidth  = halfHeight * viewportAspect; // 横余白で letterbox
    } else {
        halfWidth  = (orthographicSize * refAspect) / zoom;
        halfHeight = halfWidth / viewportAspect;  // 縦余白で letterbox
    }
    break;
case Fill:
    if (viewportAspect >= refAspect) {
        halfWidth  = (orthographicSize * refAspect) / zoom;
        halfHeight = halfWidth / viewportAspect;  // 縦をクロップ
    } else {
        halfHeight = orthographicSize / zoom;
        halfWidth  = halfHeight * viewportAspect; // 横をクロップ
    }
    break;
}
```

`pixelSnap` は描画側（Sprite / Text の screen-space 計算）で `floor(screenPos)` を適用する。Phase 1 の最小実装として、`Camera2DComponent::pixelSnap` の値を読み出す getter を提供するに留め、実際の floor 適用は既存 2D 描画パスの一箇所に挿入する（PrimitiveRenderer / Sprite renderer）。

`clearMode` は Game View / 2D pass の clear 呼び出しで参照する。実装箇所は別途 RHI 側の 2D pass コード（`Source/RenderPass/` 系）で、本 spec では「`Camera2DComponent::clearMode` を読んで分岐する」と明記するに留める。詳細実装は Phase 1 のチケット内で確定させる。

`letterboxColor` は AspectPolicy::Fit のときの余白塗り潰し色。Game View の 2D pass で letterbox 領域を別 viewport で塗る。

##### Camera2D 優先度

`TryBuildGameView2DViewProjection()` の選択ループ（[Source/Layer/EditorLayerSceneView.cpp:548-579](Source/Layer/EditorLayerSceneView.cpp:548)）と `EngineKernel.cpp:96-128` の同様ループを更新する:

1. すべての有効 Camera2D を集める（`HierarchyComponent::isActive` を考慮）。
2. `priority` 降順、同値なら entity ID 昇順で sort。
3. 先頭を選ぶ。

#### 6.5.2 Phase 2（任意拡張）

Phase 1 完了後、以下を別 spec で追加する:

- カメラ追従（`followTarget` entity ref + smoothing）
- カメラシェイク（震動量 / 持続時間）
- レイヤカリングマスク（Sprite / Text 側に `renderLayer` を追加）
- ビューポート矩形（split-screen / picture-in-picture）
- スクリーン座標モード（左上原点・ピクセル単位での Transform 表現）

#### 6.5.3 Inspector への影響

`Camera2DComponent` は `ComponentMeta.generated.h` 経由で auto-reflect される ([Source/Generated/ComponentMeta.generated.h:194-201](Source/Generated/ComponentMeta.generated.h:194))。フィールド追加後、`GEHeaderTool` で再生成すると Inspector に新フィールドが自動的に出る。

ただし enum (`AspectPolicy` / `ClearMode`) は generic UI でラベルが出ないため、`InspectorECSUI.cpp:1803` の `DrawComponentIfPresent<Camera2DComponent>` を **Camera2D 専用カスタム描画関数** に差し替える。レイアウト:

- `Background`
  - Clear Mode (combo: SolidColor / DepthOnly / DontClear)
  - Background Color (color picker、`clearMode == SolidColor` のときのみ enabled)
- `Projection`
  - Orthographic Size (drag float)
  - Zoom (drag float)
  - Near Z / Far Z (drag float)
- `Resolution`
  - Reference Resolution (uint2 input)
  - Aspect Policy (combo: Disabled / Fit / Fill)
  - Letterbox Color (color picker、`aspectPolicy == Fit` のときのみ enabled)
- `Rendering`
  - Pixel Snap (checkbox)
  - Priority (int input)

ヘッダ折りたたみは ImGui の collapsing header を使う。Phase 1 完了時点で全フィールドを編集可能にする。

#### 6.5.4 シリアライズ

`PrefabSystem.cpp` ([Source/Asset/PrefabSystem.cpp:236-245](Source/Asset/PrefabSystem.cpp:236) と `:771-773`) を更新:

```jsonc
"Camera2DComponent": {
    "orthographicSize": 10.0,
    "zoom": 1.0,
    "nearZ": 0.1,
    "farZ": 1000.0,
    "backgroundColor": [0.15, 0.15, 0.15, 1.0],
    "referenceResolution": [1920, 1080],
    "aspectPolicy": "Disabled",
    "letterboxColor": [0.0, 0.0, 0.0, 1.0],
    "pixelSnap": false,
    "clearMode": "SolidColor",
    "priority": 0
}
```

deserialize 時、新フィールドが欠けている場合は Phase 1 の既定値（後方互換: `aspectPolicy = Disabled`, `pixelSnap = false`, `clearMode = SolidColor`, `priority = 0`, `referenceResolution = {1920, 1080}`, `letterboxColor = black`）を使う。これにより既存 scene は無改変で読める。

### 6.6 Main 2D Camera タグ

3D 側に `CameraMainTagComponent` ([Source/Layer/EditorLayerInternal.h:1185](Source/Layer/EditorLayerInternal.h:1185)) があるのと対称に、2D 側に `Camera2DMainTagComponent` を新規追加する。

```cpp
// Source/Component/Camera2DMainTagComponent.h（新規）
struct Camera2DMainTagComponent {};
```

役割:

- §6.5.1 で導入した `priority` ソートよりさらに上位の選択ヒント。`Camera2DMainTagComponent` を持つ Camera2D が有効ならそれを最優先で active にする。
- 複数の Main タグが存在する場合は警告ログを出し、先に見つかったものを使う（3D の Main Camera と同じポリシー）。
- 2D 既定 entity 生成時、`Camera 2D` に自動付与する。

`ComponentMeta.generated.h` に登録するため、`GEHeaderTool` で再生成する。`InspectorECSUI` の `Add Component` メニューにも `Camera2DMainTag` を出す。

---

## 7. Scene View / Game View 初期挙動

### 7.1 Grid 既定挙動とモード別永続化

`m_showSceneGrid` は現状単一フラグ ([Source/Layer/EditorLayer.h:254](Source/Layer/EditorLayer.h:254)) で、ユーザーが `View > Overlays > Show Grid` で ON/OFF する。これを **モード別** にする:

```cpp
// EditorLayer.h（差分イメージ）
- bool m_showSceneGrid = true;
+ bool m_showSceneGrid3D = true;   // 3D シーンでの grid 表示
+ bool m_showSceneGrid2D = false;  // 2D シーンでの grid 表示（既定 OFF）
```

`m_sceneViewMode` を見て読み出す inline ヘルパを追加:

```cpp
bool IsSceneGridVisible() const {
    return (m_sceneViewMode == SceneViewMode::Mode2D) ? m_showSceneGrid2D : m_showSceneGrid3D;
}
void SetSceneGridVisible(bool value) {
    if (m_sceneViewMode == SceneViewMode::Mode2D) m_showSceneGrid2D = value;
    else m_showSceneGrid3D = value;
}
```

更新箇所:

- `EditorLayerSceneView.cpp:71` の `m_showSceneGrid && m_sceneViewMode == Mode2D` を `IsSceneGridVisible() && m_sceneViewMode == Mode2D` に変更。
- `EditorLayer.h:149` 周辺の同様参照も `IsSceneGridVisible()` に揃える。
- `EditorLayerMenu.cpp:632` の `ImGui::MenuItem("Show Grid", nullptr, &m_showSceneGrid)` を、現在 mode のフラグを bool& で渡すように差し替える（簡単のため一時 bool に値を取って書き戻す方式でも可）。

これにより `View > 2D Mode` 切替時に grid 表示が「2D 用に最後に設定された状態」へ追従し、`View > 3D Mode` でも同様に「3D 用に最後に設定された状態」へ戻る。新規 2D シーンを作ると `m_showSceneGrid2D = false`（既定値）が効くので grid OFF で開始する。

`SceneFileMetadata` への grid 状態保存は **しない**（grid 表示は editor preference であり scene 属性ではない）。

### 7.2 2D シーン生成直後

- `m_sceneViewMode == Mode2D`、`IsSceneGridVisible() == false`（既定値）。Scene View に grid は出ない。
- `Camera 2D` が 1 つ存在し `Camera2DMainTagComponent` が付いているため、`TryBuildGameView2DViewProjection()` ([Source/Layer/EditorLayerSceneView.cpp:535](Source/Layer/EditorLayerSceneView.cpp:535)) が成功し、Game View は `referenceResolution = 1920x1080` の `aspectPolicy = Fit` で表示される。`No active 2D camera` のエラーは出ない。
- Hierarchy 右クリックの `Create Sprite` / `Create Text` / `Create UI Button` ([Source/Hierarchy/HierarchyECSUI.cpp:738-744](Source/Hierarchy/HierarchyECSUI.cpp:738)) を実行すると、`Editor2D::FinalizeCreatedEntity()` で `RectTransformComponent` `sizeDelta { 128, 128 }` が付き、Camera2D 経由で即座に Game View に出る。

### 7.3 3D シーン生成直後

- 現状と同じ。`m_sceneViewMode == Mode3D`、`IsSceneGridVisible() == true`（既定値）、Main Camera (`{0, 2, -10}`) が選ばれ、Game View は 3D 投影。
- `Create Sprite` / `Create Text` / `Create UI Button` は引き続き使えるが、Camera2D が無いので Game View 上では見えない（既存挙動のまま）。

### 7.4 複数 Camera2D の選択

§6.5.1 / §6.6 で導入する選択ロジックに従う:

1. `Camera2DMainTagComponent` を持ち、`HierarchyComponent::isActive == true` な Camera2D があればそれを採用（複数あれば警告ログ + 先勝ち）。
2. 1 が無ければ、有効な全 Camera2D を `priority` 降順 → entity ID 昇順で sort して先頭。
3. 0 個なら `TryBuildGameView2DViewProjection()` は false を返す（既存通り）。

これにより「Gameplay 用 Camera2D（priority 0）+ UI overlay 用 Camera2D（priority 100, ClearMode = DepthOnly）」のような構成が可能になる（Phase 1 で骨格を入れ、UI overlay の実描画 pass は Phase 2 で詰める）。

---

## 8. UI 文言

### 8.1 ダイアログ本体

| ID | 表示文字列 |
| --- | --- |
| Title | `New Scene` |
| Subtitle | `Choose a scene mode.` |
| Radio 3D | `3D Scene` |
| Radio 3D desc | `Main camera, directional light, reflection probe, environment.` |
| Radio 2D | `2D Scene` |
| Radio 2D desc | `Camera 2D, orthographic view. For titles and UI screens.` |
| Button create | `Create` |
| Button cancel | `Cancel` |

### 8.2 ログ

- 2D 生成: `[Editor] New scene created. mode=2D`
- 3D 生成: `[Editor] New scene created. mode=3D`
- ダイアログキャンセル: `[Editor] New scene canceled.`

---

## 9. EditorLayer に追加する状態

```cpp
// EditorLayer.h（追加分）
bool m_openNewSceneModePopup    = false;        // ダイアログを開く要求
SceneViewMode m_newSceneSelectedMode = SceneViewMode::Mode3D; // ダイアログ内のラジオ選択
```

`PendingSceneAction::NewScene` を実行する代わりに `m_openNewSceneModePopup = true` を立てる。ダイアログ描画関数 `DrawNewSceneModePopup()` を `EditorLayerSceneIO.cpp` に追加する（既存の `Unsaved changes` ポップアップ描画関数の近くに置く）。

---

## 10. 既存挙動の保持

以下は **変更しない**。

- `OpenScene` / `LoadSceneFromPath` / `SaveCurrentScene` / `SaveCurrentSceneAs` / autosave / recovery の経路。
- `View > 3D Mode / 2D Mode` トグルの挙動。`m_sceneViewMode` の上書きは引き続き scene 内 entity を変えない。
- 既存 scene ファイル（`sceneViewMode = "3D"`）の読込結果。
- Hierarchy 右クリックの `Create 2D Camera` / `Create Sprite` / `Create Text` / `Create UI Button` の挙動。
- 既存 spec `Sprite_2DUI_Improvement_Spec_2026-05-02.md`、`CreateText_2DUI_Improvement_Spec_2026-05-03.md`、`UIButton_2DUI_Improvement_Spec_2026-05-04.md`、`Sprite_2DUI_Editor_Blocker_Fix_Spec_2026-05-02.md` の対象範囲。

---

## 11. 実装方針

### Phase 1: ダイアログ + 既定 entity 分岐 + Camera2D 機能拡張（本 spec の必須範囲）

#### 11.1 New Scene ダイアログ

1. `EditorLayer.h` に `m_openNewSceneModePopup` と `m_newSceneSelectedMode` を追加。
2. `EditorLayer::NewScene()` を `EditorLayer::NewScene(SceneViewMode mode)` に変更。引数無し版は削除。
3. `ExecutePendingSceneAction()` の `case PendingSceneAction::NewScene` を「`m_openNewSceneModePopup = true` を立てるだけ」に変更。直接 `NewScene()` を呼ばない。
4. `EditorLayerSceneIO.cpp` に `DrawNewSceneModePopup()` を追加。Modal で 2D/3D ラジオ・Create/Cancel ボタンを描画。Create 押下時に `NewScene(m_newSceneSelectedMode)` を呼ぶ。
5. `DrawNewSceneModePopup()` を Update から呼ぶ（既存の `Unsaved changes` ポップアップ描画と同じ場所）。

#### 11.2 既定 entity 分岐

6. `EditorLayerInternal.h` に `CreateDefaultSceneEntities2D` / `CreateDefaultSceneEntities3D` を追加。`CreateDefaultSceneEntities(Registry&, SceneViewMode)` を入口にする。
7. `Source/Hierarchy/HierarchyECSUI.cpp` の `BuildCamera2DSnapshot()` を、新フィールドを反映した既定値で更新する（`Camera2DMainTagComponent` は付けない）。

#### 11.3 Camera2D 機能拡張

8. `Source/Component/Camera2DComponent.h` に `referenceResolution` / `AspectPolicy` enum + `aspectPolicy` / `letterboxColor` / `pixelSnap` / `ClearMode` enum + `clearMode` / `priority` を追加。
9. `Source/Component/Camera2DMainTagComponent.h` を新規作成（空の tag struct）。
10. `GEHeaderTool` で `ComponentMeta.generated.h` を再生成。
11. `Source/Asset/PrefabSystem.cpp` の Camera2D の serialize / deserialize を新フィールドに対応させる。`AspectPolicy` / `ClearMode` は文字列で保存（"Disabled" / "Fit" / "Fill"、"SolidColor" / "DepthOnly" / "DontClear"）。`Camera2DMainTagComponent` の serialize / deserialize も追加。欠落フィールドは Phase 1 既定値で埋める。
12. `Source/Camera/Camera2DUtils.h` を新規作成。`BuildViewProjection(camera2D, transform, viewport, outView, outProj)` を実装し、`AspectPolicy` の Disabled / Fit / Fill を実装する。
13. `Source/Engine/EngineKernel.cpp:84-149` と `Source/Layer/EditorLayerSceneView.cpp:535-600` を `Camera2DUtils::BuildViewProjection()` に差し替える。
14. 両ファイルの Camera2D 選択ループを「Main タグ優先 → priority 降順 → entity ID 昇順」に更新する。
15. 2D pass の clear 呼び出し箇所で `Camera2DComponent::clearMode` を参照して分岐させる。`AspectPolicy::Fit` 時の letterbox 余白は `letterboxColor` で塗る。具体的な実装ファイルは `Source/RenderPass/` 系を確認しながら決める（Phase 1 内チケットで詰める）。
16. `pixelSnap` を Sprite / Text の screen-space 計算箇所で参照し、有効時は `floor()` を適用する。
17. `Source/Inspector/InspectorECSUI.cpp:1803` の `DrawComponentIfPresent<Camera2DComponent>` を Camera2D 専用カスタム描画に差し替え（§6.5.3 のレイアウト）。`Add Component` メニュー（[Source/Inspector/InspectorECSUI.cpp:1902](Source/Inspector/InspectorECSUI.cpp:1902)）に `Camera2DMainTag` を追加。

#### 11.4 Grid のモード別永続化

18. `EditorLayer.h:254` の `m_showSceneGrid` を `m_showSceneGrid2D = false` / `m_showSceneGrid3D = true` に分割。
19. `IsSceneGridVisible()` / `SetSceneGridVisible(bool)` ヘルパを追加。
20. `EditorLayer.h:149` 周辺と `EditorLayerSceneView.cpp:71` の参照を `IsSceneGridVisible()` に置換。
21. `EditorLayerMenu.cpp:632` の `Show Grid` メニュー項目を、現在モードのフラグを読み書きする形に変更。

#### 11.5 動作確認

22. 既存 3D シーンの load / save / autosave / recovery が壊れていないことを確認。
23. 既存 scene（旧 schema の Camera2D を持つもの）が無改変で読めることを確認。
24. 2D 新規シーンで `Create Sprite` / `Create Text` / `Create UI Button` がそのまま見えることを確認。
25. View > 2D Mode / 3D Mode 切替で grid 表示状態がモード別に追従することを確認。

### Phase 2: 任意拡張（本 spec の必須範囲外）

- ダイアログに `Description` プレビュー（生成される entity 一覧の表示）。
- `Last selected mode` を `imgui.ini` または editor settings に永続化。
- 2D / 3D テンプレートをユーザーが拡張できる仕組み（`Create Custom Default` のような）。
- 既存 scene の Mode を変換する `Convert to 2D / Convert to 3D` メニュー。
- Camera2D の追従 / シェイク / レイヤカリングマスク / split-screen viewport / スクリーン座標モード（§6.5.2）。

---

## 12. 受け入れ条件

すべて満たすこと。

### 12.1 New Scene ダイアログ

- [ ] `File > New Scene` または `Ctrl + N` で **必ず Mode 選択ダイアログ** が開く。Cancel 押下で現在のシーンが維持される。
- [ ] Unsaved changes 状態で `New Scene` を実行すると、保存破棄確認 → Mode 選択ダイアログ → 生成、の順に進む。途中の Cancel で現在のシーンが残る。
- [ ] Mode 選択ダイアログがキー操作 (`←` `→` `Enter` `Esc`) で完結する。
- [ ] ログに `[Editor] New scene created. mode=2D` または `mode=3D` が出力される。

### 12.2 2D シーン生成

- [ ] ダイアログで `2D Scene` を選んで `Create` を押すと:
  - registry に `Camera 2D` (`Camera2DComponent` + `Camera2DMainTagComponent` + `AudioListenerComponent`) と `Audio Settings` のみが存在する。
  - `Directional Light` / `Reflection Probe` / `Environment` / `Main Camera`（3D）は存在しない。
  - `Camera 2D` の `Camera2DComponent` が `referenceResolution = {1920, 1080}` / `aspectPolicy = Fit` / `pixelSnap = true` / `clearMode = SolidColor` / `priority = 0` で生成されている。
  - `m_sceneViewMode == Mode2D`、`IsSceneGridVisible() == false`（Scene View に grid が出ない）。
  - Game View が `Camera 2D` の orthographic 表示で `No active 2D camera` エラーが出ない。
  - Game View のアスペクトを reference (16:9) と異なる比に変えても、コンテンツ比が保たれ余白が `letterboxColor` で塗られる。
  - `Create Sprite` / `Create Text` / `Create UI Button` を実行すると Game View に表示される。

### 12.3 3D シーン生成

- [ ] ダイアログで `3D Scene` を選んで `Create` を押すと:
  - registry に `Main Camera` / `Directional Light` / `Reflection Probe` / `Environment` / `Audio Settings` が存在する（現状と一致）。
  - `m_sceneViewMode == Mode3D`、`IsSceneGridVisible() == true`、Scene View / Game View ともに 3D 表示。

### 12.4 Camera2D 機能

- [ ] Inspector に Camera2D 専用 UI が出て、Background / Projection / Resolution / Rendering の各セクションを編集できる。`AspectPolicy` と `ClearMode` は combo で選べる。
- [ ] `aspectPolicy = Disabled` のとき、投影は旧来の挙動（Game View アスペクトに従う）と一致する。
- [ ] `aspectPolicy = Fit` のとき、Game View のアスペクトに依らず reference 比のコンテンツが収まり、余白が `letterboxColor` で塗られる。
- [ ] `aspectPolicy = Fill` のとき、reference 比が外接するように拡大され、余白は出ない（はみ出しはクロップ）。
- [ ] `pixelSnap = true` のとき、整数ピクセル位置を狙った Sprite / Text が滲まない。
- [ ] `clearMode = DepthOnly` の Camera2D を `clearMode = SolidColor` の Camera2D より高 priority に設定すると、後者の上に前者の描画が重なる（複数 Camera2D 共存）。
- [ ] `Camera2DMainTagComponent` を持つカメラがあれば優先採用される。複数 Main があれば警告ログが出て先勝ちする。

### 12.5 Grid

- [ ] 2D シーン生成直後の Scene View は grid OFF。
- [ ] 3D シーン生成直後の Scene View は grid ON（既存挙動）。
- [ ] `View > Overlays > Show Grid` トグルでの ON/OFF が、現在の `m_sceneViewMode` 別に保持される。`View > 2D Mode` ⇄ `View > 3D Mode` 切替で grid 表示が各モードの最後の状態に追従する。
- [ ] grid 状態は scene ファイルに保存されない（editor preference として残す）。

### 12.6 既存挙動 / 後方互換

- [ ] 既存の 3D scene ファイルの読込結果に変化がない。
- [ ] 旧 schema の Camera2DComponent（拡張前のフィールド）を含む scene ファイルを読み込むと、新フィールドが Phase 1 既定値で埋められ、`aspectPolicy = Disabled` のため見た目が変わらない。
- [ ] 生成後にそのまま `Save Scene As...` で保存し再読込すると、選択した Mode で Scene View が復元される（`sceneViewMode` メタデータが正しく保存・復元される）。
- [ ] `View > 2D Mode / 3D Mode` トグルそれ自体の挙動（registry に触らず `m_sceneViewMode` だけ更新）が変わらない。
- [ ] autosave / recovery / Open Scene / Save / Save As の経路が壊れていない。
