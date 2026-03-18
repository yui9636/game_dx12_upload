# DX12 Scene Parity 再構築仕様書

作成日: 2026-03-17
対象: `C:\Users\yuito\Documents\MyEngine_Workspace\game`
方針: DX11 非破壊を絶対条件とし、場当たり修正ではなく、DX11 を基準参照として DX12 の描画契約を段階的に一致させる。

## 1. 目的

本仕様書の目的は、DX11 モードで現在成立しているシーン描画結果を、DX12 モードでも同一の見た目で再現するための再構築方針を定義することである。

ここでいう「同一の見た目」は、少なくとも以下を含む。

- モデルのベースカラー、法線、ラフネス、メタリックが同様に反映される
- IBL、GTAO、SSR、SSGI、Volumetric Fog、Shadow が同等の寄与を持つ
- Skybox が同一のキューブマップとして表示される
- Editor の `Scene View` 上で DX11 と同等に確認できる
- DX11 側の既存描画、Editor 操作、Asset Browser、サムネイル、UI が壊れない

## 2. 現在の確認済み状態

### 2.1 動いているもの

- DX12 起動、Present、ImGui 表示
- Asset Browser とサムネイル生成
- Skybox の基本表示
- モデルのロード、Hierarchy へのドロップ、描画キュー投入
- GBuffer へのモデル書き込み
- DeferredLighting の最低限の出力

### 2.2 ログで確認済みの事実

`Saved\Logs\runtime.log` から、以下は確認済みである。

- `ResourceManager` 経由の DX12 テクスチャ生成は成立している
- `GBuffer0` にはモデルのアルベド色が正しく書かれている
- `diffuseIBL` / `specularIBL` の cubemap リソースは DX12 で生成済みである
- `DeferredLightingPass` に IBL / LUT / Shadow の入力は到達している
- ただし最終見た目は DX11 と一致していない

### 2.3 現在の問題の本質

現在の DX12 は「表示は出るが、各パスが本来の契約通りにつながっていない」状態である。

特に以下が本質問題である。

1. 汎用 SRV バインド経路が不安定で、パスごとに専用 heap 回避が増えている
2. sampler register と DX12 static sampler 契約がズレていた
3. DeferredLighting に一時的な色保持 fallback が混在しており、本来の PBR の見た目を歪めている
4. DX11 参照結果に対する系統的な比較手順がなく、修正の評価が目視依存になっていた

## 3. 根本原因の層別整理

### 3.1 RHI / Binding 層

- DX12 の SRV テーブル運用が不安定で、`EnsureSrvBlock()` 系の汎用経路だけでは一部パスで期待通りのバインドにならない
- null SRV の view dimension が shader 側期待と一致しないケースがあった
- static sampler の slot 定義と HLSL 側 register 指定が不整合だった
- root signature 再設定タイミングが mid-frame binding を破壊しうる構造があった

### 3.2 Render Pass 層

- `DeferredLightingPass` が入力数・slot 数ともに最も複雑で、実質的な不一致の中心だった
- GTAO / SSGI / SSR / Fog の中間パスでも sampler slot のズレがあり、入力の意味が DX11 と一致していなかった
- `FinalBlitPass` / `Scene View` / `Display` の流れが複数回の暫定修正で複雑化している

### 3.3 Shader 層

- `DeferredLightingPS.hlsl` に parity fallback が混入しており、DX11 と異なる見た目を正規経路のように見せてしまっている
- GBuffer のエンコード契約と Deferred 側のデコード契約は概ね成立したが、最終 BRDF 合成はまだ DX11 と一致していない
- sampler register の不整合が複数 shader に広がっていた

### 3.4 検証層

- DX11 を基準とした固定シーン比較が未整備
- 参照画像、参照ログ、参照パラメータが揃っていない
- そのため、1 箇所の改善が別の箇所の劣化を隠してしまう

## 4. 絶対ルール

以後の DX12 parity 作業は、以下のルールを厳守する。

1. DX11 非破壊
   - DX11 の描画仕様変更は禁止
   - DX11 側コード変更は、共通化のための interface 拡張または compile 維持に限る

