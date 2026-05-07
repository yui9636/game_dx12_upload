# PostEffect DX12 Revival Spec

作成日: 2026-05-06

対象:

- `Source/PostEffect.h`
- `Source/PostEffect.cpp`
- `Source/Graphics.h`
- `Source/Graphics.cpp`
- `Source/FrameBuffer.h`
- `Source/FrameBuffer.cpp`
- `Source/RenderPass/PostProcessPass.h`
- `Source/RenderPass/PostProcessPass.cpp`
- `Source/Engine/EngineKernel.cpp`
- `External/FSR2/`（FSR2 SDK の DX12 バックエンドを追加）

関連シェーダ:

- `Data/Shader/FullScreenQuadVS.cso`
- `Data/Shader/LuminanceExtractionPS.cso`
- `Data/Shader/BloomPS.cso`

## 1. 目的

DX12 化作業の流れで `PostEffect` がパイプラインから外れたまま放置されており、Bloom / 露出 / モノクロ / Hue シフト / フラッシュ / ビネット / DoF / モーションブラー / FSR2 アップスケーリングといった一連のポストエフェクトが DX12 ビルドでは一切適用されない状態になっている。

原因はふたつある。第一に、`PostEffect` クラスが `ID3D11Device*` を直接受け取り、内部で `DX11Shader` / `DX11Buffer` / `DX11Texture` を握ったまま FSR2 の DX11 バックエンド (`ffx_fsr2_dx11.h`) を直接呼んでいるため、DX12 へポートしていない。第二に、同梱している FidelityFX FSR2 SDK が `External/FSR2/dx11/` と `External/FSR2/vk/` のみを抱えており、`External/FSR2/dx12/` を持っていないため、DX12 用バックエンドのソースコードがプロジェクト内に存在しない。

この仕様では、FSR2 SDK の DX12 バックエンドを取得して同梱したうえで、`PostEffect` を RHI 抽象化 (`ICommandList` / `IResourceFactory` / `ITexture` / `IPipelineState`) を介した DX11/DX12 両対応のクラスへ作り直し、`PostProcessPass` を RenderPipeline に正規登録することで、DX12 ビルドでもポストエフェクトと FSR2 アップスケールが効くゲーム前と同等の見た目に戻す。

## 2. 現状

### 2.1 できていること

- `Source/PostEffect.cpp` の `Process()` 本体は DX11 上で Luminance 抽出 → Bloom / カラーフィルター / DoF / モーションブラー Uber パス → FSR2 ディスパッチを実行できる仕様で残っている。コメントアウトされた古い実装ではなく、現役の `Process(const RenderContext&, ITexture*, ITexture*, ITexture*, ITexture*)` が DX11 では正しく機能する。
- `Source/PostEffect/` には `BloomExtractSystem` / `ColorFilterExtractSystem` / `DoFExtractSystem` / `MotionBlurExtractSystem` の ECS 抽出側が残っており、`Source/Layer/GameLayer.cpp:457-475` で `RenderContext` の各データに値が積まれている。Inspector / Component (`Source/Component/PostEffectComponent.h`) も生きている。
- `Source/RenderPass/PostProcessPass.{h,cpp}` は FrameGraph フレンドリーなラッパとして既に存在し、`SceneColor` / `GBufferDepth` / `GBuffer3` (Velocity) / `DisplayColor` を `builder.Read/Write` で繋いだうえで、`Graphics::Instance().GetPostEffect()->Process(...)` を呼ぶ構造になっている。
- `Source/RHI` 側は DX11 / DX12 両方の `ICommandList`、`IResourceFactory`、`ITexture`、`IPipelineState`、`IShader`、`IBuffer` 実装が揃っており、`Source/RenderPass/GTAOPass.cpp` や `Source/RenderPass/FinalBlitPass.cpp` のように `factory->CreatePipelineState(desc)` ＋ `commandList->Draw/Dispatch` の API で DX11/DX12 両対応に書ける土台が揃っている。
- `Source/RHI/DX12/DX12Texture.h` は `GetNativeResource()` で `ID3D12Resource*` を返せるため、FSR2 DX12 の `ffxGetResourceDX12()` に直接渡せる。
- `External/FSR2/CMakeLists.txt:25` には `option (FFX_FSR2_API_DX12 "Build FSR 2.0 DX12 backend" ON)` が入っており、SDK の構成上は DX12 を想定済み。`External/FSR2/shaders/` 配下の `ffx_fsr2_*_pass.hlsl` および事前コンパイル済みヘッダは DX11/DX12 共通で使える。

