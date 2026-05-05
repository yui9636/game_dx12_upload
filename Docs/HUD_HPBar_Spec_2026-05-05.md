# HUD HP Bar / Damage Number Spec v3.0 (Sprite/SpriteRenderer split, DX12 only)

作成日: 2026-05-05 (v3 改訂: Sprite/SpriteRenderer 分離 / RT 明文化 / 座標系明文化 / シェーダ立上げ計画 / DX11 互換削除)
関連:
- `Docs/BossBattle_Spec_2026-05-03.md`
- `Docs/Sprite_2DUI_Improvement_Spec_2026-05-02.md`

## 0. 改訂履歴

- **v1** (廃案): ECS `SpriteComponent` + ImGui DrawList で HUD を描く案
- **v2** (改訂): 自前 Sprite クラスを DX12 に移植する方針へ転換
- **v3** (本版): レビュー指摘 4 点を反映
  - **#1** Sprite (データ) と SpriteRenderer (描画基盤) を分離
  - **#2** HUD の Render Target を `DisplayColor` (LDR / FinalBlitPass 後) に確定
  - **#3** Sprite 描画 API の座標系を screen-pixel 起点で明文化
  - **#4** シェーダ再利用は楽観しない。立上げフェーズを切る
  - **#5** DX11 互換は持たない。古い DX11 経路 (`HeadUpDisplay` 等) は移植する

## 1. 現状確認

### 1.1 (A) 自前 Sprite クラス系統 — DX11 hardcode で死蔵

| ファイル | 状態 |
|---|---|
| `Source/Sprite/Sprite.{h,cpp}` | コンストラクタが `ID3D11Device*` を要求。DX12 で構築不能 |
| `Source/UI/UIScreen.cpp` / `UIWorld.cpp` | `commandList->GetNativeContext()` で `ID3D11DeviceContext*` を取り出す → DX12 で死ぬ |
| `UIProgressBar2D` / `UIProgressBar3D` / `UIHPNumber` / `UIDamagePopup` | 上 2 つに依存 |
| `UIManager` / `DamageTextManager` | `Initialize/Update/Render` を呼ぶ箇所が 0 |
| `Source/HeadUpDisplay.cpp` | `if (api != DX12)` で生成スキップ済 |

### 1.2 (B) ECS `SpriteComponent` 系統 — エディタ限定で動作

`UI2DDrawSystem::CollectDrawEntries` + `EditorLayerSceneView::Draw2DOverlayForRect` 経由で **エディタ SceneView と GameView 2D mode のみ** ImGui DrawList で描画される。本仕様では **触らない**。

### 1.3 RenderPipeline の現状 (HUD 挿入位置の特定根拠)

[`EngineKernel.cpp:2847-2866`](../Source/Engine/EngineKernel.cpp:2847) に登録される pass 順:

```
ExtractVisibleInstancesPass
BuildInstanceBufferPass
BuildIndirectCommandPass
ComputeCullingPass             (DX12 のみ)
ShadowPass
GBufferPass
GTAOPass / SSGIPass / VolumetricFogPass / SSRPass
DeferredLightingPass        ← ここまでが HDR SceneColor 生成
SkyboxPass
ForwardTransparentPass
EffectMeshPass
EffectParticlePass
FinalBlitPass               (DX12 のみ。SceneColor[HDR] → DisplayColor[LDR])
```

`PostProcessPass` クラスは存在するが **登録されていない** ([`PostProcessPass.cpp`](../Source/RenderPass/PostProcessPass.cpp))。実質 `FinalBlitPass` の `FinalBlitPS.cso` が tonemap も担っている。

→ **HUD 描画は FinalBlitPass の後に DisplayColor へ書く**。理由は §3.4。

## 2. 範囲

### 2.1 やること

- `Sprite` クラスを **データ専用**に書き換える (テクスチャ参照、glow パラメータ、UV 範囲などの保持)
- 新規 `SpriteRenderer` クラスを **描画基盤**として作る (PSO、ルートシグネチャ、頂点バッファ、シェーダリソースを所有)
- `SpriteRenderer::Begin` / `Draw(sprite, ...)` / `End` 形式で 1 フレームのスプライト描画をバッチ管理
- 新規 `HUDPass : IRenderPass` を追加。`UIManager::Render` と `DamageTextManager::Render` を呼び出す
- HUD は **`DisplayColor` (LDR) に書く**。`FinalBlitPass` の **後** にパスを差し込む
- `UIScreen` / `UIWorld` / `UIProgressBar2D` / `UIProgressBar3D` / `UIHPNumber` / `UIDamagePopup` を `Sprite + SpriteRenderer` ベースに書き直す
- `UIManager::Initialize/Update/Render` をエンジンループに組込む
- `DamageTextManager` 同様
- `HUDBindingSystem` のスナップショットを HUD 要素に流すドライバを `GameLayer` で接続
- `HeadUpDisplay` (DX11 lockon) を新パスに移植