2. 参照先は DX11
   - 見た目の正解は常に DX11 の出力とする
   - DX12 側独自 fallback の見た目を正解扱いしない

3. ログ先行
   - 目視だけで修正しない
   - 各パスで「入力が何で、出力がどうなったか」をログまたは readback で確認する

4. 暫定回避の封じ込め
   - pass 個別 heap / 色保持 fallback / 空色 fallback は「暫定」と明記し、撤去条件を持つ
   - 正規経路が直ったら撤去する

5. 比較シーン固定
   - 基準モデル、基準 skybox、基準 directional light、基準 camera を固定して検証する

## 5. 目標とする描画契約

### 5.1 GBuffer 契約

- `GBuffer0`: Albedo (linear) + Metallic
- `GBuffer1`: Normal + Roughness
- `GBuffer2`: World Position + Depth/aux
- `GBuffer3`: Velocity

DX11 と DX12 で、フォーマット、色空間、意味論を一致させること。

### 5.2 DeferredLighting 契約

- t0: GBuffer0
- t1: GBuffer1
- t2: GBuffer2
- t3: GTAO
- t4: ShadowMap
- t5: SSGI
- t6: Fog
- t7: SSR
- t8: Reflection Probe
- t9: Scene Depth
- t33: diffuse IBL
- t34: specular IBL
- t35: LUT GGX

sampler は以下に統一する。

- s1: ShadowCompare
- s2: PointClamp
- s3: LinearClamp

### 5.3 正規見た目

Deferred の正規見た目は、以下の順で合成されること。

- Direct Lighting
- Shadow
- Diffuse IBL
- Specular IBL
- GTAO による間接遮蔽
- SSR / Probe による反射
- SSGI による間接拡散
- Fog

これらを正規経路で成立させた後にのみ、fallback を撤去する。

## 6. 再実装フェーズ

### Phase 0: 基準作成

目的:
- DX11 の正解を固定する

作業:
- DX11 起動用フラグを一時切替できるようにする
- 基準シーンを固定する
  - skybox: `Data/Texture/IBL/Skybox.dds`
  - model: `A5`
  - fixed camera / light
- DX11 で以下を保存する
  - Scene View スクリーンショット
  - GBuffer0/1/2 readback
  - SceneColor readback
  - 主要ログ

成果物:
- `Saved/Reference/DX11/` に画像・ログを保存

### Phase 1: Binding 契約の正規化

目的:
- pass ごとの場当たり heap 回避を減らし、DX12 の binding 契約を定義する

作業:
- `DX12RootSignature` の sampler / SRV 契約を仕様化
- `DX12CommandList` の汎用 SRV テーブルを再設計
- null SRV の dimension を shader ごとに一致させる
- `SetGraphicsRootSignature` 再設定で既存 binding を壊さない構造にする

対象:
- `Source/RHI/DX12/DX12CommandList.*`
- `Source/RHI/DX12/DX12RootSignature.*`

完了条件:
- sky / GBuffer / deferred / post passes が、専用 heap 回避なしでも正しく動く見込みが立つこと

### Phase 2: GBuffer / Deferred の正規化

目的:
- 現在の fallback 依存をやめ、DX11 と同じ PBR 計算へ寄せる

作業:
- `GBufferPBRPS.hlsl` の出力契約を DX11 と照合
- `DeferredLightingPS.hlsl` の入力解釈を DX11 と照合
- 一時的な色保持補正、可視化 fallback、空色 fallback をフラグ化または撤去
- `DirectBRDF`, `DiffuseIBL`, `SpecularIBL`, AO, SSR, Probe, Fog の寄与を順に単独比較する

対象:
- `Shader/GBufferPBRPS.hlsl`
- `Shader/DeferredLightingPS.hlsl`
- `Shader/PBR.hlsli`
- `Shader/ShadingFunctions.hlsli`

完了条件:
- DX12 でモデルの base color、roughness response、metal response が DX11 に近いこと

### Phase 3: 補助パス parity

目的:
- GTAO / SSGI / SSR / Volumetric Fog が DX11 と同等に働くようにする

