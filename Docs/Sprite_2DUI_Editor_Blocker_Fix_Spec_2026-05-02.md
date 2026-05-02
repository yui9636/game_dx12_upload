# Sprite / 2D UI Editor Blocker Fix Spec

作成日: 2026-05-02
対象: Sprite 作成、Texture 選択、Inspector 更新、2D Scene View Transform/Gizmo
位置づけ: `Docs/Sprite_2DUI_Improvement_Spec_2026-05-02.md` の実装失敗を受けたブロッカー修正版。

---

## 1. 背景

前回の改善は Sprite の Inspector 表示や Game View 表示を中心に触ったが、実際の作業フローで一番重要な以下を満たせていない。

- Texture をクリックした時点で Inspector が Asset 表示へ切り替わり、Entity / Sprite 編集の文脈が失われる。
- Texture を選んでから `Create Sprite From Selected Texture` へ進む導線が不安定で、作成フローとして成立していない。
- Scene View に Sprite をドロップしても、作成後すぐに 2D Transform 操作を受け付けない。
- Sprite 作成直後の `RectTransform` と `TransformComponent` / `worldMatrix` の同期保証が弱い。
- Inspector の texture slot が「Entity を編集する UI」ではなく「Asset 選択状態」と競合している。

この仕様では、Renderer や最終 UI 機能より先に、Sprite を作る、選ぶ、テクスチャを入れる、Scene View で動かす、という最小編集ループを確実に成立させる。

---

## 2. 最優先ゴール

1. Texture をクリックしても、選択中 Entity の Inspector が勝手に消えない。
2. Texture を選んだ状態から、Sprite を確実に作成できる。
3. Texture を Sprite Inspector の slot に割り当てても、Inspector は Entity 表示のまま維持される。
4. Sprite を Scene View にドロップした直後、選択状態になり、Move / Rotate / Scale gizmo を受け付ける。
5. 2D 編集中は `RectTransform` を主データとし、`TransformComponent` は同期結果として扱う。

---

## 3. 非目標

- 新規 DX12 2D renderer の作成。
- Game View の完全な runtime UI renderer 実装。
- UI Button の最終デザインやイベントツール全体の完成。
- AssetBrowser 全体のリデザイン。
- Prefab override UI の全面改修。

今回の完了条件は、Editor 上の Sprite / 2D UI 作成と操作のブロッカー解除である。

---

## 4. 現状コード上の問題点

### 4.1 AssetBrowser の単クリックが Entity 編集文脈を破壊する

`Source/Asset/AssetBrowser.cpp` では asset item の単クリックで `EditorSelection::Instance().SelectAsset(asset.path.string())` が呼ばれる。

このため、Sprite Entity を選択している状態で texture をクリックすると、`SelectionType::Asset` へ切り替わり、Inspector は texture asset inspector を表示する。
結果として、Sprite Inspector の texture slot や create workflow から外れてしまう。

### 4.2 「選択中 texture」と「選択中 entity」が同じ selection に混ざっている

現状では `EditorSelection` が Entity selection と Asset selection を排他的に扱う。
しかし Sprite 編集では、以下を同時に持つ必要がある。

- 編集対象 Entity: Sprite / UI Button / Canvas item
- 操作用 Asset context: 今クリックした texture / font / prefab

これらは排他的にしてはいけない。

### 4.3 Sprite 作成後の Transform 同期が操作可能状態まで保証されていない

Scene View への drop は `HandleSceneAssetDrop()` で `BuildSingleSpriteSnapshot()` を作り、`CreateEntityAction` で生成している。

しかし 2D Sprite は `RectTransform` が主データで、gizmo は `TransformComponent::worldMatrix` を読んでいる。
作成直後に `RectTransform -> Transform -> worldMatrix` が同一フレームで反映されないと、gizmo が原点に出る、当たり判定がずれる、操作を受け付けない、などが起きる。

### 4.4 Inspector texture slot が transaction と selection を分離できていない