### 2.2 やらないこと

- (B) ECS `SpriteComponent` / `UI2DDrawSystem` / `EditorLayerSceneView::Draw2DOverlayForRect` の改修
- アニメーション付き演出 (HP バー減少のスムージング等は v3.1 以降)
- フォントエンジン刷新 (`FontManager` 流用)
- `Sprite3D` (ワールド空間ビルボード) の DX12 直接描画。Phase F で 3D→2D 投影で代替
- Sprite バッチング最適化 (1 ドロー = 1 スプライト。最適化は v4)

### 2.3 既存の何を変更しないか

- `RenderPipeline` のパス順 (HUDPass を末尾に追加するのみ)
- `IRenderPass` / `IResourceFactory` / `ICommandList` 抽象
- `HealthComponent` / `HUDLinkComponent` / `HUDBindingSystem` のデータ構造
- `SpriteComponent` (ECS) を **一切触らない**
- `RenderQueue` / `RenderContext` のメンバ

### 2.4 削除する

- `Sprite` クラスの **DX11 専用コンストラクタ / Render API**
  - `Sprite(ID3D11Device*)` / `Sprite(ID3D11Device*, const char*)` / `Render(ID3D11DeviceContext*, ...)` を削除
- `UIScreen / UIWorld / UI*` 内の `commandList->GetNativeContext()` 経由の DX11 直叩き
- `HeadUpDisplay::HeadUpDisplay` の `if (api != DX12)` で DX11 Sprite を作るブランチ → 新 Sprite を常時生成する形へ

`Graphics::GetAPI()` が DX11 を返した場合は HUD 描画を no-op にする (旧 DX11 ビルドを残す必要が無いため、DX11 経路は HUD 機能を提供しない)。

## 3. 設計

### 3.1 全体図

```
[HealthComponent]
    ↓
[HUDBindingSystem::Update]   (snapshot)
    ↓
[GameLayer::ApplyHUDState]   (静的 UIElement への SetProgress / SetHP)
    ↓
[UIManager::Update(dt)]      (UIDamagePopup の生存タイマ / 移動)
    ↓
[FinalBlitPass]              (SceneColor[HDR] → DisplayColor[LDR])
    ↓
[HUDPass::Execute]           ★新規
    └ SpriteRenderer::Begin(commandList, viewport)
       UIManager::Render(rc)
         └ UIElement::Render(rc)
            └ SpriteRenderer::Draw(sprite, dx, dy, dw, dh, ...)
       DamageTextManager::Render(rc)
         └ UIDamagePopup::Render(rc)
            └ SpriteRenderer::Draw(...)
       SpriteRenderer::End()
    ↓
[Swap chain present]
```

### 3.2 `Sprite` (データ) と `SpriteRenderer` (描画基盤) の責務分離 ── レビュー指摘 #1

#### Sprite (データのみ)

```cpp
// Source/Sprite/Sprite.h
class Sprite
{
public:
    Sprite() = default;
    explicit Sprite(const std::string& texturePath);

    // テクスチャは ResourceManager で共有される ITexture を保持
    void SetTexture(std::shared_ptr<ITexture> texture);
    ITexture* GetTexture() const { return m_texture.get(); }

    int GetTextureWidth() const  { return m_textureWidth; }
    int GetTextureHeight() const { return m_textureHeight; }

    // 装飾パラメータ
    void SetColor(const DirectX::XMFLOAT4& c) { m_color = c; }
    void SetGlow(const DirectX::XMFLOAT3& color, float intensity) {
        m_glowColor = color;
        m_glowIntensity = intensity;
    }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }
    const DirectX::XMFLOAT3& GetGlowColor() const { return m_glowColor; }
    float GetGlowIntensity() const { return m_glowIntensity; }

private:
    std::shared_ptr<ITexture> m_texture;
    int m_textureWidth  = 0;
    int m_textureHeight = 0;
    DirectX::XMFLOAT4 m_color = { 1, 1, 1, 1 };
    DirectX::XMFLOAT3 m_glowColor = { 0, 0, 0 };
    float m_glowIntensity = 0.0f;
};
```

