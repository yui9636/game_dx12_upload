# Sprite / 2D UI Improvement Spec

作成日: 2026-05-02

対象:

- `Source/Component/SpriteComponent.h`
- `Source/Component/RectTransformComponent.h`
- `Source/Component/CanvasItemComponent.h`
- `Source/Component/UIButtonComponent.h`
- `Source/Component/Camera2DComponent.h`
- `Source/UI/UI2DDrawSystem.h`
- `Source/UI/UIHitTestSystem.h`
- `Source/Inspector/InspectorECSUI.cpp`
- `Source/Hierarchy/HierarchyECSUI.cpp`
- `Source/Layer/EditorLayerSceneView.cpp`
- `Source/Layer/EditorLayerPanels.cpp`
- `Source/Layer/EditorLayerInternal.h`
- `Source/Asset/PrefabSystem.cpp`
- `Source/Engine/EngineKernel.cpp`

関連:

- `Data/Scene/*.scene`
- `Data/**/*.png`
- `Data/**/*.jpg`
- `Data/**/*.dds`
- `Data/**/*.tga`

## 1. 目的

現在の 2D UI / Sprite 機能は、component と editor overlay の土台はあるが、実制作で使うには導線が弱い。特に次の問題がある。

- Inspector 上で Sprite の texture を選びづらい。
- Sprite の preview / texture size 反映 / tint 操作が専用 UI になっていない。
- Game View が 2D 表示時に `No active 2D camera` を出し、Sprite / UI が表示されない場面がある。
- Sprite / UI 作成時に `Camera2DComponent` との関係が分かりづらい。
- 2D UI が editor overlay としては描けるが、runtime render pipeline との責務が曖昧。
- `RectTransformComponent`、`CanvasItemComponent`、`SpriteComponent` の必要組み合わせが user に見えない。

この仕様では、Sprite と 2D UI を「作成、編集、保存、Game View 表示、Play 中の button click」まで一貫して扱えるように改善する。

## 2. 現状

### 2.1 Component

`SpriteComponent`:

```cpp
struct SpriteComponent
{
    std::string textureAssetPath;
    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
};
```

`RectTransformComponent`:

- `anchoredPosition`
- `sizeDelta`
- `anchorMin`
- `anchorMax`
- `pivot`
- `rotationZ`
- `scale2D`

`CanvasItemComponent`:

- `sortingLayer`
- `orderInLayer`
- `visible`
- `interactable`
- `pixelSnap`
- `lockAspect`

`UIButtonComponent`:

- `buttonId`
- `enabled`

`Camera2DComponent`:

- `orthographicSize`
- `zoom`
- `nearZ`
- `farZ`
- `backgroundColor`

### 2.2 Editor 表示

- `UI2DDrawSystem::CollectDrawEntries` は `RectTransformComponent + CanvasItemComponent + TransformComponent` を持つ entity を集める。
- `EditorLayer::Draw2DOverlayForRect` は `SpriteComponent` があれば ImGui の `AddImageQuad` で表示する。
- Sprite texture が空の場合は rect の quad だけ表示される。
- `HierarchyECSUI` は `Create Sprite`、`Create Text`、`Create 2D Camera` を持つ。
- Scene View への texture D&D は 2D mode のとき Sprite entity を作成できる。
- `PrefabSystem` は Sprite / RectTransform / CanvasItem / UIButton の serialize / deserialize に対応している。

### 2.3 Game View の問題

Game View の 2D overlay は `TryBuildGameView2DViewProjection` に依存している。この関数は active な `Camera2DComponent + TransformComponent` を見つけられない場合 false を返す。

false の場合、Game View は次を表示する。

```text
No active 2D camera
```

つまり Sprite / UI が scene に存在していても、2D camera が無い、または inactive hierarchy 配下にあると Game View には出ない。現状の表示文は原因を十分に説明していないため、user は Sprite が壊れているのか、camera が無いのか判断しづらい。

### 2.4 Inspector の問題

`SpriteComponent` は generic `ComponentMeta` 経由で表示されるため、`textureAssetPath` は単なる text input、`tint` は `DragFloat4` になる。

不足:

