# PlayerEditor UI / 機能 / 操作感 改善仕様書
作成日: 2026-04-14

## 1. 目的
PlayerEditor を、単に機能が並んでいるだけの画面から、迷わず使えて、触った結果がすぐ見えて、保存までの流れが分かりやすい実用編集ツールへ引き上げる。

本仕様は「日本語化」そのものが目的ではない。  
目的は、以下の3点を同時に改善することである。

- UI の密度を適正化すること
- 操作導線を短くすること
- 編集結果を preview で即確認できること

UE を参考にするが、UE の見た目を模倣することは目的ではない。  
このコードベースに不足している「分かりやすさ」「選びやすさ」「触りやすさ」を優先する。

## 2. 現状の問題
現状の PlayerEditor は、機能があるように見えても、狭いパネルに対して情報量が多すぎて使いづらい。

特に問題が大きいのは以下である。

- Skeleton パネルが狭いのに、ボーン選択に必要な情報が見づらい
- Socket 領域が大きく、Skeleton の作業領域を圧迫している
- `Add Socket` 周辺の余白が大きく、必要な操作だけを素早く行いにくい
- Animator パネルに説明文や余白が多く、一覧を読む前にスクロール負荷が発生する
- 英文の説明文が複数行に分かれている箇所があり、縦幅を消費している
- Timeline は track を置けるが、どの animation に対する編集かが見えにくい
- StateMachine は state / transition を触れるが、操作の入口が弱い
- Preview は動いていても、今何を見ているかが弱く、結果が分かりづらい
- 保存や dirty 状態の把握が弱く、編集の終わり方が分かりにくい

要するに、問題は単純な機能不足だけではない。  
**表示領域が狭いのに説明と余白が多いこと**、**何を触っているか分かりにくいこと**、**操作しても結果が即座に見えにくいこと**が本質である。

## 3. 改善方針
改善は次の順で行う。

1. 不要な説明文を削る
2. 各パネルの密度を上げる
3. 主要操作を短い導線にまとめる
4. 選択状態と preview を強く結ぶ
5. Timeline / StateMachine を「触れる」から「作れる」へ進める

設計原則は次の通り。

- ツリー構造は維持する
- ただし、狭い領域でも読める密度にする
- 長い英語説明は置かない
- 状態表示は短くする
- 余白は削るが、操作の見分けは壊さない
- 選択した対象が常に見えるようにする
- 操作結果が preview に即反映されるようにする

## 4. 画面全体の方針
既存の全体レイアウトは維持する。

- 左: Skeleton / StateMachine
- 中央: Viewport
- 右: Properties / Animator / Input
- 下: Timeline

ただし、各領域は「何をする場所か」が一目で分かるように整理する。

- Skeleton はボーン選択と socket 設定の場所
- Animator は animation 一覧の場所
- Timeline は animation 単位のイベント編集の場所
- StateMachine は state 遷移の場所
- Properties は選択中対象の詳細だけを出す場所

## 5. 各パネルの仕様

### 5-1. Skeleton
Skeleton パネルは、ボーンを選ぶこと自体を最優先にする。

要件:

- ツリー形式は維持する
- 1 行の高さを抑える
- ボーン一覧を見やすくする
- 検索欄は残すが、縦幅を取りすぎないようにする
- Socket 領域は最小限にする
- `Add Socket` 周辺はコンパクトにする
- Socket の編集欄は必要な時だけ見えるようにする
- `Apply to Item`、`Focus View`、`Reset` は選択ボーンの近くに置く

禁止事項:

- ボーン選択のために大きな余白を置くこと
- Socket が Skeleton の視認性を壊すこと
- 長い説明文を複数行置くこと

### 5-2. Animator
Animator パネルは、一覧の見やすさを最優先にする。

要件:

- 余白を削る
- 2 行以上の英語説明文を削除する
- 現在の状態は短い表示にする
- animation 一覧をスクロール前提で見やすくする
- ダブルクリックで preview / timeline 再生を開始する
- 現在選択中の animation が分かるようにする

禁止事項:

- 説明文のためにパネルを縦に使いすぎること
- 何が選ばれているか分からない一覧にすること
- 情報より余白が目立つこと

### 5-3. Timeline
Timeline は、animation ごとの編集場所として分かりやすくする。

要件:

- 現在編集中の animation を常に見せる
- `Bind Current Animation` のような文脈操作を分かりやすくする
- track 追加時は種類ごとの初期値を持たせる
- Event は point event として扱う
- Hitbox / VFX / Audio / CameraShake は、サイズや offset をその場で触りやすくする
- 区間選択から track / item を作れるようにする
- 選択 item の start / end をすぐ編集できるようにする

禁止事項:

- track を追加しただけで何を作ったか分からない状態にすること
- event / effect / collider の意味が曖昧なまま並べること
- animation 文脈が見えないこと

### 5-4. StateMachine
StateMachine は、state の一覧と遷移条件がすぐ読めることを優先する。

要件:

- state 一覧を見やすくする
- 選択中 state の animation / timeline / transition をすぐ分かるようにする
- transition 条件は値の羅列ではなく、意味が読める形にする
- input 由来の操作は、ここで決めてそのまま preview できるようにする
- default state は明示する
- state 追加はテンプレートを使って素早く行えるようにする

禁止事項:

- state 名、transition 条件、animation 割り当てが散らばって見えること
- どの state を選んでいるか分からないこと
- transition はあるのに、次に何を触ればよいか分からないこと

### 5-5. Properties
Properties パネルは、選択対象だけを短く出す。

要件:

- state / transition / track / item / bone / socket のどれを選んでいるか分かるようにする
- 長文の説明は置かない
- 必要な編集項目だけを出す
- 選択が無い時は短い案内だけにする

禁止事項:

- 選択が無い時に大きな空白だけを見せること
- 説明文で縦幅を消費すること

## 6. 操作仕様

### 6-1. Skeleton 操作
- ボーンはツリーから選ぶ
- 選択ボーンは item にすぐ適用できる
- `Focus View` で viewport をそのボーンへ寄せる
- Socket は必要最小限の操作で追加・編集できる

### 6-2. Animator 操作
- animation は一覧から選ぶ
- ダブルクリックで preview と timeline 再生を開始する
- 再生中は animation 名と frame を見えるようにする

### 6-3. Timeline 操作
- `+ Track` から track を追加する
- track 種類ごとに初期 item を持たせる
- 区間を選んで item を作れるようにする
- Event は単発点として扱う
- Hitbox / VFX / Audio はサイズや offset を触りやすくする

### 6-4. StateMachine 操作
- state を追加する
- transition を追加する
- transition の条件を入力操作に紐づける
- preview で state 切り替えを確認する

## 7. 表示密度の仕様
最も重視するのは、狭いパネルの中で読めること。

要件:

- Skeleton と Animator の説明文は最小化する
- 2 行以上の英語説明は削除する
- 余白は必要最小限まで削る
- Socket と Animator の補助UIは、常時表示ではなく必要時表示に寄せる
- パネルタイトルと状態表示だけで大意が分かるようにする

## 8. 保存と dirty 表示

- 編集内容があるものは dirty を明示する
- `Save All` でまとめて保存できる
- `Save As` は必要な時だけ出す
- 保存先が未設定の項目は短い案内で導く

## 9. Undo / Redo
最低限、次を undo / redo できるようにする。

- state 追加 / 削除
- transition 追加 / 削除
- track 追加 / 削除
- item の位置変更
- item の start / end 変更
- bone / socket の適用
- animation 選択

## 10. 実装フェーズ

### Phase 1
- Skeleton の密度を上げる
- Animator の余白と説明文を減らす
- 英文説明を削除する
- preview の状態表示を短くする

### Phase 2
- Timeline の track / item 編集を実用密度にする
- Event を animation ごとに扱いやすくする
- 区間選択から item を作れるようにする

### Phase 3
- StateMachine の transition 導線を分かりやすくする
- input 連携を整理する
- preview から state 遷移確認をしやすくする

### Phase 4
- パネル分割を進める
- PlayerEditorPanel.cpp の巨大化を抑える
- 表示ロジックと編集ロジックを整理する

## 11. 受け入れ条件
以下を満たしたら、本仕様の改善として成立する。

- Skeleton パネルでボーン選択が現実的に行える
- Socket 領域が Skeleton の操作を邪魔しない
- Animator パネルでアニメーション一覧が読みやすい
- 英文の長い説明文が表示されない
- Timeline で何の animation を編集中か分かる
- track 追加後に編集対象がすぐ分かる
- StateMachine で state と transition の意味が読める
- preview で選択状態と再生状態が見える
- 保存と dirty 状態が分かる

## 12. 補足
本仕様は日本語UIへの翻訳ではない。  
日本語で書かれているだけで、目的は UI / 機能 / 操作感の改善である。

