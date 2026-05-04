# UI Button / 2D UI Button Improvement Spec

作成日: 2026-05-04
対象: `Create UI Button`, `UIButtonComponent`, 2D Scene View / Game View, Inspector editing, GameLoop button click transition

---

## 1. 背景

現在の UI Button は、最低限の component とクリック検出の入口は存在する。

- `UIButtonComponent` は `buttonId` と `enabled` のみを持つ。
- `Create UI Button` は `SpriteComponent`, `RectTransformComponent`, `CanvasItemComponent`, `UIButtonComponent` を持つ entity を生成する。
- texture asset から `Create UI Button` できる。
- `UIButtonClickSystem` は `MouseButtonDown` を `UIHitTestSystem::PickTopmost()` に通し、hit entity の `UIButtonComponent::buttonId` を `UIButtonClickEventQueue` に積む。
- `EngineKernel` は Play 中に `UIButtonClickSystem` を呼ぶ。
- ただし `GameLoopSystem` は現在 `clickQueue` を未使用にしており、UI Button click は scene transition 条件として完結していない。
- Inspector は `UIButtonComponent` を汎用 component meta UI で表示しているだけで、Button 専用の操作性、状態確認、必要 component 修復が不足している。

このため、ユーザーから見ると UI Button は「作れるが、何が足りないのか、クリックが本当にGameLoopへ届くのか、Inspectorでどう整えるのか」が不明確な状態になっている。

---

## 2. 最優先ゴール

1. `Create UI Button` 直後に Scene View / Game View で見える、選べる、動かせる Button を作る。
2. Button entity を選択したら Inspector 内で Button 用設定が完結する。
3. Button の texture / 表示状態 / button id / enabled / interactable を分かりやすく編集できる。
4. AssetBrowser から Inspector の texture slot へ drag & drop する経路を主操作にする。
5. Game View 上のクリック座標を Button rect 空間へ正しく変換し、ズレたクリックを出さない。
6. UI Button click を GameLoop transition 条件として使用できるようにする。

---

## 3. 非ゴール

今回の改善では以下を必須にしない。

- DX12 専用の新規 2D renderer 実装。
- Canvas layout system の全面刷新。
- 複雑な UI animation timeline。
- ScrollView / Toggle / Slider / InputField など Button 以外の Widget 実装。
- Effect Editor / Material Editor / Player Editor の picker や専用 UI の変更。
- Web / HTML 風の CSS layout。

---

## 4. Phase 方針

受け入れ条件は Phase ごとに分ける。Phase 1 の完了条件に、Phase 3 の GameLoop 全体完成条件を混ぜない。

### Phase 1: Editor 作成・Inspector・見た目の最低完成

目的:

- Button を作成し、Inspector で Button として扱える状態にする。
- Scene View / Game View preview で表示と選択が破綻しないようにする。

対象:

- `HierarchyECSUI.cpp`
- `InspectorECSUI.cpp`
- `UIButtonComponent.h`
- `SpriteComponent`, `RectTransformComponent`, `CanvasItemComponent` との連携

Phase 1 受け入れ条件:

- `Create UI Button` 直後に以下 component を持つ。
  - `NameComponent`
  - `TransformComponent`
  - `HierarchyComponent`
  - `RectTransformComponent`
  - `CanvasItemComponent`
  - `SpriteComponent`
  - `UIButtonComponent`
- 初期 `NameComponent::name` は `"Button"`。
- 初期 `UIButtonComponent::buttonId` は `"Button"`。
- 初期 `RectTransformComponent::sizeDelta` は `180 x 64`。
- 初期 `CanvasItemComponent::visible` と `interactable` は `true`。
- 作成直後に entity が選択され、Inspector が Entity Inspector のまま表示される。
- Button 専用 Inspector が表示される。
- texture は AssetBrowser から Inspector の texture slot へ drag & drop で設定できる。
- texture picker / 別 window / 個別 asset browser popup は作らない。
- `Use Texture Size` は `ResourceManager::GetTexture(path)->GetWidth()/GetHeight()` で取得した値を `RectTransformComponent::sizeDelta` に反映する。
- 欠損 component がある場合は Inspector に修復ボタンを出す。
- 修復後の frame では古い component pointer を使わない。

### Phase 2: Runtime click 判定の正確化

目的:

- Button click を「押した瞬間」ではなく「Button 上で押して、同じ Button 上で離した」操作として扱う。
- Game View のスケールや viewport rect によるズレをなくす。

対象:

- `UIButtonClickSystem`
- `UIButtonClickEventQueue`
- `UIHitTestSystem`
- `EngineKernel` の Game View rect 受け渡し

Phase 2 受け入れ条件:

- 左 mouse button down で押下候補を capture する。
- left mouse button up 時に、down と up が同じ enabled Button 上なら click event を1回だけ発行する。
- drag で別 Button に移動して release した場合は click を発行しない。
- Button が down 中に disabled / invisible / inactive になった場合は click を発行しない。
- `CanvasItemComponent::visible == false` は描画も hit も対象外。
- `CanvasItemComponent::interactable == false` は hit/click 対象外。
- `UIButtonComponent::enabled == false` は click event 対象外。ただし Inspector 上では disabled 状態として見える。
- click event は `UIButtonClickEventQueue::Push(buttonId)` に1件だけ積む。
- 同一 frame で同じ Button を複数回押せない通常入力では重複しない。

### Phase 3: GameLoop transition 連携

目的:

- UI Button click を scene transition 条件として使えるようにする。

対象:

- `GameLoopAsset`
- `GameLoopSystem`
- `GameLoopEditorPanel`
- `ValidateGameLoopAsset`

Phase 3 受け入れ条件:

- `GameLoopTransitionInput` に `uiButtonId` 相当の文字列を追加する、または transition condition 構造へ `UIButtonClicked` type を追加する。
- 保存形式は version bump する。旧 asset は keyboard / gamepad だけでも読み込める。
- `GameLoopSystem::Update()` は `clickQueue.Contains(uiButtonId)` を transition 条件として評価する。
- keyboard / gamepad / UI Button のいずれかが bind されていれば transition input は valid とする。
- GameLoop Editor の transition Inspector で Button ID を編集できる。
- Button ID 欄は空欄を許すが、keyboard / gamepad / UI Button の全てが未設定なら validate error を出す。
- Play 中に Button click で scene transition が発火する。

---

## 5. UIButtonComponent 仕様

### 5.1 最小データ

既存:

```cpp
struct UIButtonComponent
{
    std::string buttonId;
    bool enabled = true;
};
```

Phase 1 ではこの最小構造を維持してよい。

### 5.2 拡張候補

Phase 2 以降で必要なら以下を追加する。

```cpp
enum class UIButtonVisualTransition
{
    None,
    Tint,
    SpriteSwap
};

struct UIButtonComponent
{
    std::string buttonId;
    bool enabled = true;

    UIButtonVisualTransition visualTransition = UIButtonVisualTransition::Tint;
    DirectX::XMFLOAT4 normalTint   = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 hoverTint    = { 1.1f, 1.1f, 1.1f, 1.0f };
    DirectX::XMFLOAT4 pressedTint  = { 0.85f, 0.85f, 0.85f, 1.0f };
    DirectX::XMFLOAT4 disabledTint = { 0.45f, 0.45f, 0.45f, 0.7f };
};
```

注意:

- Phase 1 では state tint の保存まで必須にしない。
- 拡張する場合は `PrefabSystem` serialization と `ComponentMeta.generated.h` 更新を必須にする。

---

## 6. Create UI Button 仕様

### 6.1 Hierarchy context menu

`Create UI Button` は Button entity を root に作る。

初期値:

- `NameComponent::name`: `"Button"`
- `UIButtonComponent::buttonId`: `"Button"`
- `UIButtonComponent::enabled`: `true`
- `RectTransformComponent::sizeDelta`: `{ 180.0f, 64.0f }`
- `RectTransformComponent::pivot`: `{ 0.5f, 0.5f }`
- `CanvasItemComponent::visible`: `true`
- `CanvasItemComponent::interactable`: `true`
- `SpriteComponent::textureAssetPath`: empty allowed

作成後:

- `Editor2D::FinalizeCreatedEntity()` 相当を通す。
- 作成 entity を選択する。
- Scene View gizmo operation を Move にしてよい。

### 6.2 Create UI Button Child

選択 entity の子として Button を作る。

- `PrefabSystem::CanCreateChild(parent)` が false の場合は作成しない。
- warning を出して終了する。
- 親が 2D component を持っていなくても作成自体は許可してよいが、Inspector に親子関係と RectTransform の状態を表示する。

### 6.3 Texture asset からの作成

AssetBrowser の texture asset から `Create UI Button` した場合:

- entity 名は texture stem を使う。
- `buttonId` も texture stem を使う。
- `SpriteComponent::textureAssetPath` に texture path を入れる。
- texture size が取得できる場合、`RectTransformComponent::sizeDelta` に反映する。
- 作成後は entity 選択を維持する。

---

## 7. UIButton Inspector 仕様

`UIButtonComponent` は汎用 meta UI ではなく専用 Inspector を持つ。

表示項目:

- Button ID
- Enabled
- Interactable status
- Visual status
- Texture slot
- Use Texture Size
- Missing component repair
- Runtime click debug
- GameLoop binding helper

### 7.1 Button ID

UI:

- `InputText`
- `Generate From Name` button
- 空欄 warning

Undo:

- 入力中に毎 frame Undo を積まない。
- edit 開始時の `UIButtonComponent` を保存し、`IsItemDeactivatedAfterEdit()` で1件だけ Undo を積む。

仕様:

- `buttonId` は GameLoop transition の UI Button ID と一致する必要がある。
- 空欄の場合 click event は発行しない。
- `NameComponent::name` と一致していなくてもよい。ただし Inspector に差分表示を出す。

### 7.2 Enabled / Interactable

UI:

- `Enabled` checkbox: `UIButtonComponent::enabled`
- `Interactable` checkbox: `CanvasItemComponent::interactable`

仕様:

- `enabled == false` は Button 固有の無効状態。
- `interactable == false` は hit test 全体から除外する状態。
- Inspector では両方の違いを表示する。

Undo:

- checkbox 変更ごとに1件。
- Prefab override を mark する。

### 7.3 Texture slot

Button 専用 Inspector 内に texture slot を表示する。

操作:

- AssetBrowser から `.png`, `.jpg`, `.jpeg`, `.tga`, `.dds`, `.bmp`, `.hdr` を drag & drop する。
- 成功時は `SpriteComponent::textureAssetPath` を更新する。
- `Use Texture Size` で texture 実サイズを `RectTransformComponent::sizeDelta` に反映する。

禁止:

- Button texture picker popup。
- Button 専用 asset browser window。
- texture asset をクリックしただけで entity selection を奪う挙動。

### 7.4 Missing component repair

Button として必要な component:

- `TransformComponent`
- `HierarchyComponent`
- `RectTransformComponent`
- `CanvasItemComponent`
- `SpriteComponent`
- `UIButtonComponent`

修復ボタン:

- `Add Transform`
- `Add Hierarchy`
- `Add RectTransform`
- `Add CanvasItem`
- `Add Sprite`
- `Show CanvasItem`
- `Enable Interactable`
- `Enable Button`
- `Activate Entity`

注意:

- component 追加後は archetype が移動するため、その frame では古い pointer を使わず `return` する。
- 修復操作は Undo 対象にする。

---

## 8. Runtime click 座標仕様

### 8.1 入力座標

`InputEvent::mouseButton.x/y` は window screen 座標として扱う。

`UIButtonClickSystem` に渡す `gameViewRect` は実際に Game View texture が描画されている rect でなければならない。

禁止:

- Game View window 全体 rect を使う。
- toolbar / padding / letterbox を含んだ rect を使う。
- Fit / Fill / Pixel Perfect 後の描画領域と異なる rect を使う。

### 8.2 変換手順

1. `screenPoint` が `gameViewRect` 内にあるか判定する。
2. `screenPoint` を `gameViewRect` local にする。
3. local を NDC に変換する。
4. `view * projection` の inverse で world point を求める。
5. Z=0 plane と交差させる。
6. entity の world transform inverse で local rect 空間へ変換する。
7. `RectTransformComponent::pivot`, `sizeDelta`, `scale2D`, rotation を反映した bounds に入っているか判定する。
8. `sortingLayer`, `orderInLayer`, `z` で topmost を決める。

現在の `UIHitTestSystem` はこの流れに近い。Phase 2 では Game View rect の正確性と click capture/release を優先して検証する。

### 8.3 Fit / Fill / Pixel Perfect

将来 Game View に解像度プリセットや Fit / Fill / Pixel Perfect が入る場合:

- `m_gameViewRect` ではなく `m_gameViewContentRect` を定義する。
- `UIButtonClickSystem` には content rect のみ渡す。
- letterbox 領域クリックは UI hit しない。
- Pixel Perfect は描画時の snap と hit test の logical rect を混同しない。

---

## 9. Visual state 仕様

Phase 1:

- 表示は `SpriteComponent` の texture / tint と fallback quad でよい。
- Button が disabled の場合、Inspector 上で status を表示する。

Phase 2:

- hover / pressed / disabled state を runtime で計算する。
- state は click 判定とは分離する。

Phase 3 以降:

- `UIButtonVisualTransition::Tint` を採用する場合、描画時に Sprite tint に state tint を乗算する。
- `SpriteSwap` は texture path を複数持つ必要があるため Phase 1 では必須にしない。

---

## 10. GameLoop 連携仕様

現在 `GameLoopSystem::Update()` は `clickQueue` を受け取っているが使用していない。

### 10.1 Transition input 拡張案

最小変更:

```cpp
struct GameLoopTransitionInput
{
    uint32_t keyboardScancode = 0;
    uint8_t gamepadButton = 0xFF;
    std::string uiButtonId;
};
```

判定:

- `keyboardScancode != 0` かつ KeyDown で true。
- `gamepadButton != 0xFF` かつ GamepadButtonDown で true。
- `!uiButtonId.empty()` かつ `clickQueue.Contains(uiButtonId)` で true。