- texture thumbnail がない。
- Asset Browser から texture を drag/drop できる専用 slot がない。
- texture picker がない。
- texture path の存在確認がない。
- texture の width / height を `RectTransformComponent::sizeDelta` に反映する button がない。
- tint は color editor ではなく float drag なので色として扱いづらい。
- Sprite に必要な `RectTransformComponent` / `CanvasItemComponent` / `TransformComponent` の不足を警告しない。

## 3. 改善目標

Sprite / 2D UI 改善の完了状態は次の通り。

- Inspector で Sprite texture を thumbnail 付き slot として編集できる。
- Texture slot は Asset Browser D&D、picker、clear、open asset に対応する。
- Sprite tint は `ColorEdit4` で編集できる。
- Sprite の texture size を `RectTransformComponent::sizeDelta` に反映できる。
- Sprite entity に必要 component が不足している場合、Inspector に修復 action を出す。
- Game View の `No active 2D camera` が具体的な原因と作成 action を示す。
- 2D scene で Sprite を作成したとき、必要に応じて 2D camera を自動作成または prompt できる。
- Scene View と Game View で同じ Sprite / UI が見える。
- Play 中に `UIButtonComponent` が Sprite / RectTransform の hit test と連動して click event を出せる。
- 保存 / load 後も Sprite / UI の見た目、並び順、buttonId が維持される。

## 4. 非目標

- 完全な Unity Canvas 互換は目標にしない。
- 複雑な layout group / auto layout / text wrapping engine の刷新は含めない。
- 9-slice / atlas / sprite animation はこの仕様の必須範囲に含めない。
- DX12 専用の新規 2D renderer を Phase 1 の必須にしない。
- 既存 `Sprite` / `Sprite3D` class の大規模置き換えはしない。

## 5. 仕様の優先順位

この仕様は最終完成形までを含むが、実装完了判定は Phase ごとに分ける。`## 14. 受け入れ条件` は最終完成条件ではなく、Phase ごとの受け入れ条件を正とする。

優先順位:

1. Phase 0 / Phase 1 の完了条件を今回の最小実装ゴールとする。
2. Phase 2 / Phase 3 は editor 作業性と validation の拡張ゴールとする。
3. Phase 4 は runtime render pass 化の将来ゴールとする。
4. Phase 間で衝突する記述がある場合、対象 Phase の完了条件を優先する。
5. `DX12 専用の新規 2D renderer` は Phase 4 まで必須にしない。

## 6. Component 責務

### 6.1 SpriteComponent

Sprite の画像と色だけを持つ。

追加候補:

```cpp
enum class SpriteImageType : uint8_t
{
    Simple = 0,
    Filled,
};

struct SpriteComponent
{
    std::string textureAssetPath;
    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
    SpriteImageType imageType = SpriteImageType::Simple;
    float fillAmount = 1.0f;
};
```

Phase 1 では `textureAssetPath` と `tint` の専用 Inspector 化だけを必須にする。`uvRect`、`imageType`、`fillAmount` は Phase 2 以降でよい。

### 6.2 RectTransformComponent

2D UI の layout source of truth とする。

要件:

- editor 2D mode では `RectTransformComponent` が `TransformComponent` へ同期される。
- `anchoredPosition` は local XY position として扱う。
- `rotationZ` は local Z rotation として扱う。
- `scale2D` は local XY scale として扱う。
- `sizeDelta` は Sprite / hit test / outline の rect size として扱う。

### 6.3 CanvasItemComponent

表示と hit test の制御を持つ。

要件:

- `visible == false` の entity は draw しない。
- `interactable == false` の entity は hit test しない。
- `sortingLayer`、`orderInLayer`、`TransformComponent::worldPosition.z` の順に描画する。
- `pixelSnap == true` の場合、screen corner と text position を pixel に丸める。
- `lockAspect == true` の場合、texture size 変更時に aspect ratio を維持する。

### 6.4 UIButtonComponent

click event の識別子だけを持つ。

要件:

- Sprite / Text と同じ entity でもよい。
- `buttonId.empty()` は Inspector warning。
- `enabled == false` の button は click event を出さない。
- hit test は `RectTransformComponent + CanvasItemComponent` を使う。

## 7. Inspector 改善

### 7.1 Sprite 専用 Inspector

`DrawComponentIfPresent<SpriteComponent>` は generic drawer ではなく専用 drawer を使う。

表示:

```text
SpriteComponent
  Texture    [thumbnail 64x64] Data/...
             [Pick] [Clear] [Use Texture Size]
  Tint       ColorEdit4
  Status     Loaded 512 x 256
```

