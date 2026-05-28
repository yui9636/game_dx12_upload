# Automation API Test Spec Template

新規 API を追加した時は、最低 1 個の smoke test を書いて
`Scripts/AIAutomationSDK/tests/test_<namespace>.py` に置く。

## テストの 3 階層

| 階層             | 何を確認する                                          | 例                                      |
|------------------|------------------------------------------------------|-----------------------------------------|
| **smoke**        | エラーなく返ってくる / 必須 field がある             | `result["entities"] is list`            |
| **invariant**    | mutation 後に観測層で期待値を assert する             | HP -= 10 後に `ecs.field.get` で 90    |
| **scenario**     | 複数 API を組み合わせて 1 シナリオを通す             | Title→Battle→Result の遷移を踏破        |

## テンプレ

```python
# Scripts/AIAutomationSDK/tests/test_my_namespace.py
from engine_client import EngineClient

URL = "ws://127.0.0.1:9876"


def test_smoke_my_command():
    """smoke: command が status=ok を返し、必須 field を持つ。"""
    with EngineClient(URL) as c:
        r = c.command("my_namespace.my_command", {"foo": 42})
        assert r is not None
        assert "expected_field" in r
        assert isinstance(r["expected_field"], list)


def test_invariant_my_command_changes_state():
    """invariant: mutation 後に観測層で値を確認。"""
    with EngineClient(URL) as c:
        # 事前状態
        before = c.command("ecs.field.get", {
            "entity": "<target>", "component": "MyComp", "field": "value"
        })
        # mutation
        c.command("my_namespace.my_command", {"target": "<target>", "delta": 5})
        # 事後検証
        after = c.command("ecs.field.get", {
            "entity": "<target>", "component": "MyComp", "field": "value"
        })
        assert after["value"] == before["value"] + 5


def test_scenario_end_to_end():
    """scenario: 複数 API のシナリオ統合。失敗時は _visualState を見て判別する。"""
    with EngineClient(URL) as c:
        r = c.command("editor.mutate_and_assert", {
            "command": "my_namespace.my_command",
            "params": {"target": "<x>"},
            "assertions": [
                {"op": "eq", "lhs": {"ecs": {"entity": "<x>",
                  "component": "MyComp", "field": "value"}}, "rhs": 42},
                {"op": "ge", "lhs": {"ecs": {"entity": "<x>",
                  "component": "MyComp", "field": "hp"}}, "rhs": 1}
            ],
            "frameSync": 1,
        })
        assert r["mutation"]["ok"] is True
        assert r["assertions"]["summary"]["allPassed"] is True
```

## 観測層をフル活用 (MANDATE)

データ書き込み → screenshot → 目視 のループは **禁止**。代わりに:

| やりたい事                       | 使う API                                     |
|----------------------------------|---------------------------------------------|
| 値が変わったか                   | `ecs.field.get` / `ecs.field.watch.pull`    |
| イベントが発火したか             | `gameflow.events.pull` / `collision.events.pull` |
| Hit が起きているか               | `collision.is_hitting`                      |
| Mesh が描画されているか          | `render.queue.snapshot` (summary に出る)    |
| 文字が描画されているか           | `visual.find_text`                          |
| 特定 pixel が想定色か            | `visual.get_pixel_at_screen`                |
| シーン全体が想定構成か           | `session.assert_invariant` で複数 check 一括 |

## 落ちた時のデバッグ手順

1. `r["_visualState"]` の `entities` / `worldPositions` / `screenPositions` を見る
2. `r["hints"]` (effect_editor 等) があれば従う
3. `log.tail` で最新 50 行の log を見る (`category="Combat"` 等で絞る)
4. `gameflow.get_runtime_state` で current node / pending transitions を確認
5. 最後の手段として screenshot — ただし `visual.find_text` で済むなら撮らない
