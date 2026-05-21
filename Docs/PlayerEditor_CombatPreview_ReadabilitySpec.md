# Player Editor 超改善仕様書

## 一言でいうと

Player Editor は「パネルが並ぶ画面」ではない。  
**画面いっぱいにプレイヤーが見えて、必要な道具だけを一時的に出すアクション制作画面**にする。

小学生でも分かる基準はこれ。

- まずキャラが大きく見える
- どこを押せば何が出るか分かる
- いらないものはすぐ消える
- 攻撃を押したら攻撃が見える
- 当たり判定が当たったか見える
- カメラで迷子にならない

## 絶対ルール

### 1. パネルを置かない

左にパネル、右にパネル、下にパネル、という発想を捨てる。  
常時置いてよいのは上の薄いバーだけ。

悪い例。

```text
------+----------------+------+
|Bone |    Viewport    |Prop  |
|State|                |Input |
+------+----------------+------+
|Timeline                    |
+----------------------------+
```

これは画面が道具に占領されている。

正しい例。

```text
+------------------------------------------------+
| Open Save Setup Edit Test Camera Hitbox Reset  |
+------------------------------------------------+
|                                                |
|                                                |
|                  PLAYER VIEW                   |
|                                                |
|                                                |
|   State Idle   Frame 0   HP 100   Hitbox OFF   |
+------------------------------------------------+
```

普段はキャラを見る。  
道具は必要な時だけ出す。

### 2. Viewport が画面そのもの

Viewport は「中央の一部」ではない。  
Player Editor の本体そのもの。

最低条件。

- 画面の 85% 以上は Viewport
- Viewport の上に小さい道具だけを重ねる
- キャラ、床、ライト、カメラ、当たり判定が常に読める
- 黒画面は禁止
- 足元だけ映る状態は禁止

### 3. 道具は Drawer ではなく Tool Popover

「パネル」も「ドロワー」も、置きっぱなしになりやすい。  
代わりに **Tool Popover** を使う。

Tool Popover はこういうもの。

- ボタンを押すと出る
- Viewport の上に浮く
- 小さい
- すぐ閉じられる
- 1 個だけ表示する
- 作業が終わったら自動で閉じてもよい

例。

```text
+------------------------------------------------+
| Open Save Setup Edit Test Camera Hitbox Reset  |
+------------------------------------------------+
|                                                |
|                  PLAYER VIEW                   |
|                                                |
|     +----------- Hitbox Tool -----------+      |
|     | Body   Attack                     |      |
|     | Radius  [----o------]             |      |
|     | Bone    hand_r                    |      |
|     | Apply   Close                     |      |
|     +-----------------------------------+      |
|                                                |
+------------------------------------------------+
```

## 画面の基本形

### 上バー

上バーは薄く、1 行だけ。

```text
Open | Save | Setup Full | Edit | Test | Camera Fit | Reset | Tools
```

役割。

- `Open`: prefab / model を開く
- `Save`: prefab に保存
- `Setup Full`: Player 一式を作る
- `Edit`: 編集モード
- `Test`: 戦闘テストモード
- `Camera Fit`: 全身が見える位置へ戻す
- `Reset`: Preview 状態を戻す
- `Tools`: 道具一覧

### 下ステータス

下には小さい状態表示だけ置く。

```text
State: Idle | Anim: Idle | Frame: 0 | HP: 100 | Hitbox: 0 | Contact: 0
```

これは編集 UI ではない。  
今どうなっているかを見るためのメーター。

## モード

Player Editor は 2 つだけ。

## Edit

作るモード。

できること。

- アニメーションを見る
- State を作る
- Timeline を作る
- Hitbox を置く
- Body を置く
- Input を設定する
- Socket を置く
- 保存する

見た目。

```text
+------------------------------------------------+
| Open Save Setup Full  Edit[ON] Test Camera Fit |
+------------------------------------------------+
|                                                |
|                  PLAYER VIEW                   |
|                                                |
|  Tools: State Timeline Hitbox Body Input Bone  |
|                                                |
|  State Idle | Anim Idle | Frame 0 | Hitbox 0   |
+------------------------------------------------+
```

`Tools` の項目を押した時だけ Tool Popover が出る。

## Test

動かして確認するモード。

