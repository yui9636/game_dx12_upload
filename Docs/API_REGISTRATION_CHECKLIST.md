# 新規 Automation API 追加チェックリスト

新しい WebSocket コマンドを追加した場合、以下を **全部** 更新しないと
Python SDK / AI agent から見えない or `Unknown automation command` で reject される。

## 1. C++ 側 (`Source/Automation/AIAutomationService.cpp`)

### 1.1 ハンドラ関数を実装
```cpp
json HandleMyNewCommand(EngineKernel& kernel, Registry& registry, const json& params)
{
    // ...
    return { { "ok", true }, /* fields */ };
}
```

### 1.2 `DispatchCommand` に分岐を追加
```cpp
if (name == "my_namespace.my_new_command") {
    return HandleMyNewCommand(kernel, *registry, params);
}
```

### 1.3 (推奨) `BuildAutomationManifest` の `commandReference` に項目追加
```cpp
{ "my_namespace.my_new_command", { { "params", "<arg list>" }, { "mutates", true|false } } },
```

### 1.4 (推奨) `IsMutationCommandForAutoVisual` に追加 (mutation の場合)
`HandleEditorMutateAndAssert` が `_visualState` を自動添付するためのリスト。

## 2. Python SDK 側 (`Scripts/AIAutomationSDK/engine_client.py`)

### 2.1 `COMMANDS` リストに追加 (必須)
```python
COMMANDS: List[str] = [
    ...
    "my_namespace.my_new_command",
    ...
]
```
ここに無いと `EngineClient.command(...)` が `ValueError: Unknown automation command` を投げる。

### 2.2 (読み取り中心の冪等系の場合) `SAFE_RETRY_COMMANDS` にも追加
WebSocket 切断後の自動 retry を許可する。

### 2.3 (任意) `COMMAND_METHODS` map にメソッドエイリアスを追加
Python から `client.my_new_command(...)` で呼べるようにする。

## 3. テスト (`Scripts/AIAutomationSDK/tests/` か手動)

最低 1 個の smoke test を追加すること。テンプレートは
`Docs/API_TEST_SPEC_TEMPLATE.md` を参照。

## 4. ビルド & WS reconnect

C++ 変更後は engine の再ビルド・再起動が必須。
変更内容によっては `Generated/ComponentMeta.generated.h` 等の codegen も再走させる。

## 5. ハマりやすい落とし穴

| 症状                                              | 原因                                                 |
|---------------------------------------------------|------------------------------------------------------|
| `ValueError: Unknown automation command`          | Python `COMMANDS` リストに未追加                    |
| 接続後 `error: unknown_command`                   | C++ `DispatchCommand` の `if` 分岐未追加            |
| 動くがレスポンスが薄い                            | `_visualState` 自動添付が効く mutation 系なら IsMutationCommandForAutoVisual に登録不足 |
| `EngineConnectionError: Connection lost`         | 重い command が timeout。`HEAVY_COMMAND_TIMEOUTS` を確認 |
| field write が黙って消える                        | `ComponentMeta.generated.h` の Fields tuple に該当 field がない |

## 6. (将来) 自動化案

C++ → Python の `COMMANDS` リストを generate するスクリプトを Scripts/ 配下に
`gen_command_list.py` として作る予定。`DispatchCommand` の string literal を
正規表現で抜き、`engine_client.py` の `COMMANDS` をオーバーライドする。
現状は手動 sync。