### 2.2 できていないこと（壊れている箇所）

- `Source/PostEffect.h:2-7` で `<d3d11.h>` と `dx11/ffx_fsr2_dx11.h` を直接 include し、コンストラクタ `PostEffect(ID3D11Device* device)` で DX11 デバイスを要求している。`Source/PostEffect.cpp:18-64` で `DX11Shader` / `DX11Buffer` / `ffxFsr2GetInterfaceDX11` / `ffxGetDeviceDX11` を直接呼んでおり、DX12 では構築すらできない。
- `Source/Graphics.cpp:177` で `postEffect = std::make_unique<PostEffect>(device.Get());` を DX11 ブランチでしか実行していない。DX12 ブランチ (`Source/Graphics.cpp:49-98`) では `postEffect` が `nullptr` のまま。結果、`PostProcessPass::Execute` (`Source/RenderPass/PostProcessPass.cpp:25`) の `if (!postEffect) return;` で常に早期 return している。
- `Source/Graphics.cpp:390-393` の `Graphics::CreatePipelineState` は API に関係なく `DX11PipelineState` を直接 new している。DX12 では `IResourceFactory::CreatePipelineState` 経由でしか正しい PSO を作れないため、`PostEffect` が旧来の `Graphics::Instance().CreatePipelineState(desc)` を呼び続ける限り DX12 で破綻する。
- `Source/Engine/EngineKernel.cpp:1083-1104` で `RenderPipeline::AddPass` を呼んでいる箇所に `PostProcessPass` が無い。DX12 ブランチでは `FinalBlitPass` と `HUDPass` だけが追加されており、`PostProcessPass` がそもそもパイプラインに登録されていない。DX11 ブランチ (`isDX12 == false`) では `FinalBlitPass` / `HUDPass` も `PostProcessPass` も登録されていないので、レガシーな描画経路で動いていることになる。
- `Source/FrameBuffer.cpp:86-108` の `GetColorMap` / `GetDepthMap` / `GetRenderTargetView` / `GetDepthStencilView` は `DX11Texture` への `dynamic_cast` を前提とした DX11 専用 API。`Source/PostEffect.cpp:78,96,261-281` の `FrameBuffer::SetRenderTarget(rc.commandList, nullptr)` 自体は `ICommandList` 経由なので問題ないが、PostEffect 内部で `static_cast<DX11Texture*>(tex)` して `GetNativeSRV/RTV/DSV->GetResource(...)` で `ID3D11Resource*` を抽出する箇所は DX12 では成立しない。
- `External/FSR2/dx12/` ディレクトリ自体が存在しない。DX12 では必須となる `ffx_fsr2_dx12.cpp` / `ffx_fsr2_dx12.h` / `shaders/ffx_fsr2_shaders_dx12.{cpp,h}` がリポジトリ内に無い。
- `Game.vcxproj` のビルドターゲットは現在 DX11 用 FSR2 ソースのみを含んでいる想定（`External/FSR2/dx11/*.cpp` / `External/FSR2/shaders/*` のみ）。DX12 ソースを追加した後でビルド対象とリンクライブラリを更新する必要がある。
- `RenderContext::jitterOffset` は FSR2 が必要とするが、DX12 経路で本当に毎フレーム更新されているかが未検証。Halton 列などの jitter 生成ロジックがどこにあるか（あるいは無いか）の確認が必要。

## 3. 復活目標

