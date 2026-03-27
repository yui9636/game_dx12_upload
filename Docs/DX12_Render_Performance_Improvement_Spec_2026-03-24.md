# DX12 描画速度改善仕様書

作成日: 2026-03-24
対象: `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload`
目的: DX12 描画を DX11 より遅い状態から脱却させ、最終的に DX11 以上の描画性能と拡張性を持つ構成へ移行する

---

## 1. 背景

現在のエンジンは DX12 をメイン描画経路として運用しているが、体感・実測ともに DX11 より重い場面が目立つ。

特に現在の構造では以下が起きやすい。

- `RenderPipeline::BeginFrame()` で 1 本の command list を開始し、そのまま全パスを直列実行している
- `FrameGraph::Execute()` も setup / compile / execute が単一スレッド前提で進む
- scene upload、batch 準備、instance buffer 構築、indirect command 構築が同一フレームの主スレッドに偏っている
- DX12 なのに command recording の並列性を活かせていない
- GPU 主導描画の土台はあるが、CPU 主導の準備コストがまだ大きい

結果として、DX12 の低レベル API のコストだけ背負い、並列化や非同期化の恩恵を十分に受けられていない。

---

## 2. 現在のボトルネック認識

### 2.1 CPU 側

#### 単一 command list 依存

`C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RenderContext\RenderPipeline.cpp`

- `BeginFrame()` で command list を 1 本だけ用意している
- その command list に全パスを順番に積んでいる
- pass ごとの command recording を並列化できない

#### FrameGraph の直列実行

`C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RenderGraph\FrameGraph.cpp`

- setup
- compile
- topological sort
- execute

が 1 本の流れで直列に進む。

#### 描画準備の主スレッド集中

以下が基本的に主スレッドに集中している。

- ECS からの描画抽出
- visible instance 抽出
- instance buffer 構築
- indirect command 構築
- scene constant upload

### 2.2 GPU 側

#### DX12 固有のオーバーヘッドに対して draw 削減が不足

- dynamic CB は導入済みだが、pass 数と state 変更数はまだ多い
- non-skinned instancing は入っているが、全面的ではない
- skinned mesh は依然として CPU 主導 draw の比率が高い

#### async compute 未活用

GTAO / SSGI / SSR / blur 系は、将来的に graphics queue と分離しやすい候補だが、現在は graphics queue 側で順番待ちしている前提が強い。

---

## 3. 改善目標

### 3.1 最低目標

- 同一シーンで DX11 と同等以上のフレームレート
- editor 実行時の CPU フレーム時間を明確に削減
- 複数 view / 将来の専用ツール追加時でも性能が落ちにくい構造

### 3.2 中期目標

- CPU recording コストを DX11 比で削減
- 大量 entity / 大量 mesh の draw 準備を batch / instance 前提にする
- DX12 の command recording を複数スレッドへ展開する

### 3.3 長期目標

- multi-view / multi-window 対応の FrameGraph
- compute culling + indirect draw による GPU driven 強化
- async compute による post effect / visibility / reduction 系の非同期化

---

## 4. 設計原則

### 4.1 DX11 非破壊

- DX11 の動作と見た目は壊さない
- 改善は DX12 実装、または共通 interface の追加で行う

### 4.2 CPU と GPU の責務分離

- CPU は「何を描くか」を整理する
- GPU は「どう大量に描くか」を担当する
- 1 draw ごとの細粒度更新を減らす

### 4.3 並列化は構造変更とセットで進める

- ただ worker thread を足すだけでは速くならない
- pass 境界、resource lifetime、command list ownership を先に整理する

### 4.4 マルチスレッド化は view 単位 / pass 単位で行う

- 1 本の巨大 command list を複数人で触らない
- 各 worker は独立 command list へ記録し、最後に main queue へ submit する

### 4.5 job system / task graph 前提

本仕様の並列化は、場当たり的な `std::async` や ad-hoc thread 追加では行わない。
前提として、固定スレッドプール上で動く job system / task graph を使う。

最低限必要な性質は以下。

- 固定 worker thread pool
- 軽量 job の enqueue / steal
- dependency を持つ task graph
- main thread からの dispatch / wait / continuation
- frame ごとの一時メモリ領域を job 単位で分離できること