要件:

- `textureAssetPath` が空なら `No Texture` と表示する。
- path が存在しない、または load に失敗したら error color で表示する。
- thumbnail は `ResourceManager::Instance().GetTexture(textureAssetPath)` と `ImGuiRenderer::GetTextureID` を使う。
- Asset Browser から `.png`、`.jpg`、`.jpeg`、`.dds`、`.tga`、`.hdr` を drag/drop できる。
- `Pick` は既存 `DrawTextureSlot` と同じ cache / popup 方針を使う。
- `Clear` は `textureAssetPath.clear()`。
- `Use Texture Size` は `ResourceManager::Instance().GetTexture(textureAssetPath)` で texture を取得し、`ITexture::GetWidth()` / `ITexture::GetHeight()` を `RectTransformComponent::sizeDelta` に入れる。
- `Use Texture Size` 実行時は `lockAspect` が true なら片方変更時も aspect を保つ。
- 変更は Undo / Prefab override に対応する。

### 7.2 Texture Size 取得

texture size の取得方法は次で統一する。

```cpp
auto texture = ResourceManager::Instance().GetTexture(sprite.textureAssetPath);
if (texture) {
    rect.sizeDelta = {
        static_cast<float>(texture->GetWidth()),
        static_cast<float>(texture->GetHeight())
    };
}
```

要件:

- `ITexture::GetWidth()` / `ITexture::GetHeight()` を使用する。
- texture load に失敗した場合、`Use Texture Size` は disabled にする。
- width / height が 0 の場合、`sizeDelta` は変更しない。
- `textureAssetPath` が空の場合、`Use Texture Size` は disabled にする。
- 実行後は `SyncRectTransformToTransform(*rect, *transform)` を呼び、Scene View / Game View の表示を即時更新する。

### 7.3 Tint UI

`tint` は `DragFloat4` ではなく `ImGui::ColorEdit4` で編集する。

要件:

- alpha を編集できる。
- 0.0f - 1.0f の範囲に clamp する。
- 色変更は undoable edit session として扱う。
- `ColorEdit4` の drag 中に毎 frame Undo を積まない。
- edit 開始時に before snapshot を 1 回だけ保持する。
- edit 中は component 値だけを更新し、Undo action は作らない。
- `ImGui::IsItemDeactivatedAfterEdit()` のタイミングで、before と after が異なる場合だけ Undo action を 1 件積む。
- Prefab override の mark も Undo action 確定時に 1 回だけ行う。

### 7.4 Texture Path Undo 粒度

texture path の変更は click / D&D / picker selection / clear の 1 操作につき Undo action 1 件とする。

要件:

- text input で直接 path を編集する場合は、入力終了時に 1 件だけ Undo action を積む。
- D&D で path が変わった場合は payload delivery 時に 1 件だけ Undo action を積む。
- `Pick` popup で選択した場合は選択確定時に 1 件だけ Undo action を積む。
- `Clear` は click 1 回につき 1 件だけ Undo action を積む。
- `Use Texture Size` は `RectTransformComponent` の Undo action として 1 件だけ積む。

### 7.5 Required Component Repair

Sprite entity に必要な component が不足している場合は Inspector に repair panel を出す。

必要 component:

- `TransformComponent`
- `RectTransformComponent`
- `CanvasItemComponent`
- `SpriteComponent`

表示例:

```text
Sprite setup incomplete
[Add RectTransform] [Add CanvasItem] [Add Transform]
```

要件:

- repair action は UndoSystem 経由で記録する。
- component を追加した直後に default 値で描画可能になる。
- `SpriteComponent` 単体追加時にも repair panel が出る。

### 7.6 RectTransform 専用 UI

Phase 1 では generic drawer を継続してよいが、次の改善を仕様として定義する。

- `anchoredPosition` は `Position` と表示。
- `sizeDelta` は `Size` と表示し、最小 1.0 に clamp。
- `anchorMin` / `anchorMax` は 0.0 - 1.0 に clamp。
- `pivot` は 0.0 - 1.0 に clamp。
- `rotationZ` は degrees として表示。
- `scale2D` は最小 0.001 に clamp。
- `Reset Rect` button を持つ。

### 7.7 CanvasItem 専用 UI

Phase 1 では generic drawer を継続してよいが、次の改善を仕様として定義する。

