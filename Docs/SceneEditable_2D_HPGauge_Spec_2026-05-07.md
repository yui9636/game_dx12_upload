# Scene Editable 2D HP Gauge Spec

Created: 2026-05-07

## 1. Purpose

HPゲージを `GameLayer` の直書きHUDから切り離し、SceneView上に配置できる2D UIとして作成・保存・再利用できるようにする。

この仕様のゴールは、以下を成立させること。

- Player / Enemy / Boss のHPゲージを scene entity として配置できる
- 位置、サイズ、色、フォント、表示順、fill方向を Inspector / SceneView で編集できる
- HP値の更新は runtime system が行い、レイアウト情報は scene asset に残る
- 既存の `RectTransformComponent` / `CanvasItemComponent` / `SpriteComponent` / `TextComponent` の流れを壊さない
- 将来のUIエディタやprefab化に拡張できる

## 2. Non Goals

最初の実装では以下は扱わない。

- 3D頭上ゲージ
- ワールド座標を2Dへ投影するHPゲージ
- ステージ上のHPバー billboard
- UIアニメーション用タイムライン
- 複雑なmask / stencil / nine-slice
- UI専用ノードエディタ

ただし、これらを後で足せるように、HP値の解決、見た目、表示制御は分離しておく。

## 3. Current Situation

現状のHP表示は主に `GameLayer::InitializeHUD()` / `GameLayer::ApplyHUDState()` 側で作られている。

問題点:

- HPゲージの位置とサイズがコード固定
- SceneView上で配置できない
- scene保存/読み込みの対象にならない
- player/boss/world float の分岐が `GameLayer` 側に寄っている
- `TextComponent` で作ったUI文字と、`UIHPText2D` のような runtime UI が別系統になっている

一方、既存の2D UI entityには以下の土台がある。

- `RectTransformComponent`: 2D位置、サイズ、pivot、anchor、回転、scale
- `CanvasItemComponent`: visible、sortingLayer、orderInLayer、pixelSnap
- `SpriteComponent`: texture、tint
- `TextComponent`: text、fontAssetPath、fontSize、color、alignment、wrapping
- `UI2DDrawSystem`: 2D UI draw entry の収集
- `UI2DSpriteExtractSystem`: `SpriteComponent` を `RenderQueue` へ抽出

したがって、新しいHPゲージはこのECS UI上に乗せる。

## 4. Desired Authoring Model

HPゲージは「特別なHUDクラス」ではなく、scene entity の組み合わせで表現する。

基本構造:

```text
HPGaugeRoot
  HPGauge_Background
  HPGauge_Fill
  HPGauge_DamagePreview    optional
  HPGauge_Text             optional
  HPGauge_Label            optional
```

`HPGaugeRoot` は binding と全体表示制御を持つ。

子entityは普通の2D UIとして配置する。

例:

```text
PlayerHPGauge
  BG      SpriteComponent + RectTransformComponent
  Fill    SpriteComponent + RectTransformComponent + HPGaugeFillComponent
  Text    TextComponent   + RectTransformComponent + HPGaugeTextComponent
```

敵用やボス用も同じprefabから作る。違うのは binding target とレイアウトだけ。

## 5. Component Design

### 5.1 HPGaugeBindingComponent

HPゲージ全体の対象解決と表示制御を担当する。

```cpp
enum class HPGaugeTargetMode : uint8_t
{
    ExplicitEntity = 0,
    FirstPlayer = 1,
    FirstEnemy = 2,
    FirstBoss = 3,
    LockedOnTarget = 4
};

struct HPGaugeBindingComponent
{
    HPGaugeTargetMode targetMode = HPGaugeTargetMode::ExplicitEntity;
    EntityID explicitTarget = Entity::NULL_ID;

    bool visibleWhenNoTarget = false;
    bool hideWhenDead = false;
    bool hideWhenFull = false;

    float smoothingSpeed = 0.0f;
    float damagePreviewDelay = 0.35f;
    float damagePreviewSpeed = 6.0f;

    float currentRatio = 1.0f;
    float displayedRatio = 1.0f;
    float delayedRatio = 1.0f;
    int currentHP = 0;
    int maxHP = 0;
};
```

`currentRatio` は実HP比率。  
`displayedRatio` はなめらか表示用。  
`delayedRatio` は白/赤の遅延ダメージバー用。

最初の実装では `ExplicitEntity`、`FirstPlayer`、`FirstEnemy` まででよい。`LockedOnTarget` は lock-on system の安定後に追加する。