できること。

- WASD / パッドで動く
- Attack を出す
- Dodge を出す
- Dummy に攻撃を当てる
- HP が減る
- Hitbox が見える
- Body が見える
- Third Person Camera で見る

見た目。

```text
+------------------------------------------------+
| Save Setup Full Edit Test[ON] Hitbox Body Reset |
+------------------------------------------------+
|                                                |
|                  COMBAT VIEW                   |
|                                                |
|        Player                         Dummy    |
|                                                |
|  State Attack1 | Frame 24 | Dummy HP 80        |
+------------------------------------------------+
```

Test モードでは編集用の道具をなるべく出さない。  
ゲームとして見えることを最優先する。

## Tool Popover 一覧

### State Tool

ステートを選ぶ道具。

表示するもの。

- State 一覧
- 今の State
- Animation
- Timeline
- Loop
- Speed
- Preview ボタン

やらないこと。

- 大きなノードグラフを常時出さない
- 全ステート情報を画面に並べない

### Timeline Tool

タイムラインを作る道具。

表示するもの。

- Frame bar
- Track list
- Add Hitbox
- Add VFX
- Add Audio
- Add Shake
- Add Event
- 選択中 item の短い編集欄

Timeline Tool は横長でもよいが、Viewport の半分以上を隠してはいけない。

### Hitbox Tool

攻撃判定を作る道具。

表示するもの。

- Bone
- Radius
- Offset
- Start Frame
- End Frame
- Color
- Preview ON/OFF

大事なこと。

- Skeleton を巨大ツリーで出さない
- Viewport 上の骨マーカーをクリックして選ぶ
- 今どの骨に付いたかを Viewport に直接表示する

### Body Tool

押し戻しや被弾用 Body を作る道具。

表示するもの。

- Body list
- Shape
- Bone
- Radius / Size
- Offset
- Attribute
- Visible ON/OFF

### Input Tool

入力を変える道具。

表示するもの。

- Attack
- Dodge
- LockOn
- MoveX
- MoveY

表示名は分かりやすくする。