ポイント:
- **PSO・頂点バッファ・シェーダを一切持たない**
- 寿命は UI 要素ごと。`UIScreen::SetSprite(std::shared_ptr<Sprite>)` で渡される

#### SpriteRenderer (描画基盤、シングルトン)

```cpp
// Source/Sprite/SpriteRenderer.h
class SpriteRenderer
{
public:
    static SpriteRenderer& Instance();

    // 起動時に 1 度だけ呼ぶ。PSO/RootSig/VertexBuffer/Shader を作成
    void Initialize(IResourceFactory* factory);
    void Finalize();

    // フレームの HUD 描画開始/終了。HUDPass::Execute から呼ぶ
    // viewportPx は描画先 RT のピクセル単位サイズ (DisplayColor のサイズ)
    void Begin(ICommandList* commandList, const DirectX::XMFLOAT2& viewportPx);
    void End();

    // 矩形 1 枚を描画。詳細な座標規約は §3.3 参照
    void Draw(const Sprite& sprite,
              float dx, float dy,                  // screen pixel, top-left origin
              float dw, float dh,                  // screen pixel size
              float sx, float sy, float sw, float sh,   // texture pixel
              float angleRad,
              const DirectX::XMFLOAT4& tintColor); // 乗算用 tint (sprite 自身の color と別軸)

    // sub-rect なしバージョン (テクスチャ全域)
    void Draw(const Sprite& sprite,
              float dx, float dy, float dw, float dh,
              float angleRad,
              const DirectX::XMFLOAT4& tintColor);

private:
    SpriteRenderer() = default;

    std::shared_ptr<IShader>        m_vs;
    std::shared_ptr<IShader>        m_ps;
    std::shared_ptr<IPipelineState> m_pso;

    // 1 ドロー = 4 頂点 (TriangleStrip)。フレーム冒頭に upload heap を確保し、
    // Draw ごとに動的更新→DrawInstanced(4) を発行
    std::shared_ptr<IBuffer> m_vertexBuffer;     // capacity = 4 * kMaxSpritesPerFrame 頂点
    std::shared_ptr<IBuffer> m_constantBuffer;   // UIConstants 配列

    ICommandList* m_currentCommandList = nullptr;
    DirectX::XMFLOAT2 m_currentViewport{ 0, 0 };
    int m_drawIndex = 0;          // この Begin/End 期間中の通算ドロー数
};
```

ポイント:
- **1 つしか存在しない**。`UIElement` は描画の度に `SpriteRenderer::Instance().Draw(...)` を呼ぶだけ
- PSO/ルートシグネチャの所有はここ。Sprite には持たせない
- `Begin/End` 間で動的頂点バッファを使い回す。フレームを跨ぐ ring buffer は不要 (HUDPass は 1 フレームで完結)

### 3.3 描画 API の座標規約 ── レビュー指摘 #3

`SpriteRenderer::Draw` の引数は **すべてピクセル座標**。NDC 変換は SpriteRenderer 内で行う。

| 引数 | 単位 | 原点 | 範囲 |
|---|---|---|---|
| `dx, dy` | screen pixel | **画面左上 (0,0)** | `[0 .. viewportPx.x] / [0 .. viewportPx.y]` |
| `dw, dh` | screen pixel | (size) | 任意 |
| `sx, sy` | texture pixel | テクスチャ左上 (0,0) | `[0 .. texW] / [0 .. texH]` |
| `sw, sh` | texture pixel | (size) | 任意 |
| `angleRad` | radian | 描画 quad の中心 | 任意 |

NDC 変換式 (Begin で渡された viewportPx を使う):

```
ndc.x = (dx / viewportPx.x) * 2.0 - 1.0
ndc.y = 1.0 - (dy / viewportPx.y) * 2.0     // Y 反転
ndc.w = (dw / viewportPx.x) * 2.0
ndc.h = (dh / viewportPx.y) * 2.0
```

UV 変換式:

```
uv.x = sx / texW
uv.y = sy / texH
uv.w = sw / texW
uv.h = sh / texH
```

これは vertex shader 側ではなく **CPU 側** (Draw 関数内) で頂点に焼き込む。シェーダは標準的な `position : POSITION (NDC), texcoord : TEXCOORD` を受けるだけ。