job system は以下を処理対象にする。

- ECS 抽出
- visible instance 抽出
- instance flatten
- indirect command 構築
- pass command recording

依存解決は「task graph 上の dependency」で扱い、worker 同士が直接待ち合わせしない構造にする。

---

## 5. 期待効果

### 5.1 現実的な期待値

CPU ボトルネックが強い editor / multi-view シーンでは、次の改善幅が期待できる。

- 低リスク改善のみ: 1.1x 〜 1.3x
- batch / instancing / indirect の整理: 1.2x 〜 1.6x
- command recording のマルチスレッド化: 1.3x 〜 2.0x
- multi-view + worker recording + GPU driven 準備まで進んだ場合: 1.5x 〜 2.5x

ただし GPU ボトルネックが支配的なシーンでは、CPU 最適化だけでは伸び幅は小さい。

### 5.2 効果が出やすい条件

- pass 数が多い
- draw call 数が多い
- editor で複数ウィンドウを同時更新する
- サムネイルやプレビューなど補助描画も同時に走る
- ECS 抽出 / batch 構築が重い

---

## 6. 改善フェーズ

## Phase 0: 可視化と計測の標準化

### 目的

最適化前に、どこが遅いかを必ず見える化する。

### 実施内容

- CPU 側計測を pass 単位で統一
- 以下の時間を毎フレーム計測できるようにする
  - ECS 抽出
  - visible instance 抽出
  - instance buffer 構築
  - indirect command 構築
  - FrameGraph setup / compile / execute
  - 各 RenderPass 実行時間
- DX12 の command list recording 時間と submit 待ち時間を分離して見る
- GPU timing query は将来的に導入するが、まず CPU 側を正確に取る

### 完了条件

- 主要 pass の CPU 時間が毎フレーム確認できる
- DX11 と DX12 の差分が「感覚」ではなく数値で見える

---

## Phase 1: 低リスク高速化

### 目的

構造を大きく変えずに、DX12 の無駄なコストを減らす。

### 実施内容

- 不要な state 再設定を削減
- root signature / descriptor heap / PSO の再バインド回数を減らす
- 同一 pass 内の redundant barrier を削る
- upload buffer / descriptor allocator のフレーム再利用を徹底
- dynamic CB ring の使用状況を可視化し、spill を減らす
- texture / buffer 生成をフレーム中に行わない

### 対象箇所

- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RHI\DX12\DX12CommandList.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RHI\DX12\DX12Device.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RenderPass\*`

### 完了条件

- DX12 のフレーム時間が安定する
- descriptor / upload / barrier の無駄が明確に減る

---

## Phase 2: CPU 準備コストの削減

### 目的

描画準備の主スレッド集中を緩和する。

### 実施内容

- ECS 抽出を archetype 単位に固定
- visible instance 抽出を batch 単位で行う
- instance buffer 構築を連続メモリ前提で最適化
- indirect command 構築のアロケーションをフレーム再利用型に変更
- skinned / non-skinned の分岐を明示し、通常ケースの分岐コストを削る

### 計測項目

Phase 2 は「CPU 準備が軽くなったか」を定量で確認できる状態にする。
最低限、以下を毎フレームまたは一定間隔で採取する。

- `std::vector` の再確保回数
- mesh / material ソートコスト
- visible instance 抽出のヒット率
- indirect command 構築時のアロケーション回数
- skinned / non-skinned の分離比率
- batch あたり instance 数の分布
- prepared draw 数と実 draw 数の比率

### 改善基準

以下が見えるようになって初めて、Phase 3 へ進む。

- 再確保回数がフレーム依存で暴れていない
- ソートと抽出の CPU 時間が分離して見える
- indirect command 構築の一時確保が抑えられている
- skinned と non-skinned の比率から instancing 効果を推定できる

### 対象箇所

- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Mesh\MeshExtractSystem.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RenderPass\ExtractVisibleInstancesPass.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RenderPass\BuildInstanceBufferPass.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\RenderPass\BuildIndirectCommandPass.cpp`

### 完了条件

