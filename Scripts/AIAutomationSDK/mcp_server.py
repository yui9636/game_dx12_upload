from __future__ import annotations

import json
import sys
from typing import Any, Dict, Mapping

try:
    from .engine_client import COMMANDS, DEFAULT_URL
    from .llm_bridge import EngineLLMBridge, ToolPolicy
except ImportError:
    from engine_client import COMMANDS, DEFAULT_URL
    from llm_bridge import EngineLLMBridge, ToolPolicy


def _read_message() -> Dict[str, Any] | None:
    line = sys.stdin.readline()
    if not line:
        return None
    line = line.strip()
    if not line:
        return {}
    return json.loads(line)


def _write(value: Mapping[str, Any]) -> None:
    sys.stdout.write(json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n")
    sys.stdout.flush()


class MyEngineMCPServer:
    """Minimal stdio MCP-style server exposing one guarded engine_command tool."""

    def __init__(self, url: str = DEFAULT_URL):
        self.bridge = EngineLLMBridge(url, policy=ToolPolicy.from_env())

    def handle(self, message: Mapping[str, Any]) -> Dict[str, Any]:
        method = str(message.get("method") or "")
        request_id = message.get("id")
        try:
            if method == "initialize":
                return self._result(request_id, {
                    "protocolVersion": "2024-11-05",
                    "serverInfo": {"name": "myengine-ai-automation", "version": "1.0.0"},
                    "capabilities": {"tools": {}},
                })
            if method == "tools/list":
                return self._result(request_id, {
                    "tools": [
                        {
                            "name": "engine_command",
                            "description": "Call a guarded MyEngine DX12 automation command.",
                            "inputSchema": self.bridge.tool_schema()["parameters"],
                        },
                        {
                            "name": "engine_commands",
                            "description": "List all automation command names exposed by the bridge.",
                            "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
                        },
                    ]
                })
            if method == "tools/call":
                params = message.get("params") or {}
                if not isinstance(params, Mapping):
                    raise ValueError("tools/call params must be an object")
                tool_name = str(params.get("name") or "")
                args = params.get("arguments") or {}
                if not isinstance(args, Mapping):
                    raise ValueError("tool arguments must be an object")
                if tool_name == "engine_commands":
                    return self._tool_result(request_id, {"commands": list(COMMANDS)})
                if tool_name == "engine_command":
                    return self._tool_result(request_id, self.bridge.call_from_tool_arguments(args))
                raise ValueError(f"Unknown tool: {tool_name}")
            if method == "notifications/initialized":
                return {"jsonrpc": "2.0"}
            return self._error(request_id, -32601, f"Unknown method: {method}")
        except Exception as exc:
            return self._error(request_id, -32000, str(exc))

    @staticmethod
    def _result(request_id: Any, result: Any) -> Dict[str, Any]:
        return {"jsonrpc": "2.0", "id": request_id, "result": result}

    @staticmethod
    def _tool_result(request_id: Any, value: Any) -> Dict[str, Any]:
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {
                "content": [{"type": "text", "text": json.dumps(value, ensure_ascii=False, indent=2)}],
                "isError": False,
            },
        }

    @staticmethod
    def _error(request_id: Any, code: int, message: str) -> Dict[str, Any]:
        return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}


def main() -> int:
    server = MyEngineMCPServer()
    while True:
        message = _read_message()
        if message is None:
            return 0
        if not message:
            continue
        response = server.handle(message)
        if response.get("id") is not None or "error" in response:
            _write(response)


if __name__ == "__main__":
    raise SystemExit(main())