`UIProgressBar2D` の進捗表現は `dw` に `originalDw * progress` を渡し、`sw` に `originalSw * progress` を渡せば左から塗り潰し。`UIHPNumber` も同じ API で文字数だけ Draw する。

### 3.4 HUDPass の Render Target 選択 ── レビュー指摘 #2

| 候補 RT | 描画タイミング | 影響 |
|---|---|---|
| **HDR SceneColor** | DeferredLightingPass〜EffectParticlePass の前後 | UI が tonemap / bloom の影響を受ける。色がギラついたり、白飛びしたり、bloom が滲む。**❌ 不可** |
| **LDR DisplayColor** (FinalBlitPass 後) | FinalBlit が tonemap+ガンマ済み LDR を書いた **後** | UI が素のピクセル色で乗る。tonemap や bloom が掛からない。**✅ 採用** |
| 専用 UI RT を持って FinalBlit で合成 | FinalBlit に UI ブレンド入力を追加 | Final シェーダ書き換え必要、デザイン変更大。v4 検討 |

決定: **DisplayColor 直書き**。HUDPass の `Setup` で:

```cpp
m_hDisplayColor = builder.GetHandle("DisplayColor");
if (m_hDisplayColor.IsValid()) {
    m_hDisplayColor = builder.Write(m_hDisplayColor);
    builder.RegisterHandle("DisplayColor", m_hDisplayColor);
}
```

`Execute` で:

```cpp
ITexture* dst = resources.GetTexture(m_hDisplayColor);
rc.commandList->TransitionBarrier(dst, ResourceState::RenderTarget);
rc.commandList->OMSetRenderTargets(/*color=*/{dst}, /*depth=*/nullptr);
rc.commandList->RSSetViewport({0,0,dst->GetWidth(),dst->GetHeight()});

SpriteRenderer::Instance().Begin(rc.commandList, {dst->GetWidth(), dst->GetHeight()});
UIManager::Instance().Render(rc);
DamageTextManager::Instance().Render(rc);
SpriteRenderer::Instance().End();
```

登録位置 ([EngineKernel.cpp:2865](../Source/Engine/EngineKernel.cpp:2865)):
```cpp
if (isDX12) {
    m_renderPipeline->AddPass(std::make_shared<FinalBlitPass>(factory));
+   m_renderPipeline->AddPass(std::make_shared<HUDPass>(factory));    // FinalBlit の後
}
```

**HDR で出したい光る HUD (将来要求)** は v4 で「専用 UI RT + FinalBlit 合成」案に進化させる。今回は LDR 直書きで割り切る。

### 3.5 シェーダ立上げ ── レビュー指摘 #4

既存の `Shader/SpriteVS.hlsl` / `SpriteUI_PS.hlsl` は SM 5.0 で .cso が出ているが、**そのまま `factory->CreateShader` で動く保証は無い**。理由:

1. **DX12 のルートシグネチャに合わせた binding が必要**。シェーダの `register(b0)` / `register(t0)` / `register(s0)` がエンジンの `GlobalRootSignature` (slot 7 等) と衝突しない / 適切に紐づくか確認が要る
2. 既存 Sprite シェーダは **DX11 のレジスタ流儀**で書かれている。DX12 ではルートシグネチャから明示的に slot を指定する必要があり、ローカルルートシグネチャ or グローバル拡張のいずれかが要る
3. `DescriptorHeap` 経由のテクスチャバインドが必要 ([ImGuiRenderer.cpp:217](../Source/ImGuiRenderer.cpp:217) の方式参照)

立上げ計画:

| 段階 | 作業 | 検証 |
|---|---|---|
| **a** | 既存 `SpriteVS.cso` / `SpriteUI_PS.cso` を `factory->CreateShader` に通す | ロード成功 (失敗ログを見る) |
| **b** | Sprite 専用のローカルルートシグネチャを書き起こす ([`DX12RootSignature`](../Source/RHI/DX12/DX12RootSignature.h) を参考)。slot 構成: t0=テクスチャ SRV / s0=Sampler / b0=UIConstants | PSO 作成成功 |
| **c** | テストドロー: 1x1 白テクスチャを画面左上 (10,10,100,100) に描画する unit-style テスト | 画面に白い四角が出る |
| **d** | 既存 `SpriteUI_PS.hlsl` の `register` 周りが (b) のルートシグネチャと整合するか確認。ズレていれば HLSL を修正し再コンパイル ([`Shader/*.hlsl` の FxCompile 設定](../Game.vcxproj))| テクスチャ・カラー・glow が正しく反映される |