- Keyboard: `W`, `A`, `S`, `D`, `Space`
- Mouse: `Left`, `Right`, `Middle`
- Gamepad: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`

`M3` のような表示は禁止。

### Bone Tool

骨を見る道具。

表示するもの。

- Search
- Bone name
- Parent
- Use for Hitbox
- Use for Body
- Use for Socket

巨大な階層ツリーは補助扱い。  
基本は Viewport 上の骨マーカーをクリックする。

## Camera 仕様

## Setup Full Player と Third Person

`Setup Full Player` を押したら、Player Entity は必ず Third Person 用の設定を持つ。

必ず付けるもの。

- `CameraTPVControlComponent`

この component は Player Entity に付ける。  
Camera Entity に付けるのではない。

理由。

- 「この Player を追う」という設定だから
- Scene でも Preview でも同じ Player 設定を使えるから
- カメラを Prefab に含めなくてよいから

## Preview でも Third Person

Player Editor の Preview でも、編集中の Player Entity に `CameraTPVControlComponent` を付ける。

Test モードでは Preview Camera がこれを読む。

つまり。

- Edit モード: 全身確認用 Camera
- Test モード: Third Person Camera

## Bounding Box Fit

モデルを開いたら、必ず全身が見える位置にカメラを置く。

やってはいけないこと。

- 足元だけ映る
- 顔だけ映る
- モデルが画面からはみ出る
- Scene の `0.1` scale に引っ張られて Preview が小さすぎる

やること。

- Model の Bounding Box を読む
- Preview 表示用 scale で計算する
- 高さと半径から距離を決める
- キャラの中心より少し上を見る

計算イメージ。

```text
height = bounds height
radius = bounds radius
lookAt = bounds center + 少し上
distance = radius に合わせる
camera = player の後ろ + 上
```

小学生向けに言うと。

```text
キャラが大きいなら、カメラは遠くへ行く。
キャラが小さいなら、カメラは近くへ行く。
いつでも全身が見える。
```

## Preview Scale と Save Scale

ここは絶対に混ぜない。

### Preview Scale

Player Editor で見やすくするための大きさ。

- 編集用
- 保存しない
- Camera Fit に使う

### Save Scale

Prefab / Scene で使う本当の大きさ。

- 保存する
- Game View で使う
- Scene の `0.1` などはこっち

ルール。

```text
Player Editor では見やすく 1.0 で出してよい。
でも保存時に Scene 用 scale を壊してはいけない。
```

## Dummy 仕様

Dummy は Test モードだけで出る。

Dummy は敵ではなく、当たり判定を見るための的。

持つもの。

- Transform
- Body Collider
- Health
- Team
- 名前
- 簡単な見た目

保存しないもの。

- Dummy Entity
- Dummy の位置
- Dummy の HP

## Hitbox 表示

Hitbox は言葉でなく、Viewport に直接出す。

表示。

- Attack: 赤
- Body: 青
- Contact: 黄色
- Active Hitbox: 明るく表示
- Inactive Hitbox: 薄く表示

当たった瞬間は一瞬光らせる。

## 小学生でも分かる操作

### 攻撃判定を作る

1. `Hitbox` を押す
2. `Add Attack` を押す
3. 手の骨をクリックする
4. 丸の大きさを変える
5. 再生する
6. 当たるか見る

### Body を作る

1. `Body` を押す
2. `Add Body` を押す
3. 胸や腰の骨をクリックする
4. 大きさを変える
5. Dummy とぶつかるか見る

### 攻撃をテストする

1. `Test` を押す
2. Player と Dummy が出る
3. Attack を押す
4. 赤い Hitbox が出る
5. Dummy に当たる
6. Dummy HP が減る

### カメラを直す

1. `Camera Fit` を押す
2. 全身が見える

これだけ。

## 実行システム

Test モードでは本番に近い処理を回す。

必要なもの。

- Input
- PlayerInput
- Playback
- StateMachine
- Locomotion
- Dodge
- CharacterPhysics
- Timeline
- Animator
- RootMotion
- Transform
- TimelineHitbox
- Collision
- Damage
- Health
- VFX
- Audio
- CameraShake
- ThirdPersonCamera

目的は、Test で当たるなら Game でも当たる状態にすること。

## 保存するもの

- Player Prefab
- StateMachine
- TimelineLibrary
- InputActionMap
- Body Collider
- Attack Timeline
- Socket
- `CameraTPVControlComponent`

## 保存しないもの

- Preview Camera
- Dummy
- Floor
- Light
- Test 中の HP
- Test 中の位置
- Preview 用 scale
- Tool Popover の一時状態

## 実装フェーズ

### Phase 1: 画面を作り直す

完成条件。

- 常時パネルをなくす
- 画面のほとんどが Viewport になる
- 上バーだけ残す
- Tool Popover 方式になる
- Edit / Test を切り替えられる

### Phase 2: Camera Fit

完成条件。

- モデルを開くと全身が見える
- `Camera Fit` で必ず全身に戻る
- 巨大モデルでも足元だけにならない
- Scene scale と Preview scale が混ざらない

### Phase 3: Setup Full Player + Third Person

完成条件。

- `Setup Full Player` で `CameraTPVControlComponent` が付く
- Player Prefab に保存される
- Preview でも Test 時に Third Person Camera で見られる
- Bounding Box から初期カメラ値を決める

### Phase 4: Test Mode

完成条件。

- Player が動く
- Dummy が出る
- Attack が出る
- Dodge が出る
- Hitbox が見える
- Body が見える
- Dummy HP が減る

### Phase 5: 仕上げ

完成条件。

- Hitbox ON/OFF
- Body ON/OFF
- Contact ON/OFF
- Slow Motion
- Frame Step
- Reset Player
- Reset Dummy
- Reset Camera

## 非目標

今はやらない。

- 本格 AI
- 複数敵
- Level Editor の配置
- GameFlow 連携
- Cinematic Camera Track 完成
- Custom Track 完成

## 最終合格ライン

これを満たしたら合格。

- 開いた瞬間、キャラが全身見える
- 画面がパネルだらけではない
- 何を押せばよいか迷わない
- `Setup Full Player` で Third Person 設定が入る
- Test で Player を動かせる
- Attack / Dodge を確認できる
- Hitbox が見える
- Dummy に当たると HP が減る
- 保存しても Dummy や Preview Camera は混ざらない
- Scene View / Game View を壊さない

