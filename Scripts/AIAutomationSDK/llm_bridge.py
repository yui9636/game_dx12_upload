"""
============================================================================
AI AGENT MANDATE — READ BEFORE INVOKING ANY COMMAND
============================================================================
This bridge exposes the engine WebSocket API. The API contains a full
observation layer. Use it. Do not fall back to the
`list_entities + capture_screenshot + manual PNG inspection` loop.

Pick the smallest observation window that answers the current question:

  Question                         | Use this (NOT a screenshot)
  ---------------------------------+---------------------------------
  Is entity X on screen?           | visual.verify_entity_game_view
  Did mutating X change Y too?     | ecs.diff (snapshot, mutate, diff)
  Where are all the Players?       | ecs.query by component signature
  Did the save actually persist?   | ai_session.status (file backup log)
  What scene/mode are we in?       | get_engine_state
  Stream of changes over time?     | ecs.watch (enable once, then read)
  Bright/dark/empty render region? | visual.evaluate_capture
  Editor stuck/degraded?           | editor.recovery.get_state
  Need to undo a cascade?          | ai_session.rollback

Before any mutation cascade:
  1. ai_session.begin (if not already started)
  2. ecs.watch enable=true
  3. ecs.diff snapshot
  4. ... mutate ...
  5. ecs.diff against snapshot to verify ONLY intended changes happened
  6. visual.verify_entity_game_view to confirm rendering matches intent

See Scripts/AIAutomationSDK/README.md "MANDATE FOR AI AGENTS" section.
============================================================================
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, Mapping, Optional, Sequence

try:
    from .engine_client import COMMANDS, DEFAULT_URL, EngineClient
except ImportError:
    from engine_client import COMMANDS, DEFAULT_URL, EngineClient


DEFAULT_SAFE_DENY = {
    "asset_browser.delete",
}


@dataclass
class ToolPolicy:
    allow: Optional[set[str]] = None
    deny: Optional[set[str]] = None
    require_session: bool = True

    @classmethod
    def from_env(cls) -> "ToolPolicy":
        allow_text = os.environ.get("MYENGINE_AI_ALLOW_COMMANDS", "").strip()
        deny_text = os.environ.get("MYENGINE_AI_DENY_COMMANDS", "").strip()
        allow = {item.strip() for item in allow_text.split(",") if item.strip()} or None
        deny = set(DEFAULT_SAFE_DENY)
        deny.update(item.strip() for item in deny_text.split(",") if item.strip())
        require_session = os.environ.get("MYENGINE_AI_REQUIRE_SESSION", "1") not in {"0", "false", "False"}
        return cls(allow=allow, deny=deny, require_session=require_session)

    def check(self, command: str) -> None:
        if command not in COMMANDS:
            raise ValueError(f"Unknown engine command: {command}")
        if self.allow is not None and command not in self.allow:
            raise PermissionError(f"Command is not allowed by policy: {command}")
        if self.deny and command in self.deny:
            raise PermissionError(f"Command is denied by policy: {command}")


class EngineLLMBridge:
    """Small bridge that exposes the engine WebSocket API as one LLM tool."""

    def __init__(self, url: str = DEFAULT_URL, *, policy: Optional[ToolPolicy] = None):
        self.url = url
        self.policy = policy or ToolPolicy.from_env()

    def tool_schema(self) -> Dict[str, Any]:
        return {
            "type": "function",
            "name": "engine_command",
            "description": "Call a MyEngine DX12 AI automation command through the local WebSocket bridge.",
            "parameters": {
                "type": "object",
                "additionalProperties": False,
                "required": ["command"],
                "properties": {
                    "command": {
                        "type": "string",
                        "enum": list(COMMANDS),
                        "description": "Automation command name.",
                    },
                    "params": {
                        "type": "object",
                        "additionalProperties": True,
                        "description": "Command parameters.",
                    },
                    "timeout": {
                        "type": "number",
                        "minimum": 0.1,
                        "maximum": 300.0,
                        "description": "Optional per-command timeout in seconds.",
                    },
                },
            },
        }

    def call(self, command: str, params: Optional[Mapping[str, Any]] = None, *, timeout: Optional[float] = None) -> Any:
        self.policy.check(command)
        with EngineClient(self.url, timeout=timeout or float(os.environ.get("MYENGINE_AI_TIMEOUT", "30"))) as engine:
            if self.policy.require_session and not command.startswith("ai_session."):
                status = engine.ai_session_status()
                if not isinstance(status, Mapping) or not status.get("active", False):
                    raise PermissionError("AI session is required. Call ai_session.begin first.")
            return engine.command(command, params or {}, timeout=timeout)

    def call_from_tool_arguments(self, arguments: Mapping[str, Any]) -> Any:
        return self.call(
            str(arguments.get("command") or ""),
            arguments.get("params") if isinstance(arguments.get("params"), Mapping) else {},
            timeout=float(arguments["timeout"]) if "timeout" in arguments else None,
        )

    def export_schema(self, path: str) -> Dict[str, Any]:
        schema = self.tool_schema()
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(schema, ensure_ascii=False, indent=2), encoding="utf-8")
        return {"path": str(target), "schema": schema}


def main(argv: Optional[Sequence[str]] = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Expose MyEngine automation commands to LLM tool callers.")
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--schema", action="store_true", help="Print the engine_command tool schema.")
    parser.add_argument("--export-schema", help="Write the tool schema JSON to a path.")
    parser.add_argument("--command", help="Run one engine command.")
    parser.add_argument("--params", default="{}", help="JSON object parameters for --command.")
    parser.add_argument("--timeout", type=float)
    args = parser.parse_args(argv)

    bridge = EngineLLMBridge(args.url)
    if args.export_schema:
        print(json.dumps(bridge.export_schema(args.export_schema), ensure_ascii=False, indent=2))
        return 0
    if args.schema or not args.command:
        print(json.dumps(bridge.tool_schema(), ensure_ascii=False, indent=2))
        return 0
    params = json.loads(args.params)
    if not isinstance(params, Mapping):
        raise SystemExit("--params must be a JSON object")
    print(json.dumps(bridge.call(args.command, params, timeout=args.timeout), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