Sprite の texture slot は、クリック、popup、texture preview load、選択、Undo 記録を一つの即時 UI 内で行っている。
ここでは global selection を変えてはならない。
また texture 選択は「popup 内の候補をクリックした瞬間に Entity の SpriteComponent を変更する」処理であり、AssetBrowser の asset selection とは別物でなければならない。

---

## 5. 設計原則

### 5.1 Entity selection と Asset context を分離する

`EditorSelection` の `SelectionType::Entity` は、Sprite 編集中に維持する。
Texture のクリックは Entity selection を破壊せず、別の asset context に入れる。

推奨:

- `EditorAssetContext` のような小さい singleton を追加する。
- 最低限、以下を保持する。
  - `std::string activeAssetPath`
  - `AssetType activeAssetType`
  - `uint64_t revision`
- AssetBrowser の単クリックは `EditorAssetContext::SetActiveAsset(path)` を更新する。
- Inspector を Asset 表示に切り替える操作は、明示的な `Inspect Asset` 操作、または Entity が未選択の時だけに限定する。

代替:

- `EditorSelection` に Entity selection を保持したまま asset side selection を追加する。
- ただし `SelectionType::Asset` へ切り替えて Entity を失う実装は不可。

### 5.2 2D Entity 作成後には必ず post-create finalize を通す

Sprite / Text / UI Button / Empty 2D を作った直後に、以下を必ず行う。

```cpp
FinalizeCreated2DEntity(registry, entity);
```

仕様:

- `TransformComponent` がなければ追加する。
- `HierarchyComponent` がなければ追加する。
- `RectTransformComponent` がなければ追加する。
- `CanvasItemComponent` がなければ追加する。
- `RectTransformComponent` の値を `TransformComponent` に同期する。
- `TransformComponent::isDirty = true` にする。
- `HierarchySystem::MarkDirtyRecursive(entity, registry)` を呼ぶ。
- 同一フレームで gizmo が使えるよう、worldMatrix を更新する。
  - 既存 `HierarchySystem::Update(registry)` を呼べる場所なら呼ぶ。
  - 呼べない場合は `TransformComponent::worldPosition / worldScale / worldRotation / worldMatrix` を最低限同期する helper を用意する。
- `EditorSelection::SelectEntity(entity)` を呼ぶ。
- Scene View を focus し、`m_gizmoOperation = Translate` にする。
- 直前の mouse release が picking として処理され、生成直後の選択を上書きしないよう、pick pending を clear する。

### 5.3 2D 編集の主データは RectTransform

2D Scene View では、Inspector と gizmo のどちらから操作しても `RectTransformComponent` を主データとして扱う。

- Move: `RectTransformComponent::anchoredPosition`
- Rotate: `RectTransformComponent::rotationZ`
- Scale: `RectTransformComponent::scale2D`
- Size: `RectTransformComponent::sizeDelta`

`TransformComponent` は描画、gizmo、hit test 用の同期結果である。
2D 操作中に `TransformComponent` だけが更新される状態は禁止する。

### 5.4 Texture slot は global selection を変えない

Sprite Inspector の texture slot 操作は、常に選択中 Entity に対する編集として扱う。

- Texture thumbnail click: popup を開くだけ。
- Popup candidate click: `SpriteComponent::textureAssetPath` を変更する。
- `EditorSelection::SelectAsset()` は呼ばない。
- Inspector 表示は Entity のまま維持する。
- Undo は texture path 変更 1 操作につき 1 件。
- Popup open / close だけでは Undo を積まない。

---

## 6. 必須 UI 仕様

### 6.1 AssetBrowser

Texture を単クリックした場合:

- `EditorAssetContext.activeAssetPath` を texture path にする。
- Entity selection は維持する。
- Inspector は、選択中 Entity があれば Entity Inspector のまま。
- Entity が選択されていなければ Asset Inspector を表示してよい。

Texture を右クリックした場合:

- `Create Sprite`
- `Create UI Button`
- `Assign to Selected Sprite`

を表示する。

表示条件:

- `Create Sprite`: texture asset なら常に表示。
- `Create UI Button`: texture asset なら常に表示。
- `Assign to Selected Sprite`: 選択中 Entity が `SpriteComponent` を持つ場合のみ表示。