- CPU 側の prepared draw 準備時間が短縮される
- draw 数増加に対する CPU 時間の伸びが緩やかになる

---

## Phase 3: command recording のマルチスレッド化

### 目的

DX12 の本命。pass recording を複数 worker へ分散する。

### 方針

1 本の command list に全 pass を積む構造をやめ、pass 群または pass chunk ごとに command list を分ける。

### 前提: RenderContext の分解

現在の `RenderContext` は、フレーム全体で共有される状態と、view ごとの状態と、prepared draw 用の一時データをまとめて持ちやすい。
このまま worker thread に広げると、所有権と更新順が曖昧になり事故要因になる。

そのため、Phase 3 に入る前提として `RenderContext` は少なくとも次の 4 層に分解する。

#### Frame shared state

- graphics device 参照
- グローバル light / environment
- frame 共通の profiler / timing
- frame 共通の resource table

#### View state

- camera
- view / projection
- viewport
- target / depth
- history / jitter
- view 単位の visible set

#### Pass local state

- その pass が読む resource handle
- その pass 専用の scratch
- prepared draw の参照範囲

#### Worker local recording state

- worker 専用 command list
- worker 専用 allocator
- worker 専用 descriptor / upload allocator
- worker 専用 temporary buffer

#### allocator の寿命ルール

worker local recording state に含まれる allocator は、寿命を明確に固定する。

- command allocator は frame in-flight 数ぶん保持する
- descriptor allocator は worker ごと・frame ごとに分ける
- upload allocator も worker ごと・frame ごとに分ける
- `Reset()` は、その frame の GPU 完了が確認できたタイミングでのみ行う
- 同一 allocator を GPU 使用中フレームと次フレームで共有しない

つまり、allocator の基本単位は

- `worker`
- `frame index`
- 必要なら `queue type`

の組み合わせで管理する。

これにより、worker が記録した command list と allocator の寿命が一致し、フレーム跨ぎの破綻を防ぐ。

### 分解ルール

- frame shared state は読み取り中心に限定する
- view state は view owner だけが構築し、worker は読み取りのみ
- pass local state は pass 実行中だけ有効
- worker local recording state は他 worker と共有しない
- `active buffer` や `prepared command` のような可変状態は、worker に共有しない
- main thread は dispatch と submit 順の決定だけを担当する

### 実施内容

#### 3.1 command list ownership の分離

- main thread 用 command list
- worker thread 用 command list pool
- worker ごとの command allocator pool

を持つ。

#### 3.2 worker recording 対象

まず並列化しやすい対象から着手する。

- `GBufferPass`
- `ShadowPass`
- `DrawObjectsPass`
- `ForwardTransparentPass` の一部準備

#### 3.2.1 command list 粒度

この段階で command list 粒度を明文化する。

- 基本単位は `1 pass = N command list`
- `N` は view 数、batch 数、負荷で決まる
- `1 view = 1 command list` を初期形とし、重い pass だけ `1 pass = 複数 command list` へ広げる

初期実装では以下を原則とする。

- 軽い pass: `1 pass = 1 command list`
- 重い geometry pass: `1 pass = view ごとに 1 command list`
- 将来的な大規模 pass: `1 pass = batch chunk ごとに複数 command list`

material 単位で command list を細かく割るのは禁止する。
粒度を細かくしすぎると submit オーバーヘッドが増え、逆効果になりやすいためである。

#### 3.3 実行モデル

- main thread: FrameGraph compile / job dispatch / submit order 決定
- worker thread: command recording
- main thread: recorded command list を queue にまとめて submit

#### 3.3.1 scheduling モデル

recording は task graph 上で次のように流す。

1. main thread が FrameGraph compile を実行
2. compile 済み pass 群から independent job を生成
3. worker が command recording を担当
4. main thread が dependency 解決済み command list を submit 順に束ねる

pass 間依存は FrameGraph の dependency をそのまま task graph へ落とす。
つまり将来的には、FrameGraph は compile 結果を task graph へ変換する責務も持つ。

#### 3.3.2 submit モデル

submit は main thread が一元管理する。

基本方針:

- worker は command list の recording だけを担当する
- submit は graphics queue owner である main thread のみが行う
- submit 単位は「依存が解決済みの pass 群」または「view 内の pass chunk」とする