- DX12 ビルドで `PostProcessPass` が FrameGraph に組み込まれて毎フレーム実行され、Bloom / カラーフィルター / DoF / モーションブラー / FSR2 アップスケールが視覚的に効く。
- DX11 ビルドの挙動は壊さない。DX11 で従来通り `PostEffect::Process` が動き、見た目が DX12 化前と同じであること。
- `PostEffect` クラスが `ID3D11Device*` / `ID3D11DeviceContext*` / `DX11Shader` / `DX11Buffer` / `DX11Texture` への直接依存を持たない。RHI 抽象化のみを使う。
- FSR2 のバックエンド切り替えは API に応じて `ffx_fsr2_dx11.h` または `ffx_fsr2_dx12.h` を選び、共通の `FfxFsr2Context` を保持する形にする。
- `Graphics::CreatePipelineState` が API に応じて DX11/DX12 の PSO を返す。可能なら廃止し、各 Pass / Effect が `Graphics::GetResourceFactory()->CreatePipelineState(desc)` を直接呼ぶ。
- `Editor View` / `Game View` で別々の Luminance / PostProcess バッファ (`FrameBufferId::EditorLuminance` / `EditorPostProcess`) を使い分ける現行ロジックを維持する。

## 4. 設計方針

### 4.1 全体方針

- 「DX11 専用クラス」を「RHI 抽象を介して DX11/DX12 共通」に書き直す既存パターン（`GTAOPass` / `FinalBlitPass` / `DeferredLightingPass` 等）に揃える。新規パターンを発明しない。
- FSR2 はバックエンド差分を `PostEffect` 内の薄いブリッジ関数に閉じ込める。具体的には `InitFsr2()` / `DispatchFsr2()` の 2 関数で API 別実装を分岐し、それ以外のロジックは API 非依存とする。
- 既存の `PostProcessPass` インターフェースを変えない。Pass の Setup / Execute 仕様は今のままで、`PostEffect::Process` のシグネチャも `(const RenderContext&, ITexture* src, ITexture* dst, ITexture* depth, ITexture* velocity)` を維持する。

### 4.2 RHI 抽象化への置き換え方針

- コンストラクタを `PostEffect(IResourceFactory* factory, GraphicsAPI api, void* nativeDevice)` に変更する。`nativeDevice` は FSR2 SDK が要求する素のデバイスポインタ（DX11 では `ID3D11Device*`、DX12 では `ID3D12Device*`）。`api` を保持して FSR2 ディスパッチで分岐する。
- `fullscreenQuadVS` / `luminanceExtractionPS` / `uberPostPS` を `factory->CreateShader(ShaderType::Vertex|Pixel, "Data/Shader/...")` で作る。`std::unique_ptr<IShader>` のままでよい。
- `constantBuffer` を `factory->CreateBuffer(sizeof(CbPostEffect), BufferType::Constant)` で作る。`UpdateBuffer` は既に `ICommandList::UpdateBuffer` で抽象化済み。
- PSO は `factory->CreatePipelineState(desc)` で作る。`Graphics::Instance().CreatePipelineState(desc)` の呼び出し（`Source/PostEffect.cpp:39,42`）を全て `factory->CreatePipelineState(desc)` に置き換える。
- `LuminanceExtraction` / `UberPostProcess` 内の `FrameBuffer::SetRenderTarget(rc.commandList, nullptr)` は `ICommandList::SetRenderTarget(ITexture*, ITexture*)` を直接呼ぶ書き方に変える。FrameBuffer の DX11 限定 API には依存しない。
- バインド方式は `GTAOPass.cpp:100-111` のパターンに揃える。すなわち DX12 では `static_cast<DX12CommandList*>` して `BindPixelTextureTable` を使い、DX11 では従来通り `PSSetTextures` を呼ぶ。

### 4.3 FSR2 DX12 バックエンドの取り回し