(a)〜(d) は Phase A 内のチェックリストとして扱う。シェーダを **書き直さなくて済む可能性は高いが、楽観しない**。書き直しが要る場合は HLSL に `[RootSignature(...)]` 属性付きで再コンパイルする。

### 3.6 UI 要素の改修方針

各 `UIElement::Render(RenderContext& rc)` を以下のパターンに書き換える:

```cpp
// 旧 (DX11 直叩き):
ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
sprite->Render(dc, dx, dy, dz, dw, dh, sx, sy, sw, sh, angle, r, g, b, a);

// 新 (SpriteRenderer 経由):
SpriteRenderer::Instance().Draw(*sprite, dx, dy, dw, dh, sx, sy, sw, sh, angle, {r, g, b, a});
```

`UIScreen / UIWorld / UIProgressBar2D / UIProgressBar3D / UIHPNumber / UIDamagePopup` 6 ファイルが対象。

`UIWorld` (3D 配置) は `Sprite3D` の代替として:
- `Render(rc)` の冒頭でカメラのビュー projection でワールド位置をスクリーン座標に変換
- それを `dx, dy` に渡して 2D の `SpriteRenderer::Draw` を呼ぶ
- `Sprite3D` クラス自体は本仕様の対象外 (Phase F 完了時点で `UIWorld` 経由なら不要)

### 3.7 DX11 互換削除 ── レビュー指摘 #5

- `Sprite::Sprite(ID3D11Device*)` / `Sprite::Render(ID3D11DeviceContext*, ...)` を削除
- `Sprite` ヘッダから `<d3d11.h>` / `<wrl.h>` を消す
- `Sprite3D` も同様 (`Sprite3D` は本仕様で UIWorld 経由に置換するため、DX11 API は削除して構わない)
- `HeadUpDisplay::HeadUpDisplay()` の `if (api != DX12) ...` を削除し、新 `Sprite()` コンストラクタ + `SetTexture` で常時初期化
- `HeadUpDisplay::Render(ID3D11DeviceContext*)` のシグネチャを `Render(RenderContext&)` に変更し、`SpriteRenderer::Draw` 経由で描画

`Graphics::GetAPI() == DX11` 環境を残したい場合は、`SpriteRenderer::Initialize` を no-op にして HUD 描画をまるごと skip する。**DX11 ビルドでは HUD が出ない** ことを許容する。

## 4. 実装フェーズ

| Phase | 内容 | 受入条件 |
|---|---|---|
| **A. SpriteRenderer 立上げ** | 新規 `SpriteRenderer` クラス。シェーダロード、ローカルルートシグネチャ作成、PSO 作成、頂点/定数バッファ用意。3.5 のチェックリスト (a)〜(d) を完走 | テストドロー (1x1 白テクスチャ → 画面左上 100x100) で白い四角が画面に表示される |
| **B. Sprite クラス再設計** | DX11 API 削除。`SetTexture` / `GetTexture` / glow / color のデータ専用に。`Source/Sprite/Sprite.{h,cpp}` 書換 | ビルド成功 (DX11 経路の caller を §3.7 に従って migrate 済) |
| **C. HUDPass 新規** | `IRenderPass` 派生。`Setup` で `DisplayColor` を Write、`Execute` で `SpriteRenderer::Begin/End` の枠を作る。中身は空 | パイプラインに `HUDPass` が登録され、ビルド・実行が通る (HUD はまだ出ない) |
| **D. UIElement 書換 + UIManager 起動** | `UIScreen / UIWorld / UIProgressBar2D / UIProgressBar3D / UIHPNumber / UIDamagePopup` の Render を `SpriteRenderer::Draw` 経由に。`UIManager::Initialize/Update/Render` を `GameLayer` / `HUDPass` に組込 | UIManager に手で `UIProgressBar2D` を追加して `SetProgress(0.5)` するテストで、画面下に半分塗りつぶしバーが出る |
| **E. HUDBindingSystem ↔ HUD ブリッジ** | `GameLayer::Initialize` で `s_playerBar` / `s_bossBar` / `s_playerHPText` / `s_bossHPText` をシングルトン保持。`ApplyHUDState` でスナップショットを反映 | プレイヤー / ボスの HealthComponent 変動で画面 HP バーが追従 |
| **F. UIDamagePopup + DamageTextManager** | `Initialize/Update/Render` を engine ループに組込。`HealthSystem` の `Spawn` が実描画される | ヒットでダメージ数字が浮き 0.8s で消える |
| **G. UIProgressBar3D / UIHPNumber (頭上)** | カメラのビュー projection でスクリーン投影してから 2D Draw。HUDBindingSystem の `world[]` ループで動的 spawn / プール再利用 | enemy 頭上に HP ゲージが追従 |
| **H. HeadUpDisplay 移植 + (B) 非干渉確認** | `HeadUpDisplay` を新 Sprite + RenderContext 経由に書換。エディタ 2D mode preview / Inspector 編集が壊れていないことを手動確認 | DX12 で lockon カーソルが描かれる。既存 Sprite entity 編集の振る舞いが v2 と同じ |