### 6.2 Hierarchy context menu

Hierarchy の空白右クリック:

- `Create Sprite From Active Texture`
- `Create UI Button From Active Texture`

を表示する。

条件:

- `EditorAssetContext.activeAssetPath` が texture の場合のみ表示。
- このメニューは `SelectionType::Asset` に依存しない。

Entity 右クリック:

- `Create Sprite Child From Active Texture`
- `Create UI Button Child From Active Texture`

を表示する。

条件:

- `EditorAssetContext.activeAssetPath` が texture の場合のみ表示。
- Prefab child lock は既存仕様どおり守る。

### 6.3 Inspector: SpriteComponent

SpriteComponent には専用 inspector を使う。

表示項目:

- Texture slot
- Texture path text
- Texture preview thumbnail
- Texture size
- `Use Texture Size`
- Tint `ColorEdit4`
- Missing components repair

Repair:

- `Add Transform`
- `Add RectTransform`
- `Add CanvasItem`
- `Activate Entity`
- `Show CanvasItem`

Repair button を押した frame は、archetype 移動後の古い component pointer を使わない。
処理後に inspector draw を打ち切り、次 frame に再取得する。

### 6.4 Inspector: Texture Asset

Texture Asset Inspector には Entity 作成導線を置く。

Texture asset を inspect している場合:

- `Create Sprite`
- `Create UI Button`
- `Assign to Selected Sprite` を表示する。

ただし、Texture asset inspect に切り替わるのは、ユーザーが明示的に asset inspect を選んだ時だけ。
単クリック texture で Sprite Entity Inspector が消える挙動は禁止する。

---

## 7. Sprite / UI Button 作成仕様

### 7.1 Create Sprite

作成される Entity:

- `NameComponent`
- `TransformComponent`
- `HierarchyComponent`
- `RectTransformComponent`
- `CanvasItemComponent`
- `SpriteComponent`

初期値:

- `NameComponent::name`: texture がある場合は texture stem、ない場合は `"Sprite"`
- `SpriteComponent::textureAssetPath`: texture path または empty
- `RectTransformComponent::anchoredPosition`: placement point
- `RectTransformComponent::sizeDelta`: texture size が取れる場合は width / height、取れない場合は 128 x 128
- `CanvasItemComponent::visible = true`
- `CanvasItemComponent::interactable = true`

作成後:

- `FinalizeCreated2DEntity()` を必ず通す。
- 作成 Entity を選択する。
- Inspector は Entity Inspector を表示する。
- Scene View gizmo は作成 Entity の中心に出る。

### 7.2 Create UI Button

Sprite の構成に加えて:

- `UIButtonComponent`

初期値:

- `UIButtonComponent::buttonId`: Entity name と同じ。重複する場合は suffix を付ける。
- `UIButtonComponent::enabled = true`
- `RectTransformComponent::sizeDelta`: texture があれば texture size、なければ 180 x 64

---

## 8. Scene View Drop 仕様

Texture を Scene View 2D mode に drop した場合:

1. Drop preview は mouse 位置の canvas point に出す。
2. Delivery frame で Sprite Entity を作成する。
3. Texture size を `ResourceManager::Instance().GetTexture(path)->GetWidth()/GetHeight()` から取得する。
4. `FinalizeCreated2DEntity()` を通す。
5. 作成 Entity を選択する。
6. その frame の picking は作成後選択を上書きしない。
7. 次 frame まで待たず、少なくとも直後の Scene View で gizmo が正しい位置に表示される。

禁止:

- Drop 後に Asset selection へ戻ること。
- Drop 後に Inspector が Texture Asset 表示になること。
- Drop 後に `TransformComponent` だけ存在し、`RectTransformComponent` とずれること。

---

## 9. Gizmo / Transform 操作仕様

2D mode で Sprite Entity が選択されている場合:

- Move gizmo は `RectTransformComponent::anchoredPosition` を更新する。
- Rotate gizmo は `RectTransformComponent::rotationZ` を更新する。
- Scale gizmo は `RectTransformComponent::scale2D` を更新する。
- 操作中は毎 frame `SyncRectTransformToTransform()` を呼ぶ。
- 操作終了時に Undo は 1 件だけ積む。
- 操作終了時に Prefab override を mark する。

Gizmo 表示位置:

- `RectTransformComponent` から同期済みの `TransformComponent::worldMatrix` を使う。
- 作成直後でも sprite center に出る。

Hit test:

- Sprite の picking は `UIHitTestSystem::PickTopmost()` を使う。
- `CanvasItemComponent::visible == false` は pick 不可。
- `HierarchyComponent::isActive == false` は pick 不可。

---

## 10. Undo 仕様

Undo 単位:

- Sprite 作成: 1 undo
- Texture assign: 1 undo
- Texture clear: 1 undo
- Use Texture Size: 1 undo
- Repair component add: 1 undo per button
- Gizmo drag: drag 開始から release まで 1 undo
- Tint ColorEdit4 drag: drag 開始から release まで 1 undo

禁止:

- ColorEdit4 や gizmo drag で毎 frame Undo を積むこと。
- Popup open / close だけで Undo を積むこと。
- Texture click だけで Undo を積むこと。

---

## 11. 実装対象ファイル

想定変更箇所:

- `Source/Asset/AssetBrowser.cpp`
- `Source/Engine/EditorSelection.h`
- `Source/Engine/EditorSelection.cpp`
- 新規: `Source/Engine/EditorAssetContext.h`
- 新規: `Source/Engine/EditorAssetContext.cpp`
- `Source/Hierarchy/HierarchyECSUI.cpp`
- `Source/Inspector/InspectorECSUI.cpp`
- `Source/Layer/EditorLayerSceneView.cpp`
- `Source/Layer/EditorLayerInternal.h`
- 必要なら `Game.vcxproj`
- 必要なら `Game.vcxproj.filters`

`Game.vcxproj` / `.filters` は新規 cpp/h を追加する時だけ触る。

---

## 12. 実装フェーズ

### Phase 0: 失敗再現ログ

目的:

- 修正前に、ユーザー報告の 2 症状を再現可能にする。

確認項目:

- Texture 単クリックで Inspector が Entity から Asset に切り替わる。
- Sprite drop 後に gizmo が出ない、または操作できない。

成果物:

- 短い再現メモを commit message または PR description に残す。

### Phase 1: Asset context 分離

目的:

- Texture click で Entity selection を破壊しない。

作業:

- `EditorAssetContext` を追加する。
- AssetBrowser の単クリックを `EditorAssetContext::SetActiveAsset()` に変更する。
- `EditorSelection::SelectAsset()` は明示 inspect の時だけ呼ぶ。
- Hierarchy の selected texture create 系は `EditorAssetContext` を見る。

受け入れ条件:

- Sprite Entity 選択中に texture を単クリックしても Inspector は Sprite Entity のまま。
- Hierarchy 右クリックで active texture から Sprite を作成できる。

### Phase 2: 2D creation finalizer

目的:

- Sprite / UI Button 作成直後に Scene View 操作可能にする。

作業:

- `FinalizeCreated2DEntity()` を追加する。
- Scene View drop、Hierarchy create、AssetBrowser create、Inspector create の全経路で呼ぶ。
- 作成後 selection / focus / pick pending clear を統一する。

受け入れ条件:

- Texture drop 直後、Sprite が選択され、gizmo が center に出る。
- Move / Rotate / Scale が `RectTransform` に反映される。

### Phase 3: Sprite Inspector texture transaction

目的:

- Texture slot 操作を Entity 編集として閉じる。

作業:

- Texture slot popup は local state として扱う。
- Popup 内の texture candidate click は `SpriteComponent` のみ変更する。
- Global asset selection は変更しない。
- Undo 粒度を 1 操作 1 件にする。

受け入れ条件:

- Sprite Inspector の texture thumbnail をクリックしても Inspector は切り替わらない。
- Texture を選ぶと Sprite に反映され、Undo 1 回で戻る。