- `External/FSR2/dx12/ffx_fsr2_dx12.h` を include し、`ffxFsr2GetScratchMemorySizeDX12()` / `ffxFsr2GetInterfaceDX12()` / `ffxGetDeviceDX12()` / `ffxGetResourceDX12()` を使う。API は DX11 版と一対一対応している。
- `FfxFsr2DispatchDescription::commandList` には DX11 では `ID3D11DeviceContext*`、DX12 では `ID3D12GraphicsCommandList*` を `(FfxCommandList)` キャストで渡す。`DX12CommandList::GetNativeCommandList()` (`Source/RHI/DX12/DX12CommandList.h:104`) を使って取り出す。
- リソース受け渡しは `ITexture* → ID3D12Resource*` を `static_cast<DX12Texture*>(tex)->GetNativeResource()` で取得する。SRV / RTV / DSV のハンドルを抜き出す必要は無く、`ID3D12Resource*` が直接渡せる。
- FSR2 出力の `dst` は `FFX_RESOURCE_STATE_UNORDERED_ACCESS` を要求する。DX12 経路では `rc.commandList->TransitionBarrier(dst, ResourceState::UnorderedAccess)` をディスパッチ前に必ず入れる。完了後は `ResourceState::ShaderResource` などディスプレイ向けにバリアを戻す。
- 入力 `src` / `depth` / `velocity` は `ResourceState::ShaderResource` 状態で渡す。GTAO / SSR / DeferredLighting と同じパターン。
- FSR2 の内部リソース命名は `L"FSR2_InputColor"` / `L"FSR2_InputDepth"` / `L"FSR2_InputVelocity"` / `L"FSR2_Output"` を維持する（PIX キャプチャ可読性のため）。

### 4.4 `Graphics` 側の手当て

- `Graphics::Initialize` の DX12 ブランチ末尾（`Source/Graphics.cpp:97` の直前）で `postEffect = std::make_unique<PostEffect>(resourceFactory.get(), api, m_dx12Device->GetDevice());` を呼ぶ。
- DX11 ブランチ側 `Source/Graphics.cpp:177` の呼び出しも、新しいシグネチャに合わせて `postEffect = std::make_unique<PostEffect>(resourceFactory.get(), GraphicsAPI::DX11, device.Get());` に書き換える。
- `Source/Graphics.cpp:390-393` の `Graphics::CreatePipelineState` は中身を `return resourceFactory->CreatePipelineState(desc);` にする（あるいは廃止）。`Source/PostEffect.cpp` 以外で `Graphics::Instance().CreatePipelineState(desc)` を直接呼んでいる箇所が他にあれば棚卸しする。
- `Graphics.h:14` の `#include "PostEffect.h"` 経由で d3d11.h が二次的に引かれている。新しい `PostEffect.h` は `<d3d11.h>` を public include しない（実装ファイル内に閉じ込める）ようにし、`Graphics.h` への汚染を断つ。

### 4.5 `EngineKernel` 側のパス登録

- `Source/Engine/EngineKernel.cpp:1100-1104` の DX12 ブロックを次の順で再構築する。
  1. `EffectMeshPass` / `EffectParticlePass` の後に `PostProcessPass` を追加。
  2. その後に `FinalBlitPass`、`HUDPass` を追加。
- `PostProcessPass::Setup` は `SceneColor` を Read、`DisplayColor` を Write として宣言しているため、`FinalBlitPass`（同じく `SceneColor` Read / `DisplayColor` Write）との順序関係を `EngineKernel` の登録順で表現する必要がある。FrameGraph 上は同一リソースに対する Write の前後関係でディペンデンシが解決されるはずだが、もし `FinalBlitPass` を残すなら「PostProcessPass で `DisplayColor` を最終形に書き込み、FinalBlitPass はバックバッファへのコピー専用に専念する」よう責務を見直す（4.7 で詳述）。
- DX11 ブランチでは旧来パスで動いているため、`PostProcessPass` の DX11 経路追加は別タスクとして切り分けてよい。本仕様では DX12 を最優先する。

### 4.6 `FrameBuffer` の扱い

- `PostEffect` の中で `FrameBuffer::SetRenderTarget(...)` を呼んでいる箇所は `commandList->SetRenderTarget(workTexture, nullptr)` に置き換える。`FrameBuffer*` ではなく `ITexture*` を受ける形に内部関数 `LuminanceExtraction` / `UberPostProcess` のシグネチャを変更する。
- FrameBuffer 自体の DX12 対応は既に `Source/FrameBuffer.cpp:24-49` の `IResourceFactory*` コンストラクタ経由で済んでおり、DX12 経路で `FrameBufferId::Luminance` / `EditorLuminance` / `PostProcess` / `EditorPostProcess` は `Source/Graphics.cpp:79-84` で生成済み。`PostEffect` からは `Graphics::GetFrameBuffer(...)->GetColorTexture(0)` 経由で `ITexture*` を受け取れば良い。

