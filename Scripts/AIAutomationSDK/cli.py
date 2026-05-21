from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, Mapping, Optional

from engine_client import COMMAND_METHODS, COMMANDS, DEFAULT_TIMEOUT, DEFAULT_URL, EngineClient
from engine_client import EngineCommandError, EngineConnectionError


def load_json_arg(text: Optional[str]) -> Dict[str, Any]:
    if not text:
        return {}
    value = json.loads(text)
    if not isinstance(value, dict):
        raise ValueError("--params must decode to a JSON object.")
    return value


def load_payload_from_file(path: Path) -> Dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("Command file must contain a JSON object.")
    return value


def print_json(value: Any) -> None:
    print(json.dumps(value, ensure_ascii=False, indent=2))


def run_command(args: argparse.Namespace) -> int:
    params = load_json_arg(args.params)
    command_id = args.id
    command_name = args.name

    if args.file:
        payload = load_payload_from_file(Path(args.file))
        command_name = str(payload.get("command") or command_name)
        command_id = str(payload.get("id") or command_id) if (payload.get("id") or command_id) else None
        file_params = payload.get("params") or {}
        if not isinstance(file_params, Mapping):
            raise ValueError("Command file params must be a JSON object.")
        merged = dict(file_params)
        merged.update(params)
        params = merged

    with EngineClient(args.url, timeout=args.timeout) as engine:
        result = engine.command(
            command_name,
            params,
            command_id=command_id,
            raw_response=args.raw_response,
            timeout=args.timeout,
        )
    print_json(result)
    return 0


def run_smoke(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        results = {
            "ping": engine.ping(raw_response=args.raw_response),
            "get_engine_state": engine.get_engine_state(raw_response=args.raw_response),
            "list_entities": engine.list_entities(raw_response=args.raw_response),
        }
    print_json(results)
    return 0


def run_methods(_: argparse.Namespace) -> int:
    print_json({
        "commandCount": len(COMMANDS),
        "commands": COMMANDS,
        "methods": COMMAND_METHODS,
    })
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Client CLI for the MyEngine AI automation WebSocket server.",
    )
    parser.add_argument("--url", default=DEFAULT_URL, help=f"WebSocket URL. Default: {DEFAULT_URL}")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="Socket timeout in seconds.")

    subparsers = parser.add_subparsers(dest="subcommand", required=True)

    command_parser = subparsers.add_parser("command", help="Send one automation command.")
    command_parser.add_argument("name", nargs="?", default="ping", help="Automation command name.")
    command_parser.add_argument("--params", help="JSON object to send as params.")
    command_parser.add_argument("--file", help="Path to a command JSON file. --params overrides file params.")
    command_parser.add_argument("--id", help="Optional command id.")
    command_parser.add_argument("--raw-response", action="store_true", help="Print the full response envelope.")
    command_parser.set_defaults(func=run_command)

    smoke_parser = subparsers.add_parser("smoke", help="Run a small read-only command sequence.")
    smoke_parser.add_argument("--raw-response", action="store_true", help="Print full response envelopes.")
    smoke_parser.set_defaults(func=run_smoke)

    methods_parser = subparsers.add_parser("methods", help="Print available commands and SDK method names.")
    methods_parser.set_defaults(func=run_methods)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.func(args))
    except EngineCommandError as exc:
        print_json(exc.response)
        return 2
    except (EngineConnectionError, OSError) as exc:
        print(f"Connection failed: {exc}", file=sys.stderr)
        return 3
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