- `visible`、`interactable` は checkbox。
- `sortingLayer`、`orderInLayer` は int input。
- `pixelSnap`、`lockAspect` は checkbox。
- `Bring Forward` / `Send Backward` button で `orderInLayer` を変更できる。

### 7.8 UIButton 専用 UI

表示:

```text
UIButtonComponent
  Button Id  [StartButton]
  Enabled    [x]
  Status     Hit test ready
```

warning:

- `buttonId` が空。
- `RectTransformComponent` がない。
- `CanvasItemComponent` がない。
- `CanvasItemComponent::interactable == false`。
- `CanvasItemComponent::visible == false`。

## 8. Game View 改善

### 8.1 No Active 2D Camera 表示

現在の `No active 2D camera` は、原因と解決手段が不足している。表示を次のようにする。

```text
No active 2D camera
Create a 2D Camera or switch the scene to 3D mode.
[Create 2D Camera] [Use Scene View Camera]
```

要件:

- `Camera2DComponent` が 0 件なら `Create 2D Camera` を出す。
- `Camera2DComponent` があるが inactive hierarchy 配下なら `2D Camera is inactive` と出す。
- `Camera2DComponent` はあるが `TransformComponent` がないなら `2D Camera has no Transform` と出す。
- button は Game View overlay 内に出す。
- `Create 2D Camera` は `HierarchyECSUI::BuildCamera2DSnapshot` と同等の snapshot を使う。

### 8.2 Fallback 2D Preview

Editor の作業性を優先し、Camera2D が無い場合でも Scene View と同じ editor 2D camera を使った fallback preview を許可する。

優先順位:

1. active な `Camera2DComponent + TransformComponent` がある場合は必ずそれを使う。
2. active 2D camera が無く、Editor mode かつ `Use Scene View Camera` が明示的に有効な場合だけ fallback preview を使う。
3. active 2D camera が無く、fallback が無効な場合は `No active 2D camera` と作成 action を表示する。
4. Play mode では fallback preview を使わない。

要件:

- fallback は Editor mode のみ有効。
- fallback は user が `Use Scene View Camera` を選んだときだけ有効にする。自動 fallback はしない。
- Play mode では fallback を使わず、active 2D camera の不足を error として表示する。
- fallback 使用中は `Editor 2D Preview` badge を表示する。
- fallback は Game runtime camera の代替ではない。
- fallback 中でも `Create 2D Camera` action は表示し続ける。

### 8.3 Game View 2D Overlay

現在は base render texture の上に `Draw2DOverlayForRect` で Sprite / Text を描いている。Phase 1 ではこの方式を維持する。

要件:

- `m_gameViewShowUIOverlay == true` のとき Sprite / Text を描く。
- `m_gameViewShow2DOverlay == true` は label / safe badge / debug 表示だけに限定する。
- Sprite が存在するのに UI overlay が off の場合、toolbar に視覚的に off state を出す。
- Game View toolbar の `UI` toggle は Sprite / Text 表示に直結するため、tooltip を `Show Sprite/Text UI overlay` にする。

### 8.4 Runtime Render への接続

Phase 2 以降で、Sprite / 2D UI は ImGui overlay ではなく render pipeline の pass として描けるようにする。

候補:

```text
GameLayer registry
  -> UI2DDrawSystem::CollectDrawEntries
  -> UI2DRenderPass
  -> sceneColor/displayTexture
```

要件:

- DX11 / DX12 のどちらでも同じ draw entry を使う。
- editor overlay と runtime render pass は同じ sorting 規則を使う。
- Play build でも UI が表示される。
- ImGui に依存する描画は editor preview 専用に限定する。

## 9. Scene View 改善

### 9.1 Sprite 作成

Sprite 作成 entry point:

- Hierarchy context menu `Create Sprite`
- Asset Browser texture 右クリック `Create Sprite`
- Scene View 2D mode への texture D&D
- Inspector repair action

要件:

- `Create Sprite` は `TransformComponent + HierarchyComponent + RectTransformComponent + CanvasItemComponent + SpriteComponent` を必ず付与する。
- texture 付き作成の場合、texture width / height を `sizeDelta` に入れる。
- texture 無し作成の場合、`sizeDelta = { 128, 128 }` とする。
- 作成後は entity を選択する。
- 2D scene に camera が無い場合、toast / status bar で `2D Camera is required for Game View` を出す。