### 4.7 `FinalBlitPass` との責務整理

- 現行の `FinalBlitPass` は `SceneColor` を `DisplayColor` に解像度合わせて貼るだけのフルスクリーンブリットになっている。FSR2 でレンダースケール (0.67) からディスプレイ解像度へアップスケールするのは `PostEffect` の役目になるため、両者を併存させると `DisplayColor` への二重書きや解像度ミスマッチが発生する。
- 対応方針は次のいずれか。本仕様では (a) を採用する：
  - (a) `PostEffect::Process` の出力先を直接 `DisplayColor` (= `FrameBufferId::Display` の `ColorTexture(0)`) にする。`FinalBlitPass` は DX12 でも DX11 でも `PostProcessPass` 実行時には登録しない（あるいは `PostProcessPass` が enable のときは実体スキップする）。
  - (b) `PostEffect::Process` の出力を `PostProcess` バッファに書き、`FinalBlitPass` が `PostProcess → DisplayColor` を担当する。コードは現状に近いが、FSR2 を `PostProcess` バッファに dispatch する形になり、ディスプレイ解像度サイズの中間バッファが新たに必要。
- (a) を採用する場合、`PostProcessPass.cpp:34` の `dstTex` フォールバック（`FrameBufferId::Display` の color 0）はそのまま使える。`FinalBlitPass` を登録から外す代わりに、`HUDPass` の前に `PostProcessPass` を入れるだけで完結する。

### 4.8 Editor View / Game View 切替の維持

- `Source/PostEffect.cpp:223-226` の `editorDisplay && dst == editorDisplay->GetColorTexture(0)` 判定は、Editor View 上でも個別の Luminance / PostProcess を使うために必要。新しい実装でも同じ条件分岐を維持する。
- `EditorLuminance` と `EditorPostProcess` の FrameBuffer は DX12 ブランチでも生成済み（`Source/Graphics.cpp:80,84`）なので、追加実装は不要。

## 5. FSR2 DX12 SDK の取得手順

GPUOpen 公式の FidelityFX-FSR2 リポジトリ（FSR 2.2 系の `dx12/` バックエンド付きリリース）から、現状の `External/FSR2/` と同じバージョンに対応する DX12 ファイル一式を取得する。リポジトリ内の `External/FSR2/ffx_fsr2.cpp` と `ffx_fsr2_interface.h` のヘッダ・コメント・APIシグネチャからバージョンを特定し、同一マイナーバージョンのリリースから差分のみを切り出す。

### 5.1 取得対象ファイル（最小セット）

- `dx12/CMakeLists.txt`
- `dx12/ffx_fsr2_dx12.h`
- `dx12/ffx_fsr2_dx12.cpp`
- `dx12/shaders/CMakeLists.txt`（あれば）
- `dx12/shaders/ffx_fsr2_shaders_dx12.h`
- `dx12/shaders/ffx_fsr2_shaders_dx12.cpp`
- `dx12/shaders/ffx_fsr2_*_pass_*.h`（DXIL 事前コンパイル済みヘッダ群一式）

### 5.2 配置先

- 取得した一式を `External/FSR2/dx12/` 以下にそのままコピーする。`External/FSR2/dx11/` と対称な構造になる想定。
- `External/FSR2/shaders/` は DX11 / DX12 共有のため変更不要。ただし取得したリリースとシェーダのバイナリがハッシュレベルで一致するか念のため確認する。

### 5.3 ビルド構成への組み込み

