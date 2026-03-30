# オーディオシステム ECS 化・外部ライブラリ導入仕様書

## 目的

現在の音声再生基盤は、`Audio` singleton と `AudioSource` 共有ポインタを中心にした旧構造です。  
この構造は、現在の ECS editor / prefab / scene save-load / timeline / gameplay workflow と噛み合っていません。

本仕様書の目的は次の 4 点です。

- オーディオの正を ECS component / system に統一する
- 旧 `Audio` singleton を段階的に置き換えられる構造にする
- 2D / 3D / UI / BGM / one-shot / looping / timeline trigger を同一ルールで扱えるようにする
- 外部ライブラリ導入も見据えた、長期運用しやすい音声基盤を定義する


## 現状の問題

### 1. singleton 中心

現在は `Audio::Instance()` が唯一の入口です。

- scene / world 単位で分離できない
- editor / runtime / test を分けにくい
- ECS 化が進んだ他システムと構造が揃わない

関係箇所:
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Audio\Audio.h`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Audio\Audio.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\System\Framework.cpp`

### 2. runtime handle が gameplay 側へ漏れている

現在は `BGMComponent` や timeline 側が `std::shared_ptr<AudioSource>` を直接持っています。

- save/load に弱い
- prefab/undo と相性が悪い
- runtime state と authoring state が混ざっている

関係箇所:
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Audio\BGMComponent.h`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Audio\BGMComponent.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Timeline\TimelineCharacter.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Timeline\TimelineSequencerComponent.cpp`

### 3. WAV 手書き loader 依存

`AudioResource` は RIFF PCM 寄りの簡易 loader です。

- `.wav` 前提が強い
- `.ogg` / `.mp3` / `.flac` などの拡張がしづらい
- streaming や decode policy を育てにくい