保存:

- JSON に `uiButtonId` を追加する。
- 旧 asset 読み込みでは未指定なら empty。
- `GameLoopAsset::version` を上げる。

Validation:

- `keyboardScancode == 0`
- `gamepadButton == 0xFF`
- `uiButtonId.empty()`

上記すべてを満たす場合のみ input unbound error。

### 10.2 Editor UI

GameLoop transition Inspector:

- Keyboard binding
- Gamepad binding
- UI Button ID input

補助:

- scene 内の `UIButtonComponent::buttonId` 一覧を combo 表示してもよい。
- ただし Phase 1 では一覧 picker は必須にしない。

---

## 11. Undo / Prefab Override

対象:

- `UIButtonComponent::buttonId`
- `UIButtonComponent::enabled`
- `CanvasItemComponent::interactable`
- `SpriteComponent::textureAssetPath`
- `RectTransformComponent::sizeDelta`

ルール:

- text input / drag / color edit は操作完了時に1件だけ Undo。
- checkbox / button 操作は即時1件。
- texture drop は1件。
- `Use Texture Size` は `RectTransformComponent` の変更1件。
- component repair は `OptionalComponentUndoAction` を使う。
- 変更後は `PrefabSystem::MarkPrefabOverride(entity, registry)` を呼ぶ。

---

## 12. AssetBrowser / Selection 仕様

Button texture 設定:

- 主経路は Inspector texture slot への drag & drop。
- AssetBrowser の texture click は active asset 更新だけにし、entity selection を奪わない現行方針を維持する。

Context menu:

- `Create UI Button`
- `Create UI Button From Active Texture`
- `Assign to Selected Button` は Phase 2 以降でよい。

禁止:

- texture click だけで selected Button に自動 assign。
- Button 専用の別 asset picker window。

---

## 13. Debug / Diagnostics

Button Inspector に以下の status を出す。

- Button ID empty
- Missing `RectTransformComponent`
- Missing `CanvasItemComponent`
- Missing `SpriteComponent`
- `CanvasItem.visible == false`
- `CanvasItem.interactable == false`
- `UIButton.enabled == false`
- Texture load failed
- GameLoop transition に同じ Button ID が見つからない

Runtime debug overlay は Phase 2 以降:

- hovered Button ID
- pressed Button ID
- last clicked Button ID
- clickQueue contents for current frame

---

## 14. 受け入れテスト

### Phase 1 tests

- Hierarchy 右クリック `Create UI Button` で Button entity が作られ選択される。
- Scene View 2D で Button rect が表示される。
- Transform gizmo で移動・回転・scale できる。
- Inspector で Button ID を編集できる。
- Inspector で `Enabled` を切り替えられる。
- Inspector texture slot へ texture asset を drop して sprite 表示が更新される。
- `Use Texture Size` で rect size が texture size になる。
- component を削った Button entity で repair button が出る。
- repair 後にクラッシュしない。

### Phase 2 tests

- Game View で Button を mouse down/up すると click event が1回だけ出る。
- down は Button 上、up は Button 外なら click event が出ない。
- disabled Button は click event が出ない。
- invisible / inactive / non-interactable Button は click event が出ない。
- 重なった Button は sortingLayer / orderInLayer / z の最前面だけ click される。
- Game View の toolbar や letterbox 領域をクリックしても Button が反応しない。

### Phase 3 tests

- GameLoop transition に `uiButtonId` を設定できる。
- Play 中に Button click で scene transition が発火する。
- keyboard / gamepad の既存 transition は壊れない。
- old GameLoop asset を読み込める。
- `uiButtonId` 未設定かつ keyboard/gamepad 未設定の transition は validate error になる。

---

## 15. 実装順序

推奨順:

1. `UIButtonComponent` 専用 Inspector を追加する。
2. `Create UI Button` / texture 生成初期値を確認し、不足分を補う。
3. texture slot / `Use Texture Size` / missing component repair を実装する。
4. `UIButtonClickSystem` を down/up capture 方式に変更する。
5. Game View content rect を明確化し、click system に渡す rect を統一する。
6. `GameLoopTransitionInput` に `uiButtonId` を追加する。
7. JSON serialization / validation / editor UI を更新する。
8. Phase ごとの受け入れテストを通す。

---

## 16. 注意点

- Effect Editor の asset picker / window には影響させない。
- Material Editor の texture picker には影響させない。
- SpriteComponent Inspector の texture slot と Button Inspector の texture slot は同じ思想にするが、Button 専用 popup は作らない。
- Game View fallback camera は preview 用であり、Play 中の Button click 判定は active Camera2D を基準にする。
- `No active 2D camera` 状態で editor が crash/assert しないこと。ただし runtime click は発火しない。
- `buttonId` と entity name は別物として扱う。勝手に同期しない。