- `Game.vcxproj` の `<ItemGroup>` に `External/FSR2/dx12/ffx_fsr2_dx12.cpp` および `External/FSR2/dx12/shaders/ffx_fsr2_shaders_dx12.cpp` を `<ClCompile>` で追加する。`<ClInclude>` 側にもヘッダを追加する。
- インクルードパスとして `External/FSR2` は既に通っている前提（DX11 版が動くため）。追加で `External/FSR2/dx12` を通す必要があるかは include 文 `#include <dx12/ffx_fsr2_dx12.h>` の書き方に揃えれば不要。
- 追加で必要となる Windows SDK のリンクライブラリは無し。`d3d12.lib` / `dxgi.lib` は既に DX12 経路でリンク済み。

### 5.4 Lisence / 第三者コードの扱い

- FSR2 SDK は MIT。`External/FSR2/dx12/` 以下のソース冒頭の AMD コピーライト・コメントは削らずそのまま残す。`Docs/` あるいはリポジトリ ROOT の `THIRD_PARTY_LICENSES.md` 等に追記する運用があれば、DX12 backend を追加した旨を一行追記する。

## 6. 実装計画

実装は依存関係に従い段階的に進める。各段階ごとにビルドが通る粒度で区切る。

### 6.1 Phase 1: FSR2 DX12 バックエンドの取り込み

- 5.1 の手順で `External/FSR2/dx12/` を配置する。
- `Game.vcxproj` / `Game.vcxproj.filters` に DX12 ソースを追加。
- 動作確認: `#include <dx12/ffx_fsr2_dx12.h>` する空のスタブ .cpp を一時的に追加してビルドが通ることを確認したのち削除する。または `PostEffect.cpp` の include 追加だけで実コードは未使用のままビルドが通ることを確認する。

### 6.2 Phase 2: `PostEffect` の RHI 抽象化（DX11 動作維持）

- `PostEffect.h` の include を `<d3d11.h>` / `<dx11/ffx_fsr2_dx11.h>` から `<ffx_fsr2.h>` のみに減らす。前方宣言で `IShader` / `IBuffer` / `IResourceFactory` / `ICommandList` / `ITexture` / `IPipelineState` を扱う。
- コンストラクタを `PostEffect(IResourceFactory*, GraphicsAPI, void* nativeDevice)` に変更。実装ファイル側で `api == GraphicsAPI::DX11` のときのみ DX11 FSR2 を初期化し、DX12 の場合は `m_fsr2Initialized = false` のまま素通りする。
- `LuminanceExtraction` / `UberPostProcess` が `FrameBuffer*` ではなく `ITexture*` を直接受け取るように変更。`commandList->SetRenderTarget(targetTex, nullptr)` 経由で書く。
- `Graphics::Initialize` の DX11 ブランチで新しいコンストラクタを呼ぶ。この時点で DX11 ビルドが従来通り動くこと、PostEffect のビジュアルが回帰していないことを確認する。

### 6.3 Phase 3: FSR2 ディスパッチの DX12 分岐実装

- `PostEffect.cpp` 実装に `#include <dx12/ffx_fsr2_dx12.h>` を追加。`InitFsr2DX12(ID3D12Device*)` / `DispatchFsr2DX12(...)` のヘルパ関数を実装。DX12 経路でも `m_fsr2Context` / `m_fsr2Interface` は同じ struct を流用する（FSR2 SDK が API 共通の型を使っているため）。
- `Process` 関数末尾の FSR2 ディスパッチを `if (api == GraphicsAPI::DX11) { ... DX11 path ... } else { ... DX12 path ... }` で分岐させる。リソース取得は DX12 では `static_cast<DX12Texture*>(tex)->GetNativeResource()`。`commandList` は DX12 では `static_cast<DX12CommandList*>(rc.commandList)->GetNativeCommandList()`。

### 6.4 Phase 4: PSO / バインドの DX12 化

- `LuminanceExtraction` / `UberPostProcess` のテクスチャバインドを GTAOPass のパターンに合わせる。
  - DX12 経路: `BindPixelTextureTable` で `t0..t3` を一括バインド。
  - DX11 経路: `PSSetTextures` 維持。