### Phase 4: UI Button create flow

目的:

- Texture から UI Button を確実に作れる。

作業:

- Asset context texture から UI Button を作成するメニューを追加する。
- 作成後 `UIButtonComponent` と Sprite 必須 component を持つ。
- 作成後 Scene View で transform 操作可能にする。

受け入れ条件:

- Texture 右クリック、Hierarchy 右クリック、Asset Inspector のいずれかから UI Button を作成できる。

### Phase 5: Build and regression check

目的:

- 変更が Editor 全体を壊していないことを確認する。

コマンド:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Game.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m
```

受け入れ条件:

- 0 error。
- 新規警告が出る場合は内容を明記する。
- ビルド生成物 `.cso` が dirty になっても、意図しない場合は commit しない。

---

## 13. 受け入れ条件

### AC-1: Texture click does not destroy Sprite editing

手順:

1. Sprite Entity を選択する。
2. AssetBrowser で png texture を単クリックする。

期待:

- Inspector は Sprite Entity のまま。
- Active texture context はクリックした texture になる。
- Sprite Entity selection は維持される。

### AC-2: Create Sprite from active texture

手順:

1. AssetBrowser で png texture を単クリックする。
2. Hierarchy 空白を右クリックする。
3. `Create Sprite From Active Texture` を選ぶ。

期待:

- Sprite Entity が作成される。
- 作成 Entity が選択される。
- Inspector は Entity Inspector。
- SpriteComponent に texture path が入っている。
- RectTransform sizeDelta は texture width / height。

### AC-3: Scene View drop creates immediately editable Sprite

手順:

1. Scene View を 2D mode にする。
2. AssetBrowser から png texture を Scene View へ drop する。
3. そのまま Move gizmo でドラッグする。

期待:

- Sprite が drop 位置に出る。
- 作成 Entity が選択される。
- Gizmo が Sprite center に出る。
- Move 操作を受け付ける。
- `RectTransform::anchoredPosition` が変わる。
- `TransformComponent` も同期される。

### AC-4: Sprite Inspector texture slot is stable

手順:

1. Sprite Entity を選択する。
2. Sprite Inspector の texture thumbnail / slot をクリックする。
3. Popup から texture を選ぶ。

期待:

- Inspector は Entity Inspector のまま。
- SpriteComponent texture path が更新される。
- Undo 1 回で前の texture path に戻る。
- AssetBrowser selection は変更されない。

### AC-5: Create Sprite without texture remains editable

手順:

1. Hierarchy から `Create Sprite` を選ぶ。
2. 作成された Sprite を Scene View で move する。
3. Inspector texture slot から texture を割り当てる。

期待:

- texture が empty でも gizmo 操作できる。
- texture 割り当て後も Inspector は Entity のまま。
- `Use Texture Size` が使える。

### AC-6: UI Button create flow

手順:

1. Texture を active context にする。
2. `Create UI Button From Active Texture` を実行する。

期待:

- Entity に `SpriteComponent`, `UIButtonComponent`, `RectTransformComponent`, `CanvasItemComponent`, `TransformComponent`, `HierarchyComponent` がある。
- 作成直後に Scene View gizmo 操作できる。
- Inspector は Entity のまま。

---

## 14. 禁止事項

- Texture asset 単クリックで Entity selection を解除しない。
- Sprite Inspector の texture slot クリックで Asset Inspector へ切り替えない。
- Sprite 作成後、`RectTransform` と `TransformComponent` が同期されていない状態を残さない。
- 2D mode の gizmo 操作で `TransformComponent` だけを更新しない。
- 作成直後の Scene View picking で、生成された Entity selection を上書きしない。
- Build の副産物 `.cso` を理由なく commit しない。

---

## 15. 実装完了の定義

以下を全て満たしたら完了。

- AC-1 から AC-6 まで手動確認済み。
- Debug x64 build が 0 error。
- 変更範囲が Sprite / 2D UI Editor flow に限定されている。
- 仕様書と実装が一致している。
- 既存の未関係 dirty files を commit しない。