作業:
- 各パスごとに単体比較を行う
- sampler / viewport / half-res の扱いを DX11 と揃える
- 出力を readback して平均色・エッジ・存在量を比較する

対象:
- `Source/RenderPass/GTAOPass.*`
- `Source/RenderPass/SSGIPass.*`
- `Source/RenderPass/SSRPass.*`
- `Source/RenderPass/VolumetricFogPass.*`
- 各対応 HLSL

完了条件:
- DeferredLighting に入る直前の中間テクスチャが DX11 と同傾向であること

### Phase 4: Scene View / FinalBlit 整理

目的:
- Editor の見た目と実出力を一致させる

作業:
- `Scene` と `Display` の役割を固定
- `FinalBlitPass` の必要性と出力先を明確化
- `Scene View` が何を見ているかを API 共通で統一

対象:
- `Source/Engine/EngineKernel.cpp`
- `Source/RenderContext/RenderPipeline.*`
- `Source/RenderPass/FinalBlitPass.*`
- `Source/Layer/EditorLayer.cpp`
- `Source/ImGuiRenderer.*`

### Phase 5: 暫定コード撤去

目的:
- 場当たり修正を整理し、維持可能な構造に戻す

撤去対象:
- Deferred の色保持 fallback
- Skybox の暫定 fallback 色
- pass 個別の特殊回避で、正規 binding に統合できるもの
- 使われなくなったログ / readback

## 7. 検証方法

各フェーズで以下を必須とする。

### 7.1 ビルド
- Debug|x64 DX11 成功
- Debug|x64 DX12 成功

### 7.2 実行
- DX11 起動成功
- DX12 起動成功
- 60 秒クラッシュなし

### 7.3 参照比較
- 同一モデル `A5`
- 同一 skybox
- 同一 camera
- 同一 directional light

### 7.4 ログ
最低限以下を比較する。

- `GBuffer0Snapshot`
- `GBuffer1Snapshot`
- `GBuffer2Snapshot`
- `SceneOpaqueSnapshot`
- `SceneOverAlbedoMask`
- IBL / shadow / GTAO / SSR / SSGI 入力ログ

## 8. 現時点の実装判断

現状のコードは「完全に破綻している」のではなく、以下の段階まで来ている。

- Resource loading: 概ね成立
- GBuffer write: 概ね成立
- Skybox: 基本表示成立
- Deferred input binding: 一部成立、一部暫定
- Final PBR parity: 未達

したがって、次の作業は「全部作り直す」よりも、上記フェーズに沿って契約を固め直す方が妥当である。

## 9. 次の実施順

次に着手すべき順番は以下で固定する。

1. DX11 参照キャプチャを取得する
2. Deferred の fallback を無効化して差分を可視化する
3. GTAO / IBL / SSGI / SSR / Fog を 1 つずつ DX11 比較する
4. 正規経路が成立した段階で暫定回避を撤去する

以上を、DX11 非破壊を維持しながら進める。

## 10. 進捗メモ (2026-03-17 夜)

この仕様書に切り替えたあと、以下の共有fallback / 契約ずれは解消済みである。

- DeferredLightingPS.hlsl に入っていた共有の parity fallback を撤去
- SceneDataUploadSystem.cpp の DX12 専用 cascade split 無効化ハックを撤去
- SkyBoxPS.hlsl の共有青空 fallback を撤去
- GlobalRootSignature.cpp で、DX12 時に IBL を汎用 PSSetTextures(33, 2, ...) から流し込む処理を停止

これにより、DX11/DX12 共通シェーダに混ざっていた見た目補正と、DX12 側の責務衝突は一段整理できた。

### 現在の残件

- DeferredLightingPass の DX12 専用 heap 回避は、DX12CommandList の共有テーブル経路へ移行済み。GBufferPBRShader は今回の回帰で黒化したため、契約が固まるまで専用 heap を維持する
- 汎用 DX12CommandList::PSSetTextures() の契約を最終形にできていない
- GTAO / SSGI / SSR / Fog の寄与は、まだ DX11 と同じ前提で検証し切れていない
- Display と Scene View の最終出力経路も整理余地がある