### 9.2 2D Camera 作成導線

2D Camera は Sprite / UI scene の基本要素とする。

要件:

- Scene View が 2D mode のとき、Hierarchy context menu 上部に `Create 2D Camera` を出す。
- Sprite 作成時に active 2D camera が無ければ `Create 2D Camera` prompt を出す。
- `BuildCamera2DSnapshot` は default で `Transform.localPosition = { 0, 0, -100 }` とする。
- `Camera2DComponent::orthographicSize` は scene view で sprite が見える値にする。

### 9.3 Selection / Gizmo

要件:

- 2D mode で Sprite を選択すると rect outline が表示される。
- translate gizmo は `RectTransformComponent::anchoredPosition` を更新する。
- rotate gizmo は `rotationZ` を更新する。
- scale gizmo は `scale2D` を更新する。
- `sizeDelta` の resize handle は Phase 2 でよい。

## 10. Hit Test / Button Click

### 10.1 Hit Test

`UIHitTestSystem::PickTopmost` は次を守る。

- `visible == false` は対象外。
- `interactable == false` は対象外。
- inactive hierarchy は対象外。
- `sortingLayer` が大きいものを優先。
- `orderInLayer` が大きいものを優先。
- `worldPosition.z` が大きいものを優先。
- 同値なら entity id が大きいものを優先。

### 10.2 Game View 座標変換

button click は Game View の表示スケール、解像度プリセット、Fit / Fill / Pixel Perfect の影響を受ける。したがって mouse position は window 全体ではなく、最終的に描画された image rect で正規化する。

基準:

- Game View の描画領域は `EditorLayer::m_gameViewRect` を正とする。
- `m_gameViewRect = { imageMin.x, imageMin.y, imageSize.x, imageSize.y }` は `EditorLayer::DrawGameView` で Fit / Fill / Pixel Perfect 適用後の実表示 rect とする。
- `UIButtonClickSystem` に渡す `gameViewRect` は必ずこの `m_gameViewRect` と同じ座標系にする。
- mouse event の `x / y` は screen coordinate として扱う。
- `UIHitTestSystem::ScreenToCanvasPoint(gameViewRect, view, projection, screenPoint, outWorldPoint)` で canvas world point に変換する。

変換式:

```cpp
const float localX = (screenPoint.x - gameViewRect.x) / gameViewRect.z;
const float localY = (screenPoint.y - gameViewRect.y) / gameViewRect.w;
if (localX < 0.0f || localX > 1.0f || localY < 0.0f || localY > 1.0f) {
    return false;
}

const float ndcX = localX * 2.0f - 1.0f;
const float ndcY = 1.0f - localY * 2.0f;
```

要件:

- `Fit` で余白が出た場合、余白クリックは button hit しない。
- `Fill` で image が window からはみ出す場合、実際の image rect 基準で hit test する。
- `Pixel Perfect` / 1x / 2x / 3x scale でも `m_gameViewRect` 基準で hit test する。
- `m_gameViewRect.z <= 1.0f` または `m_gameViewRect.w <= 1.0f` の場合は hit test しない。
- Scene View の hit test は `m_sceneViewRect`、Game View の hit test は `m_gameViewRect` を使い分ける。

### 10.3 UIButtonClickSystem

要件:

- left mouse down で topmost を pick する。
- pick entity に `UIButtonComponent` があり、`enabled == true` で `buttonId` が空でなければ queue に push する。
- `buttonId` 重複は validate warning。
- click queue は frame end で clear する。
- Game View click を扱う場合は `EditorLayer::GetGameViewRect()` を使用する。
- Scene View click を扱う場合は `EditorLayer::GetSceneViewRect()` 相当の rect を使用する。
- Game View と Scene View の両方が hover されていない frame では UI click を処理しない。

### 10.4 GameLoop 連携

`GameLoop` の `UIButtonClicked` condition は `UIButtonClickEventQueue` を読む。Sprite / 2D UI 改善では、button entity を editor で作りやすくする。

テンプレート:

```text
Create UI Button
  - NameComponent
  - TransformComponent
  - HierarchyComponent
  - RectTransformComponent
  - CanvasItemComponent
  - SpriteComponent
  - UIButtonComponent
```

default:

- name: `Button`
- buttonId: `Button`
- size: `{ 240, 64 }`
- tint: `{ 1, 1, 1, 1 }`
- interactable: true

## 11. Validation

### 11.1 Sprite Validation

Warning:

- `SpriteComponent` があるが `RectTransformComponent` がない。
- `SpriteComponent` があるが `CanvasItemComponent` がない。
- `SpriteComponent` があるが `TransformComponent` がない。
- `textureAssetPath` が空。
- `textureAssetPath` の file が存在しない。
- `ResourceManager` で texture load に失敗する。
- `sizeDelta.x <= 0` または `sizeDelta.y <= 0`。

### 11.2 2D Scene Validation

Warning:

- sceneViewMode が 2D だが active `Camera2DComponent` が存在しない。
- `Camera2DComponent` があるが inactive hierarchy 配下。
- Sprite / Text / UIButton が 1 つ以上あるのに active 2D camera がない。

### 11.3 UIButton Validation

Warning:

- `buttonId` が空。
- `buttonId` が重複。
- `UIButtonComponent` があるが `RectTransformComponent` がない。
- `UIButtonComponent` があるが `CanvasItemComponent` がない。
- `CanvasItemComponent::interactable == false`。
- `CanvasItemComponent::visible == false`。

## 12. Serialization

`PrefabSystem` はすでに Sprite / RectTransform / CanvasItem / UIButton / Camera2D の serialize に対応している。改善後も component name は維持する。

追加 field を入れる場合:

- 読み込み時は default 値で backward compatible にする。
- 保存時は新 field を出力する。
- 既存 scene の `SpriteComponent` は壊さない。

例:

```json
"SpriteComponent": {
  "textureAssetPath": "Data/UI/StartButton.png",
  "tint": [1.0, 1.0, 1.0, 1.0],
  "uvRect": [0.0, 0.0, 1.0, 1.0],
  "imageType": 0,
  "fillAmount": 1.0
}
```

Phase 1 では既存 field のままでよい。

## 13. 実装フェーズ

### Phase 0: Game View の原因表示

目的: `No active 2D camera` の原因を分かるようにする。

作業:

- `TryBuildGameView2DViewProjection` を診断付きにする。
- 0 camera / inactive camera / no transform を区別する。
- Game View overlay に原因と action を表示する。
- `Create 2D Camera` action を追加する。

完了条件:

- 2D camera がない scene で `No active 2D camera` の理由と作成 action が表示される。
- Game View から 2D camera を作成できる。
- fallback preview は自動では有効にならない。

### Phase 1: Sprite Inspector 専用化

目的: Inspector で Sprite を直接操作できる。

作業:

- `DrawSpriteComponentInspector` を追加する。
- texture thumbnail slot を追加する。
- texture D&D / picker / clear を追加する。
- `ColorEdit4` tint を追加する。
- `Use Texture Size` を追加する。
- required component repair を追加する。

完了条件:

- texture を Inspector から選べる。
- texture D&D で Sprite に反映できる。
- tint を色として編集できる。
- texture size を rect size に反映できる。
- `ColorEdit4` drag 中に Undo が毎 frame 積まれない。
- `Use Texture Size` は `ITexture::GetWidth()` / `ITexture::GetHeight()` を使う。

### Phase 2: Sprite / UI 作成導線

目的: 2D UI 作成時に必要 component と camera が揃う。

作業:

- `Create UI Button` template を追加する。
- Sprite 作成時に active 2D camera が無ければ prompt / status を出す。
- Hierarchy / Scene View / Asset Browser の作成導線を揃える。
- Sprite / Button 作成後に selection と Scene View focus を整える。

完了条件:

- 空 scene から 2D camera、sprite、button を迷わず作れる。
- Game View で作成直後の Sprite が見える。

### Phase 3: Validation

目的: 表示不能な Sprite / UI を保存前に検出する。

作業:

- Sprite / 2D Scene / UIButton validation を追加する。
- Inspector warning と status bar に出す。
- Save / Play 前に warning summary を出す。

完了条件:

- `SpriteComponent` だけ追加した entity が warning になる。
- 2D camera がない 2D scene が warning になる。
- buttonId 空 / 重複が warning になる。
- Game View の Fit / Fill / Pixel Perfect で button hit test がずれない。

### Phase 4: Runtime UI Render Pass

目的: editor overlay ではなく game render として Sprite / UI を描けるようにする。