### 5.2 HPGaugeFillComponent

HP比率に応じて Sprite の表示幅、UV、色を制御する。

```cpp
enum class HPGaugeFillDirection : uint8_t
{
    LeftToRight = 0,
    RightToLeft = 1,
    BottomToTop = 2,
    TopToBottom = 3
};

enum class HPGaugeColorMode : uint8_t
{
    Fixed = 0,
    Threshold = 1,
    Gradient = 2
};

struct HPGaugeFillComponent
{
    EntityID bindingRoot = Entity::NULL_ID;
    HPGaugeFillDirection direction = HPGaugeFillDirection::LeftToRight;

    bool useDisplayedRatio = true;
    bool preserveAuthoredRect = true;
    float minVisibleRatio = 0.0f;

    HPGaugeColorMode colorMode = HPGaugeColorMode::Threshold;
    DirectX::XMFLOAT4 highColor = { 0.18f, 0.86f, 0.36f, 1.0f };
    DirectX::XMFLOAT4 midColor  = { 0.95f, 0.72f, 0.16f, 1.0f };
    DirectX::XMFLOAT4 lowColor  = { 0.95f, 0.16f, 0.12f, 1.0f };
    float midThreshold = 0.5f;
    float lowThreshold = 0.25f;
};
```

重要: runtime system が authored `RectTransformComponent.sizeDelta` を直接書き換え続けると、scene編集値とruntime値が混ざる。  
そのため `preserveAuthoredRect = true` の場合は、描画時だけ effective rect / UV crop を使う。

このためには `UI2DSpritePacket` へ以下のような描画時パラメータを足す。

```cpp
float fillRatio = 1.0f;
HPGaugeFillDirection fillDirection = HPGaugeFillDirection::LeftToRight;
bool useFillClip = false;
```

最初の実装では `SpriteRenderer::DrawQuad` にUV cropがないため、以下のどちらかが必要。

推奨:

- `UI2DSpritePacket` に `sourceRectNorm` を追加
- `UI2DSpriteRenderSystem` が fill方向に応じて quadの頂点とUVを切る

暫定:

- runtimeで `RectTransformComponent.sizeDelta` を変更する
- ただし authored base size を `HPGaugeFillRuntimeComponent` に保持する

長期的には推奨案に寄せる。

### 5.3 HPGaugeTextComponent

HP文字列を `TextComponent` に流し込む。

```cpp
enum class HPGaugeTextFormat : uint8_t
{
    CurrentSlashMax = 0, // "100 / 100"
    CurrentOnly = 1,    // "100"
    Percent = 2,        // "75%"
    LabelAndValue = 3   // "HP 100 / 100"
};

struct HPGaugeTextComponent
{
    EntityID bindingRoot = Entity::NULL_ID;
    HPGaugeTextFormat format = HPGaugeTextFormat::CurrentSlashMax;
    std::string label = "HP";
    bool hideWhenFull = false;
    bool hideWhenNoTarget = true;
};
```

`TextComponent` の `fontAssetPath`、`fontSize`、`color`、`alignment` は既存のInspectorで編集する。

### 5.4 HPGaugeVisibilityComponent optional

複数の子entityをまとめて表示/非表示にしたい場合の補助。

```cpp
struct HPGaugeVisibilityComponent
{
    EntityID bindingRoot = Entity::NULL_ID;
    bool inheritRootVisibility = true;
};
```

最初は必須ではない。`HPGaugeSystem` が root の子階層をたどって `CanvasItemComponent.visible` を制御できるなら不要。

## 6. Runtime System

### 6.1 HPGaugeSystem::Update

実行順は `HealthSystem` の後、UI draw extract の前。

推奨位置:

```text
DamageSystem
HealthSystem
HPGaugeSystem
HUDBindingSystem legacy optional
ApplyHUDState legacy optional
...
UI2DSpriteExtractSystem
```

処理:

1. `HPGaugeBindingComponent` を持つrootを列挙
2. targetを解決
3. targetの `HealthComponent` を読む
4. `currentRatio` / `displayedRatio` / `delayedRatio` を更新
5. root配下の `HPGaugeFillComponent` へratioを反映
6. root配下の `HPGaugeTextComponent` へ文字列を反映
7. 表示条件に応じて `CanvasItemComponent.visible` を制御

target解決:

- `ExplicitEntity`: `explicitTarget`
- `FirstPlayer`: `PlayerTagComponent + HealthComponent`
- `FirstEnemy`: `EnemyTagComponent + HealthComponent`
- `FirstBoss`: 当面は `EnemyTagComponent` の最初、後で `BossTagComponent`
- `LockedOnTarget`: 後回し

### 6.2 Runtime Ratio Application

推奨実装は「描画packetにratioを載せる」方式。

理由:

- scene上の authored rect を壊さない
- SceneViewで編集中の幅とGameViewのruntime幅が混ざらない
- Undo/Redoや保存時にHP減少後の幅が保存されない
- fill方向やUV cropを描画側で一元化できる

必要変更:

- `RenderQueue::UI2DSpritePacket`
  - `bool useFillClip`
  - `float fillRatio`
  - `uint8_t fillDirection`
  - optional `sourceRectNorm`
- `UI2DSpriteExtractSystem`
  - `HPGaugeFillComponent` があれば packet に fill情報を詰める
- `UI2DSpriteRenderSystem`
  - fill情報に応じて quad corners とUVを調整
- `SpriteRenderer`
  - `DrawQuad` にUV指定版を追加

暫定実装を選ぶなら、`HPGaugeFillRuntimeComponent` を用意する。

```cpp
struct HPGaugeFillRuntimeComponent
{
    DirectX::XMFLOAT2 authoredSizeDelta = { 0.0f, 0.0f };
    bool captured = false;
};
```

ただし暫定方式では、Edit/Play切り替え時に authored size を復元する必要がある。

## 7. Editor Requirements

### 7.1 Create Menu

Hierarchy or menu に以下を追加する。

```text
Create > UI > HP Gauge
```

生成されるentity:

```text
HPGauge
  Background
  Fill
  DamagePreview
  ValueText
  LabelText optional
```

初期値:

- Root anchor: left top or bottom left selectable
- Background size: 360 x 24
- Fill size: 360 x 24
- Text size: 360 x 32
- Fill texture: `Data/Texture/UI/White.png`
- Background tint: dark translucent
- Fill tint: green
- Text font: `Data/Font/ArialUni.ttf`

### 7.2 Inspector

`HPGaugeBindingComponent`:

- targetMode combo
- explicitTarget entity picker
- visibleWhenNoTarget checkbox
- hideWhenDead checkbox
- hideWhenFull checkbox
- smoothingSpeed drag float
- damagePreviewDelay drag float
- damagePreviewSpeed drag float
- runtime values readonly display

`HPGaugeFillComponent`:

- bindingRoot entity picker
- direction combo
- useDisplayedRatio checkbox
- minVisibleRatio drag float
- colorMode combo
- high/mid/low color editor
- mid/low threshold drag float

`HPGaugeTextComponent`:

- bindingRoot entity picker
- format combo
- label input
- hideWhenFull checkbox
- hideWhenNoTarget checkbox

### 7.3 SceneView Editing

SceneViewでは既存の2D UI編集と同じ操作でよい。

- move: `RectTransformComponent.anchoredPosition`
- resize: `RectTransformComponent.sizeDelta`
- pivot/anchor: Inspector
- order: `CanvasItemComponent.sortingLayer/orderInLayer`
- color: `SpriteComponent.tint` / `TextComponent.color`
- font: `TextComponent.fontAssetPath`

HPゲージ専用のSceneView操作はPhase 1では不要。

## 8. Save / Load / Prefab

新規componentは以下に登録する。

- `Generated/ComponentMeta.generated.h`
- `PrefabSystem.cpp` serialize / deserialize
- `EntitySnapshot`
- Inspector add component menu

HPゲージは prefab として保存できる必要がある。

推奨prefab:

```text
Data/Prefabs/UI/PlayerHPGauge.prefab
Data/Prefabs/UI/BossHPGauge.prefab
```

scene上にはprefab instanceとして配置し、targetModeだけ差し替えられるようにする。

## 9. Rendering Details

### 9.1 Fill Clip

LeftToRight:

- 表示幅 = authored width * ratio
- pivotが左でなくても、見た目の左端を固定する
- UVのU max = ratio

RightToLeft:

- 表示幅 = authored width * ratio
- 右端を固定
- UVのU min = 1 - ratio

BottomToTop:

- 表示高さ = authored height * ratio
- 下端を固定
- UVのV min/maxをratioで調整

TopToBottom:

- 表示高さ = authored height * ratio
- 上端を固定
- UVをratioで調整

### 9.2 Text Update