- `Graphics::CreatePipelineState` の依存を切り、`factory->CreatePipelineState` のみを使う形に統一する。
- DX12 経路で `commandList->TransitionBarrier(...)` を入れる。具体的には:
  - `LuminanceExtraction` 開始前: `src` を `ShaderResource`、`luminance` を `RenderTarget`。
  - `UberPostProcess` 開始前: `src` / `luminance` / `depth` / `velocity` を `ShaderResource`、`workFB->GetColorTexture(0)` を `RenderTarget`。
  - FSR2 dispatch 直前: `workFB->GetColorTexture(0)` を `ShaderResource` あるいは `Common`、`dst` を `UnorderedAccess`、`depth` / `velocity` を `ShaderResource`。
  - FSR2 dispatch 直後: `dst` を `ShaderResource` に戻す。

### 6.5 Phase 5: `PostProcessPass` の登録

- `Source/Engine/EngineKernel.cpp:1100-1104` の DX12 ブロックを次のように変更する。
  ```cpp
  if (isDX12) {
      SpriteRenderer::Instance().Initialize(factory);
      m_renderPipeline->AddPass(std::make_shared<PostProcessPass>());
      // FinalBlitPass は責務整理に従って外す（4.7 (a) 案）
      m_renderPipeline->AddPass(std::make_shared<HUDPass>(factory));
  }
  ```
- `RenderPipeline.cpp:75-80` のクローン関数 (`if (dynamic_cast<PostProcessPass*>...)`) は既に存在しているため変更不要。

### 6.6 Phase 6: 動作検証とチューニング

- 8 章の「動作検証」項目を順に確認する。
- ジッター値の妥当性（FSR2 の収束に直結）を Halton(2,3) などで確認する。`RenderContext::jitterOffset` の更新ロジックがどこにあるかを確認し、未実装ならカメラ更新と一緒に毎フレーム更新する処理を追加する（本仕様の対象外だが、未実装の場合は 9.2 のリスクとして扱う）。
- DRED / Debug Layer のエラーログ (`dx12_dred.log` / `dx12_device_lost.log`) に新規エラーが出ていないことを確認する。

## 7. 受入条件

- DX12 ビルドで `PostProcessPass` が毎フレーム実行され、`FrameBufferId::Display` あるいはバックバッファに Bloom と FSR2 アップスケールが反映された絵が出る。
- Inspector / `PostEffectComponent` の各種パラメータ（Bloom 強度、Exposure、HueShift、Vignette、DoF Focus、MotionBlur Intensity）を変更すると即座に絵に反映される。
- DX11 ビルドで PostEffect の見た目が DX12 化前と同じ。回帰なし。
- `Graphics::Instance().GetPostEffect()` が DX12 でも非 nullptr。
- `External/FSR2/dx12/` がリポジトリに含まれ、`ffx_fsr2_dx12.cpp` がビルド対象に入っている。
- `dx12_device_lost.log` / `dx12_dred.log` に PostEffect 起因の新規 Page Fault / Hang が記録されない。
- Editor View 上でも Game View と同等のポストエフェクトが効き、`EditorLuminance` / `EditorPostProcess` が個別に使われる。

## 8. 動作検証

### 8.1 機能検証

- Bloom: 強度 0 / 0.5 / 1.0 / 5.0 でハイライトの広がり方が連続的に変化する。
- ColorFilter: monoBlend を 1.0 にするとモノクロ化、hueShift で色相が動く、flashAmount で全画面フラッシュ、vignetteAmount で四隅が暗くなる。
- DoF: focusDistance / focusRange / bokehRadius を変えると非フォーカス域がボケる。`enableDoF=false` で完全 OFF。
- MotionBlur: カメラ回転中に enableMotionBlur=true で残像が出る、false で残像が消える。
- FSR2: Render Scale 0.67 でジャギなく出力される。`enableSharpening=true` のシャープ感が確認できる。

### 8.2 パフォーマンス検証

- DX12 で PostProcessPass の GPU 時間（PIX / RenderDoc）が DX11 と同等 ±10% に収まる。
- フレーム全体の GPU 時間が PostEffect 復活前と比べてフルパイプラインで妥当な増分（参考値: 1080p で 1〜2 ms 程度）に収まる。

### 8.3 安定性検証