作業:

- `UI2DRenderPass` を追加する。
- `UI2DDrawSystem::CollectDrawEntries` を render pass からも使う。
- DX11 / DX12 の texture binding に対応する。
- alpha blend / no depth write / sorted draw を実装する。
- Game View overlay との二重描画を避ける。

完了条件:

- Play build でも Sprite / UI が表示される。
- Editor Game View と runtime output の見た目が一致する。

## 14. 受け入れ条件

この章は Phase 別の受け入れ条件を定義する。最終完成条件と今回の最小完了条件を混ぜない。

### Phase 0 受け入れ条件

- 2D camera がない Game View で、原因と `Create 2D Camera` action が表示される。
- active 2D camera が inactive hierarchy 配下にある場合、inactive が原因だと表示される。
- `Camera2DComponent` があるが `TransformComponent` がない場合、no transform が原因だと表示される。
- `Use Scene View Camera` を押したときだけ Editor mode fallback preview が有効になる。
- Play mode では fallback preview が使われない。
- `Create 2D Camera` 後、Game View の `No active 2D camera` が消える。
- Debug x64 build が通る。

### Phase 1 受け入れ条件

- Inspector で Sprite texture を選択 / clear できる。
- Inspector で Sprite texture thumbnail が見える。
- Inspector で Sprite tint を `ColorEdit4` で変更できる。
- `ColorEdit4` drag 1 回につき Undo action は最大 1 件だけ積まれる。
- `Use Texture Size` で `RectTransformComponent::sizeDelta` が `ITexture::GetWidth()` / `ITexture::GetHeight()` の値になる。
- Sprite entity に必要 component がない場合、Inspector に warning と repair action が出る。
- repair action 後、Sprite が Scene View 2D mode に表示可能になる。
- Debug x64 build が通る。

### Phase 2 受け入れ条件

- Hierarchy から `Create Sprite` した entity が Scene View 2D mode に表示される。
- Texture asset を Scene View 2D mode に D&D すると Sprite が作成される。
- `Create UI Button` template で Sprite / RectTransform / CanvasItem / UIButton を持つ entity が作られる。
- Sprite / Button 作成時に active 2D camera がない場合、camera 作成導線が表示される。
- scene save / load 後も Sprite texture、tint、rect、canvas、buttonId が維持される。
- Debug x64 build が通る。

### Phase 3 受け入れ条件

- Sprite / Text / Button が Game View に表示される。
- Game View の Fit / Fill / Pixel Perfect / 1x / 2x / 3x で button hit test が視覚位置と一致する。
- Game View の余白クリックでは button click が発生しない。
- `UIButtonComponent` の click が `UIButtonClickEventQueue` に入る。
- Sprite / 2D Scene / UIButton validation が warning を出す。
- Debug x64 build が通る。

### Phase 4 受け入れ条件

- editor-only ImGui overlay を使わず、runtime render pass で Sprite / UI が表示される。
- DX11 / DX12 のどちらでも同じ draw entry / sorting で表示される。
- Play build でも Sprite / UI が表示される。
- Editor Game View と runtime output の見た目が一致する。
- Debug x64 build が通る。

## 15. 禁止事項

- Sprite を `TransformComponent` だけで描画対象にしない。
- `SpriteComponent` があるだけで hit test しない。hit test は `RectTransformComponent + CanvasItemComponent` を必須にする。
- inactive hierarchy の UI を描画 / click 対象にしない。
- Game View の 2D fallback を Play mode の正式 camera として扱わない。
- editor-only ImGui draw を runtime renderer と混同しない。
- texture path を absolute path として保存しない。`Data/...` 相対 path を保存する。
- `buttonId` 空の button を GameLoop の遷移条件として扱わない。

## 16. 最初に直す順序

1. Game View の `No active 2D camera` 診断表示を改善する。
2. Game View overlay に `Create 2D Camera` action を追加する。
3. `DrawSpriteComponentInspector` を作る。
4. Sprite texture slot に thumbnail / picker / D&D / clear を入れる。
5. Sprite tint を `ColorEdit4` にする。
6. `Use Texture Size` を追加する。
7. Sprite required component repair を追加する。
8. `Create UI Button` template を追加する。
9. Sprite / UIButton / 2D scene validation を追加する。
10. Runtime `UI2DRenderPass` を検討、実装する。