各 Phase は独立してビルド・コミット・push する。Phase A の (c) で **「自前 Sprite 経路で 1 枚絵が画面に出る」** が milestone。

## 5. 受入条件 (全 Phase 完了時)

- [ ] DX12 ランタイムで自前 `Sprite + SpriteRenderer` 経由で画面に絵が出る
- [ ] HUDPass がパイプライン末尾 (FinalBlitPass の後) に登録され、`DisplayColor` に書く
- [ ] プレイヤー / ボスの HP バーが `HealthComponent` の値に同期
- [ ] enemy 頭上の HP ゲージが追従
- [ ] 攻撃ヒット時にダメージ数字が浮いて消える
- [ ] **ImGui DrawList を介さず**、エンジン自前の DX12 描画コマンドで HUD が出ている
- [ ] (B) ECS `SpriteComponent` 経路は v2 と同じ振る舞いを維持
- [ ] DX11 専用の Sprite API はソースから消えている
- [ ] `HeadUpDisplay` も DX12 で動く

## 6. リスク・留意

| 項目 | リスク | 対処 |
|---|---|---|
| ローカルルートシグネチャ設計 | エンジンの `GlobalRootSignature` の slot と衝突 | `DX12RootSignature` を直接生成して PSO に紐付ける ([FinalBlitPass.cpp:13-37](../Source/RenderPass/FinalBlitPass.cpp) 参照)。GlobalRootSignature には触らない |
| 動的頂点バッファの DX12 アップロード | DX12 では `Map/Unmap` のたびに upload heap を確保する必要 | エンジンの `DX12Buffer` が `BufferType::Upload` の dynamic update をサポートしているか確認。無ければ Phase A で `SpriteRenderer` 内に upload buffer を内製 |
| シェーダ binding 不整合 | `SpriteUI_PS.hlsl` の `tex : register(t0)` がローカルルートシグネチャと噛み合わないと SRV が見えない | Phase A の (b)〜(d) で必ず検証する |
| FrameGraph の依存関係 | `HUDPass` が `DisplayColor` を Write、`FinalBlitPass` も `DisplayColor` を Write 宣言済 → Read/Write 順序で Write→Write になり実行順が正しく解決されるか | FrameGraphBuilder が pass の登録順を尊重するか確認。されない場合は `HUDPass` で `Read("DisplayColor")` も併用して順序強制 |
| (B) との混在 | 両方が描く先は別 (HUDPass=DisplayColor, ECS Path=ImGui swap chain overlay) | レイヤとして衝突しない。ECS は最後に被さるが、ECS は editor のみで動作するため runtime ビルドでは HUDPass のみが出る |
| DX11 経路放棄 | DX11 で開発を続けたい人が居ると HUD が動かない | 仕様 §3.7 で明示。ビルド警告ではなくコンパイル可能を維持 (no-op) |

## 7. 採用しない案

- **(B) ECS パスを HUDPass に統合** — スコープ爆発。両系統並走
- **HDR HUD (Bloom 越しに光らせる)** — 現状の FinalBlit は tonemap を内包しており、HDR で出すと色が崩れる。LDR 直書きで割り切る
- **Sprite バッチング** — ドロー数が増えても問題ない量 (HUD <50 ドロー/フレーム)。最適化は v4
- **DX11 互換** — 削除する

## 8. Future (v4)

- 専用 UI RT + FinalBlit 合成 (HDR HUD の道を開く)
- Sprite バッチング (同テクスチャまとめ)
- `Sprite3D` 復活 / 自前 DX12 ビルボード描画
- HUD のフェード / 演出 (`UILifetimeComponent` 相当)