初期形:

- `1 pass chunk = 1 submit group`
- group 内に複数 command list があってよい
- main thread は group 順に `ExecuteCommandLists` する

#### 3.3.3 command list の merge 方針

worker が記録した command list を 1 本へ再記録し直すことはしない。

採用方針:

- merge は「再記録」ではなく「submit 時の配列結合」とする
- 同じ submit group に属する command list は、そのまま queue へまとめて渡す
- 依存のある group は fence または frame graph dependency で順序を保証する

これにより、worker の recording 結果を壊さずに submit できる。

#### 3.3.4 fence 方針

fence は queue 単位で管理する。

- graphics queue submit のたびに fence value を進める
- frame 完了判定は、その frame が使った submit group の最終 fence で行う
- allocator / descriptor / upload の再利用可否も、その fence 完了で判断する

初期段階では、細かい pass ごとの fence を乱立させず、
「frame または submit group 単位」の fence に限定する。

#### 3.4 禁止事項

- 1 本の `ICommandList` を複数スレッドで共有しない
- worker thread 間で descriptor allocator を共有しない
- shared mutable state を pass 内で直接触らない
- 1 つの `RenderContext` インスタンスを複数 worker で更新しない

### 期待効果

- editor 時の CPU フレーム時間を大きく削減できる
- multi-view / multi-window に備えられる

### 完了条件

- command recording が複数 worker で走る
- main thread は dispatch と submit 中心になる
- DX12 が DX11 より遅い主要因の 1 つを取り除ける

---

## Phase 4: tool-ready multi-view / multi-window 対応 FrameGraph

### 目的

将来の専用ツールウィンドウを前提に、描画を `view` 単位で完全に分離する。

ここで言う multi-window は、

- 常時 `Game View` と `Scene View` を 2 画面同時表示すること

ではない。

本フェーズの本質は、

- 通常時は 1 view でもよい
- 必要なときだけ 2 本目、3 本目の tool view を増やせる
- その際に main view を壊さない

という UE 系エディタに近い土台を作ることにある。

### 方針

- グローバル定数と view 定数を分ける
- `FrameGraph` は 1 本共有ではなく、view ごとに独立実行する
- 各 view は独自の
  - render size
  - viewport
  - target / depth
  - prev scene / history / jitter
  - shadow state
  - renderer state
  - post / display 出力
  を持つ
- panel 表示サイズと内部 render size を分離する

### Phase 4 で避けるべき誤実装

以下は「view を分けたように見えて、実際には壊れる」典型なので禁止する。

- 1 つの `RenderContext` を view ごとに上書き再利用する
- 1 つの `FrameGraph` 実体を複数 view で使い回す
- 1 セットの pass インスタンスを複数 view で使い回す
- view ごとに camera だけ分けて、内部 pass サイズは screen 固定のままにする
- `SceneColor` / `PrevScene` / `DisplayColor` だけ差し替え、GBuffer や AO/SSR/Fog の内部作業 RT は共有する
- panel の表示サイズをそのまま camera aspect に使い、render target 側のサイズ契約を合わせない

### 補足

現在は「FrameGraph は直列、recording だけ並列」が現実的な段階だが、最終形では FrameGraph 自体も並列化対象に含める。
具体的には、compile 済み pass DAG を task graph 化し、依存のない pass / view は並列 recording できる構造を目指す。

ただし Phase 4 の完了条件は「並列化」ではなく「view 分離の完全性」である。

### 実施内容

#### 4.1 `RenderViewContext` 導入

`RenderContext` を view ごとに直接書き換えて回すのではなく、view 専用の文脈を明示する。

最低限必要な要素:

- camera / view / projection
- render size / display size / aspect
- main render target / depth
- scene color / prev scene / display color
- shadow map
- scene constant buffer / shadow constant buffer
- per-view renderer
- history key
- per-view feature enable

#### 4.2 `CbSceneGlobal` と `CbView` の分離

`CbScene` 1 本共有ではなく、以下に分割する。

- `CbSceneGlobal`
  - 時間
  - directional light
  - point lights
  - environment / IBL
  - view 非依存の共通値