`HPGaugeTextComponent` は毎フレーム `TextComponent.text` を更新する。  
ただし値が変わったときだけ更新し、不要なstring allocationを避ける。

### 9.3 Visibility

Rootの表示状態は `HPGaugeBindingComponent` が決める。  
子の個別visibleはユーザー編集値として尊重したいので、root非表示時は子を破壊せず、描画収集側で親のactive/visibleを考慮するのが理想。

既存の `UI2DDrawSystem` は `HierarchyComponent.isActive` を見ている。親Canvasのvisible継承が必要なら、Phase 1で `Hierarchy` 親の `CanvasItemComponent.visible` をたどる処理を追加する。

## 10. Migration Plan

### Phase 1: ECS HP Gauge Minimum

- `HPGaugeBindingComponent`
- `HPGaugeFillComponent`
- `HPGaugeTextComponent`
- `HPGaugeSystem`
- Inspector表示
- Prefab/Scene保存
- `Create > UI > HP Gauge`
- `FirstPlayer` / `FirstEnemy` binding
- fill ratio反映
- text更新

この段階で `GameLayer` の直書きHPバーはまだ残してよい。  
新しいscene配置HPゲージが動くことを確認してから削除する。

### Phase 2: Proper Fill Rendering

- `UI2DSpritePacket` に fill clip 追加
- `UI2DSpriteRenderSystem` にUV crop追加
- `SpriteRenderer::DrawQuad` UV指定対応
- runtimeで `RectTransformComponent` を汚さない

### Phase 3: HUD Legacy Removal

- `GameLayer::InitializeHUD()` の player/boss hardcoded bar を削除
- `HUDBindingSystem` の役割を縮小または削除
- `UIProgressBar2D` / `UIHPText2D` の使い道を整理

### Phase 4: Advanced

- damage preview bar
- smooth decrease animation
- low HP pulse
- lock-on target HP
- boss phase segmented HP
- UI animation timeline
- world-to-screen HP gauge

## 11. Acceptance Criteria

Phase 1完了条件:

- SceneViewでHPゲージを作成できる
- 生成されたHPゲージが scene に保存される
- 再読み込み後も配置、サイズ、色、フォントが保持される
- `FirstPlayer` bindingでplayerのHPが表示される
- `FirstEnemy` bindingでenemy/bossのHPが表示される
- damageを受けるとfillと数字が更新される
- GameView上でsortingLayer/orderInLayerが反映される
- `GameLayer` に新しいHPゲージのレイアウト直書きが増えない

Phase 2完了条件:

- HP減少時に authored `RectTransformComponent.sizeDelta` が変化しない
- Fill方向4種が動く
- SpriteのUV cropが正しく反映される
- Scene保存時にruntime HP比率がレイアウトへ混入しない

## 12. Risks

### 12.1 RuntimeがRectTransformを書き換える問題

最も危険。  
実装を急ぐ場合でも、最終的には描画packet側のclipに寄せる。

### 12.2 Canvas visible の親子継承

rootを非表示にしても子が描画される可能性がある。  
`UI2DDrawSystem` 側で親階層の `HierarchyComponent.isActive` と `CanvasItemComponent.visible` を解決する必要がある。

### 12.3 Entity reference の保存

`explicitTarget` はscene保存後にEntityIDが変わる可能性がある。  
長期的には scene-local reference remap が必要。Phase 1では `FirstPlayer` / `FirstEnemy` を推奨する。

### 12.4 TextComponent 描画経路

Spriteは `UI2DSpriteExtractSystem` でRenderQueueへ行くが、`TextComponent` のGameView描画経路が不十分な場合、HP数字だけ出ない可能性がある。  
Phase 1開始時に `TextComponent` がGameView HUDPassで描画されているか確認する。

## 13. Recommended First Implementation Order

1. `HPGaugeBindingComponent` / `HPGaugeFillComponent` / `HPGaugeTextComponent` を追加
2. Prefab/Scene serialize対応
3. Inspector対応
4. `HPGaugeSystem` を作り、target解決とtext更新だけ先に通す
5. 暫定でfillの `RectTransformComponent.sizeDelta` を更新して動作確認
6. `Create > UI > HP Gauge` を追加
7. GameLayer hardcoded HPと並行表示で検証
8. `UI2DSpritePacket` fill clip対応へ置き換える
9. hardcoded HPを削除

最短で遊べる状態にするなら 1-6。  
エディタとして破綻しない状態にするなら 8 まで必須。