- 5 分間カメラを動かし続けて DRED ログにエラーが出ないこと。
- ウィンドウリサイズを 5 回連続で行い、`Graphics::OnResize` 経由で FrameBuffer が再生成された後でも PostEffect が正しく動作すること。FSR2 コンテキストの再生成が必要な場合（display size が変わるため）は `PostEffect::OnResize(uint32_t, uint32_t)` を追加する。

## 9. 移行リスクと対応

### 9.1 FSR2 SDK バージョン不整合

- 既存 `External/FSR2/ffx_fsr2.cpp` のバージョンと、外部から取得する DX12 バックエンドのバージョンが食い違うと、`FfxFsr2Context` のレイアウトが合わず実行時クラッシュする可能性がある。
- 対応: 取得時にコミットハッシュ / リリースタグを README または取得元 URL コメントとしてリポジトリに残す。可能なら DX11 / 共通シェーダ部も同一リリースのものに揃える（差分が最小ならスキップ可）。

### 9.2 `RenderContext::jitterOffset` の更新有無

- FSR2 はカメラジッターを毎フレーム入れる前提で TAA 風の収束を行う。jitterOffset が常に 0 だと残像/にじみが出る。
- 対応: `RenderContext` の jitter 更新位置を確認し、未実装なら Halton(2,3) 16 サンプルでカメラ Projection 行列にも同 jitter を適用する。

### 9.3 リソース状態のミス

- DX12 では UAV へのバリアを忘れると Page Fault → Device Removed の典型例になる。
- 対応: 6.4 のバリア要件をチェックリスト化して PR レビュー観点に残す。`dx12_dred.log` / `dx12_device_lost.log` を CI ではなく手動で毎ビルド確認する手順を運用に組み込む。

### 9.4 `Graphics::CreatePipelineState` 全置換の影響範囲

- 直接呼び出している箇所が他のパスにあると、置き換え時に DX11 経路でも DX12 経路でも回帰が起きうる。
- 対応: 置換前に `grep -rn "Graphics::Instance().CreatePipelineState"` で網羅し、各呼び出し箇所が `factory->CreatePipelineState` に等価で置き換えできることを確認する。

### 9.5 `FinalBlitPass` を外したことによる DX11 ビルドへの影響

- 4.7 (a) 案では DX11 経路の `FinalBlitPass` 登録は元々無いため影響なし。ただし将来 DX11 経路にも `PostProcessPass` を追加する場合は同様に出力先を `DisplayColor` に統一する。

## 10. スコープ外

- 新しいポストエフェクト（例: AutoExposure、TAA、SSAO 切替）の追加。本仕様は「DX12 化作業の流れでつぶれた既存 PostEffect の復活」に限る。
- DX11 経路への `PostProcessPass` 追加。本仕様は DX12 復活を最優先とし、DX11 は既存の動作維持のみ。
- FSR2 から FSR3 や DLSS への置換。本仕様は FSR2 同等機能の DX12 移植のみ。
- シェーダ (HLSL) の書き換え。`LuminanceExtractionPS.hlsl` / `BloomPS.hlsl` は API 非依存のフルスクリーン PS であり、修正不要。

## 11. 参考

- 既存の DX12 移植リファレンス Pass: `Source/RenderPass/GTAOPass.cpp`、`Source/RenderPass/FinalBlitPass.cpp`、`Source/RenderPass/DeferredLightingPass.cpp`
- RHI 抽象化エントリポイント: `Source/RHI/IResourceFactory.h`、`Source/RHI/ICommandList.h`、`Source/RHI/PipelineStateDesc.h`
- DX12 デバイス / コマンドリスト native 取り出し: `Source/RHI/DX12/DX12Device.h:15`、`Source/RHI/DX12/DX12Texture.h:38`、`Source/RHI/DX12/DX12CommandList.h:104`
- 旧 DX11 PostEffect 実装（移植元のロジック確認用）: `Source/PostEffect.cpp:202-314`
- FSR2 DX11 バックエンド API（DX12 版と一対一対応）: `External/FSR2/dx11/ffx_fsr2_dx11.h:33-95`