- `CbView`
  - viewProjection
  - prevViewProjection
  - jitter
  - cameraPosition
  - renderW / renderH
  - view 専用 shadow / history 参照

#### 4.3 `RenderPipeline::ExecuteView()` 化

`ExecuteViews()` はあくまで orchestration に限定し、実体は

- `ExecuteView(RenderViewContext&)`

の積み上げにする。

これにより

- 1 view 実行
- 複数 view 実行
- 将来の tool window 実行

を同一モデルで扱える。

#### 4.4 FrameGraph / pass 実体の view 分離

各 view は以下を独立保持する。

- `FrameGraph`
- pass instance 群
- transient resource pool 上の resource namespace
- history state

pass が `ResourceHandle` や一時キャッシュをメンバに持つ場合、view ごとに独立インスタンス化する。
共有 pass のまま再入可能であることを前提にしない。

#### 4.5 render size と panel size の分離

本フェーズで最重要の土台。

各 view は少なくとも次を持つ。

- `renderWidth / renderHeight`
- `displayWidth / displayHeight`
- `panelWidth / panelHeight`

ルール:

- camera aspect は `renderWidth / renderHeight` から決める
- panel は presentation のみ担当する
- panel サイズ変更時に、必要なら view の render target 群を再確保する
- GBuffer / Scene / PrevScene / Display / AO / SSR / Fog など、内部 pass で使う全 RT は view の render size に従う

#### 4.6 tool window 前提の view 管理

本フェーズでは「Game + Scene を常時 2 画面表示」を必須にしない。

代わりに、以下の形を成立させる。

- main view は 1 本で正常動作
- 必要な tool が開いたときだけ追加 view を生成
- tool view を閉じたら history / RT / shadow / renderer を安全に破棄
- tool view の存在が main view の見た目や速度を壊さない

想定する tool view:

- model behavior tool
- particle / mesh effect tool
- 将来の preview / bake / capture 系ツール

### 完了条件

Phase 4 完了は、以下をすべて満たした状態を指す。

- main view 単体で Phase 3 と同じ見た目を維持する
- 2 本目の tool view を追加しても main view が変化しない
- tool view を追加しても
  - 色が変わらない
  - model が flicker しない
  - history / prev scene が混線しない
  - shadow が混線しない
- view ごとに
  - render size
  - internal pass size
  - display target
  - history
  が独立している
- panel 表示サイズの違いで camera / projection / internal pass が壊れない

### 効果

- カメラ競合を防げる
- editor の専用ツールウィンドウを自然に増やせる
- main view を壊さずに preview/tool view を追加できる
- マルチスレッド recording と相性が良い

---

## Phase 5: GPU driven の強化

### 目的

CPU 準備をさらに減らし、DX12 の利点を本格的に活かす。

### 実装順

GPU driven は次の順で導入する。

1. CPU args 構築
2. GPU args 更新
3. `ExecuteIndirect`
4. material grouping
5. bindless 検討

この順を崩さない。
`bindless` は最終段階であり、先に args と grouping を安定させる。

### 5-A. GPU culling

- compute ベースの culling を導入する
- frustum / distance / optional occlusion を GPU 側で処理する
- CPU 側 visible 抽出との差分を比較できるようにする

### 5-B. visible list 生成

- culling 結果を GPU 可視リストとして保持する
- instance buffer と 1 対 1 で参照できる構造へ揃える

### 5-C. indirect args 更新

- visible list から `DrawIndexedInstancedIndirect` 引数を更新する
- CPU 側で args を毎回組み立てる経路を段階的に外す

### 5-D. ExecuteIndirect 本格化

- prepared command を GPU 側更新済み args へ接続する
- graphics queue 側では submit と依存管理だけを見る構造へ寄せる

### 5-E. material grouping

- material 切り替え回数を減らすため、grouping key を強化する
- mesh / material / shader variant ごとのまとまりを保つ

### 5-F. bindless 化の検討

- bindless は最終段階とする
- 先に grouping / visible list / args 更新を安定させる
- bindless は descriptor 方針を大きく変えるため、前提が整ってから導入判断する

### 5-G. skinned mesh 方針