関係箇所:
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Audio\AudioResource.cpp`

### 4. Windows / XAudio2 直結

XAudio2 自体は悪くありませんが、今のコードは低レベル API に直結しすぎています。

- resource management
- stream 管理
- bus / mixer
- asset import
- profiling / debug

をほぼ自前で持つ必要があります。


## 設計方針

### 1. 正は ECS

今後の authoring 情報は、すべて ECS component を正とします。

- editor は ECS component を編集する
- scene / prefab / autosave は ECS component を保存する
- runtime voice や backend handle は system 側の runtime-only state として保持する

### 2. runtime handle は component に保存しない

以下は ECS component に保存しません。

- `AudioSource*`
- `std::shared_ptr<AudioSource>`
- `IXAudio2SourceVoice*`
- 外部ライブラリの再生ハンドル

これらは system 内の map / runtime state で管理します。

### 3. 旧 `Audio` は bridge 扱い

旧 `Audio` singleton は移行期間の bridge としてのみ許可します。  
完成形では、ECS audio systems が正になります。


## 推奨 ECS component

### `AudioEmitterComponent`

用途:
- 2D / 3D 音源
- BGM
- loop 再生
- play on start

主な項目:
- `clipAssetPath`
- `playOnStart`
- `loop`
- `is3D`
- `volume`
- `pitch`
- `dopplerScale`
- `minDistance`
- `maxDistance`
- `spatialBlend`
- `busName`
- `priority`
- `streaming`
- `autoDestroyOnFinish`

### `AudioListenerComponent`

用途:
- 有効 listener の指定

主な項目:
- `isPrimary`
- `volumeScale`

備考:
- 実際の位置・向きは `TransformComponent` / active camera から取得

### `AudioOneShotRequestComponent`

用途:
- 一時的な再生要求
- UI click
- event trigger
- timeline trigger

主な項目:
- `clipAssetPath`
- `volume`
- `pitch`
- `is3D`
- `worldPosition`
- `busName`
- `lifetimeFrames`

備考:
- system が消費後に component を除去する

### `AudioBusSendComponent`

用途:
- entity を特定 bus に所属させる

主な項目:
- `busName`
- `sendVolume`

### `AudioStateComponent`（runtime-only）

用途:
- 再生状態の同期表示

主な項目:
- `isPlaying`
- `playbackTimeSec`
- `isVirtualized`
- `lastStartFrame`

備考:
- scene / prefab 保存対象外

### `AudioSettingsComponent`

用途:
- scene 単位の既定設定

主な項目:
- `masterVolume`
- `bgmVolume`
- `sfxVolume`
- `uiVolume`
- `muteAll`
- `debugDraw`


## 推奨 system 構成

### `AudioWorldSystem`

責務:
- backend 初期化/終了
- world 単位の音声更新
- master/bus volume 適用

### `AudioAssetSystem`

責務:
- `clipAssetPath` から audio asset を取得
- preload / streaming / cache 管理

### `AudioListenerExtractSystem`

責務:
- active listener を 1 つ決定
- transform から位置・向きを抽出

### `AudioEmitterSyncSystem`

責務:
- `AudioEmitterComponent` と runtime voice の差分同期
- position / volume / pitch / loop / play / stop を適用

### `AudioOneShotSystem`

責務:
- one-shot request を消費して再生
- 消費後に request component を削除

### `AudioTimelineBridgeSystem`

責務:
- 既存 timeline audio item を ECS request に変換
- 旧 `Audio::Play2D/Play3D` の直接呼び出しを段階的に置換

### `AudioCleanupSystem`

責務:
- 再生終了 voice の回収
- runtime state の整理

### `AudioDebugSystem`

責務:
- active voice 一覧
- bus 音量
- listener
- stream 数
- clip cache


## AudioClip asset 方針

現在はファイルパス直指定が多いですが、今後は `AudioClip` asset を正式導入します。

初期版の保存項目:

- `sourcePath`
- `importedPath`
- `streaming`
- `defaultVolume`
- `defaultPitch`
- `defaultLoop`
- `channelLayout`
- `sampleRate`
- `lengthSec`

初期対応フォーマット:

- `.wav`
- `.ogg`
- `.mp3`

将来候補:

- `.flac`


## 外部ライブラリ導入検討

### 候補A: XAudio2 継続

長所:

- 既に導入済み
- Windows では低レイテンシ
- 既存コードを一部流用できる

短所:

- 低レベル API のため、上位設計コストが大きい
- decoding / streaming / bus / asset 管理を自前で持つ必要がある
- Windows 依存が強い

結論:

- 移行 bridge としては有効
- 最終推奨の中心には置かない

参考:
- [Microsoft Learn: XAudio2 Introduction](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-introduction)

### 候補B: miniaudio

長所:

- 導入が軽い
- device / decoding / resource management / engine / node graph を持つ
- ECS から薄く包みやすい
- エンジン主導の設計を維持しやすい

短所:

- Wwise のような大型 authoring tool はない
- bus / event / mixer ルールは自前設計が必要

結論:

- **本仕様の第一推奨**
- 現在のエンジン文化と最も相性がよい

参考:
- [miniaudio Manual](https://miniaud.io/docs/manual/index.html)

### 候補C: Wwise / FMOD など middleware

長所:

- event authoring
- bus / state / bank / profiler
- audio designer workflow が強い

短所:

- 導入・運用コストが高い
- エンジン設計を middleware 主導に寄せやすい
- 現段階では少し重い

結論:

- 将来の別トラック候補
- 最初の ECS audio 基盤としては採用しない

参考:
- [Audiokinetic Wwise](https://www.audiokinetic.com/en/products/wwise/)


## 最終推奨

本仕様では以下を正式方針とします。

1. **第一推奨 backend は miniaudio**
2. **旧 XAudio2 実装は移行 bridge に限定**
3. **Wwise / FMOD は将来別仕様で検討**

つまり、

- ECS を正にする
- backend は miniaudio を第一候補にする
- 旧 `Audio` を段階的に薄くする

これが最も安全で、今のプロジェクトにも合っています。


## editor 要件

### Inspector

`AudioEmitterComponent` に対し、少なくとも以下を編集可能にします。

- clip asset
- 2D / 3D
- volume
- pitch
- loop
- play on start
- bus
- distance
- streaming

### Asset Browser

- `AudioClip` を `AssetType::Audio` として正式扱い
- preview 再生
- drag & drop で entity に audio emitter を付加可能

### Window > Audio

debug / profiler 用に `Window > Audio` を追加し、以下を表示します。

- active voices
- listener entity
- bus volume
- stream count
- cache 状態
- mute/solo/debug toggle


## timeline / gameplay 連携方針

### timeline

現在:

- `TimelineCharacter.cpp`
- `TimelineSequencerComponent.cpp`

が `Audio::Play2D/Play3D` を直接呼んでいます。

変更後:

- timeline は `AudioOneShotRequestComponent` か audio command を生成する
- runtime voice の保持は system 側のみ

### BGM

現在:

- `BGMComponent` が `std::shared_ptr<AudioSource>` を直接保持

変更後:

- `BGMComponent` は廃止、または `AudioEmitterComponent` に吸収
- preview 再生も editor audio request を使う


## 移行フェーズ

### Phase 1: backend abstraction

追加:

- `IAudioBackend`
- `IAudioClip`
- `IAudioVoice`

実装:

- `MiniaudioBackend`
- 暫定 `XAudio2Backend`

### Phase 2: ECS component/system 導入

追加:

- `AudioEmitterComponent`
- `AudioListenerComponent`
- `AudioOneShotRequestComponent`
- `AudioSettingsComponent`

### Phase 3: asset/import

- `AudioClip` asset 正式化
- `.wav/.ogg/.mp3` import
- Asset Browser preview

### Phase 4: editor integration

- inspector
- `Window > Audio`
- drag & drop
- scene/prefab/undo

### Phase 5: legacy bridge replacement

- `BGMComponent` 置換
- timeline の直接呼び出し置換
- `Framework` の `Audio::Instance()->...` 依存縮小

### Phase 6: legacy removal

- 旧 `Audio`
- 旧 `AudioResource`
- 旧 `AudioSource`

の削除、または compatibility layer 化


## 完了条件

以下を満たしたら完了扱いです。

- audio の正が ECS component/system になっている
- gameplay / editor / timeline が旧 `Audio::Play2D/Play3D` を直接呼ばない
- 2D / 3D / BGM / one-shot / loop が ECS 経由で動く
- scene / prefab / undo / autosave が audio authoring 状態を保持できる
- `AudioClip` asset が導入され、Asset Browser preview がある
- `Window > Audio` で debug/profiler が見える
- `miniaudio` を第一 backend とした再生経路が安定動作する
- 旧 XAudio2 実装が bridge か廃止のどちらかに整理されている


## 実装優先度

S:

- backend abstraction
- miniaudio 導入
- ECS emitter/listener/request
- timeline/BGM bridge

A:

- AudioClip asset
- Asset Browser preview
- `Window > Audio`
- bus / mixer 整理

B:

- occlusion
- reverb zone
- ducking
- authoring preset


## 結論

今回の最適解は、

- **音声の正を完全に ECS へ寄せる**
- **外部ライブラリは miniaudio を第一候補に採用する**
- **旧 XAudio2 実装は移行 bridge に限定する**

です。

この方針なら、

- 2D / UI
- title screen
- 3D spatial audio
- timeline
- prefab / scene

を同一ルールで育てられます。
