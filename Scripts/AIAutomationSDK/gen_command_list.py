#!/usr/bin/env python3
"""
AIAutomationService.cpp の DispatchCommand 中の `if (name == "foo.bar")` を全抽出し、
engine_client.py の COMMANDS リストと突き合わせる。

Usage:
    python gen_command_list.py                # diff のみ表示
    python gen_command_list.py --emit-stubs   # 不足分の Python COMMANDS 追加 snippet を吐く
    python gen_command_list.py --check        # 不足がある場合 exit 1 (CI 用)
"""
from __future__ import annotations
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / "Source" / "Automation" / "AIAutomationService.cpp"
PY  = ROOT / "Scripts" / "AIAutomationSDK" / "engine_client.py"

CPP_PATTERN = re.compile(r'name\s*==\s*"([a-zA-Z_][a-zA-Z0-9_.]*)"')
PY_LIST_PATTERN = re.compile(r'COMMANDS:\s*List\[str\]\s*=\s*\[(.*?)\]', re.DOTALL)


def extract_cpp_commands() -> set[str]:
    if not CPP.exists():
        print(f"[gen] missing C++ source: {CPP}", file=sys.stderr)
        sys.exit(2)
    text = CPP.read_text(encoding="utf-8", errors="replace")
    return set(CPP_PATTERN.findall(text))


def extract_py_commands() -> set[str]:
    if not PY.exists():
        print(f"[gen] missing Python SDK: {PY}", file=sys.stderr)
        sys.exit(2)
    text = PY.read_text(encoding="utf-8")
    m = PY_LIST_PATTERN.search(text)
    if not m:
        print("[gen] could not find COMMANDS list in engine_client.py", file=sys.stderr)
        sys.exit(2)
    body = m.group(1)
    return set(re.findall(r'"([a-zA-Z_][a-zA-Z0-9_.]*)"', body))


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--emit-stubs", action="store_true", help="print Python COMMANDS additions")
    p.add_argument("--check", action="store_true", help="exit 1 if Python list is missing entries")
    args = p.parse_args()

    cpp = extract_cpp_commands()
    py = extract_py_commands()

    missing_in_py = sorted(cpp - py)
    stale_in_py = sorted(py - cpp)

    print(f"[gen] CPP commands found:    {len(cpp)}")
    print(f"[gen] Python COMMANDS:       {len(py)}")
    print(f"[gen] Missing in Python:     {len(missing_in_py)}")
    print(f"[gen] Stale in Python (not in C++): {len(stale_in_py)}")

    if missing_in_py:
        print("\n--- Missing in Python COMMANDS ---")
        for c in missing_in_py:
            print(f"  {c!r}")
        if args.emit_stubs:
            print("\n--- Snippet to paste into engine_client.py ---")
            for c in missing_in_py:
                print(f'    "{c}",')

    if stale_in_py:
        print("\n--- Stale in Python (not found in C++) ---")
        for c in stale_in_py:
            print(f"  {c!r}")

    if args.check and missing_in_py:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