- compute skinning
- skinned instance grouping
- skinned を別 queue / 別 path に残す案

を比較し、non-skinned と同じ timeline に無理やり乗せない

### 完了条件

- non-skinned は CPU 主導 draw からほぼ脱却
- CPU は high-level dispatch 中心になる

---

## Phase 6: async compute の導入

### 目的

graphics queue に集中している重い後処理を分散する。

### 候補

- GTAO
- SSGI
- SSR blur
- volumetric fog の一部
- reduction / prefix / culling 系

### 注意点

- queue 分離は barrier と ownership を厳密に管理する必要がある
- async compute は導入順を誤ると逆に遅くなる
- まず graphics queue 側で pass の寿命と依存を明確にしてから着手する

### 導入条件

以下をすべて満たした場合にのみ導入する。

- graphics queue 上での resource lifetime 管理が完成している
- パス依存が可視化済みで、queue 間の ownership 移動が説明できる
- GPU 時間計測が導入済みである
- graphics queue 単独運用時のボトルネック位置が把握できている
- 対象パスを単独 queue 化した方が速いと確認できている

これを満たさない段階では、async compute は採用しない。

---

## 7. マルチスレッド導入方針

## 7.1 導入する価値

結論として、導入価値は高い。

理由:

- DX12 の command recording は本来マルチスレッドと相性が良い
- 本エンジンは editor / preview / 将来の複数ツールビューを抱える予定で、CPU 側準備が重くなりやすい
- Archetype ECS と batch 抽出は並列処理に向く

## 7.2 先にやるべきこと

ただし、以下を整理してから導入する。

- worker ごとの command allocator
- worker ごとの descriptor / upload ownership
- RenderContext の thread-safe 分離
- view ごとの独立状態

## 7.2.1 Phase 2.5: CPU task graph 化

Phase 3 の前に、CPU 準備系を task graph 化する中間段階を設ける。

対象:

- ECS 抽出
- visible instance 抽出
- instance flatten
- indirect build

目的:

- command recording 以外の CPU 準備を先に並列化する
- worker thread / dependency / scratch allocator の運用を先に安定させる
- Phase 3 の recording 並列化を楽にする

完了条件:

- 上記 4 系統が job graph 上で走る
- dependency が明示され、main thread での逐次実行から脱却している
- FrameGraph compile 後に、その結果を task graph 側へ安全に引き渡せる

## 7.3 導入段階

### 段階 A
- CPU task 並列化
- culling / batch build / instance flatten の並列化

### 段階 B
- command list recording 並列化
- pass または view ごとに worker へ分配

### 段階 C
- multi-view と組み合わせる
- Scene / Game / Tool window を独立 recording

### 段階 D
- compute culling / indirect 更新まで並列化

---

## 8. まず着手すべき実装

現実的に、次の順がよい。

1. Phase 0: 計測の標準化
2. Phase 1: DX12 の state / barrier / upload の無駄削減
3. Phase 3 の前段として、CPU task 並列化を導入
4. worker command list pool を追加
5. `GBufferPass` と `ShadowPass` の recording を並列化
6. その後に multi-view FrameGraph へ進む

理由:

- いきなり multi-window FrameGraph に行くより、先に recording の並列化基盤を作った方が効果が出やすい
- その基盤があると、将来の専用ツールウィンドウも設計しやすい

---

## 9. 完了の定義

本仕様の完了は次を指す。

- DX12 が DX11 と同等以上の描画速度を出す
- CPU フレーム時間の主要部が並列化されている
- multi-view / multi-window 追加で破綻しない
- command recording が単一主スレッド依存から脱却している
- GPU driven の準備が整い、将来的な compute culling 強化へ自然に進める

---

## 10. まとめ

現在の DX12 が遅い主因は、DX12 を使っていながら

- command recording が 1 本の主スレッド依存
- 描画準備が主スレッド集中
- multi-view / multi-window を前提にしていない

ことにある。

したがって本命は、単なる小手先の最適化ではなく

- CPU 準備の整理
- command recording のマルチスレッド化
- multi-view FrameGraph
- GPU driven 強化

の順に進めることにある。

この順で進めれば、DX12 は DX11 より速くなる余地が十分にある。
