from __future__ import annotations

import ast
import json
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Mapping, Optional

try:
    from .engine_client import COMMANDS, DEFAULT_URL, EngineClient
    from .tools import EngineTools
except ImportError:
    from engine_client import COMMANDS, DEFAULT_URL, EngineClient
    from tools import EngineTools


class QualityGateRunner:
    """Phase 7 safety and regression gate for AI authoring infrastructure."""

    def __init__(self, project_root: Optional[Path] = None, url: str = DEFAULT_URL):
        self.project_root = Path.cwd() if project_root is None else Path(project_root)
        self.url = url

    def run(
        self,
        *,
        require_engine: bool = False,
        run_python_compile: bool = True,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        started = time.time()
        report: Dict[str, Any] = {
            "name": "ai_foundation_quality_gate",
            "startedAt": self._timestamp(started),
            "checks": [],
            "summary": {"ok": True, "passed": 0, "failed": 0, "skipped": 0},
        }

        self._record(report, "command_sync", self._check_command_sync)
        if run_python_compile:
            self._record(report, "python_compile", self._check_python_compile)
        else:
            self._skip(report, "python_compile")

        if require_engine:
            self._record(report, "engine_smoke", self._check_engine_smoke)
            self._record(report, "coverage", self._check_coverage)
            self._record(report, "recovery_api", self._check_recovery_api)
        else:
            self._skip(report, "engine_smoke")
            self._skip(report, "coverage")
            self._skip(report, "recovery_api")

        finished = time.time()
        report["finishedAt"] = self._timestamp(finished)
        report["durationMs"] = int((finished - started) * 1000)
        report["summary"]["ok"] = report["summary"]["failed"] == 0

        if report_path:
            target = self.project_root / report_path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            report["reportPath"] = report_path
        return report

    def _record(self, report: Dict[str, Any], check_id: str, fn: Any) -> None:
        started = time.time()
        try:
            details = fn()
            ok = bool(details.get("ok", True))
            item = {
                "id": check_id,
                "ok": ok,
                "details": details,
                "durationMs": int((time.time() - started) * 1000),
            }
        except Exception as exc:
            ok = False
            item = {
                "id": check_id,
                "ok": False,
                "error": str(exc),
                "durationMs": int((time.time() - started) * 1000),
            }
        report["checks"].append(item)
        if ok:
            report["summary"]["passed"] += 1
        else:
            report["summary"]["failed"] += 1

    def _skip(self, report: Dict[str, Any], check_id: str) -> None:
        report["checks"].append({"id": check_id, "ok": True, "skipped": True})
        report["summary"]["skipped"] += 1

    def _check_command_sync(self) -> Dict[str, Any]:
        cpp_path = self.project_root / "Source/Automation/AIAutomationService.cpp"
        py_path = self.project_root / "Scripts/AIAutomationSDK/engine_client.py"
        cpp = cpp_path.read_text(encoding="utf-8", errors="ignore")
        cpp_commands = sorted(set(re.findall(r'name\s*==\s*"([^"]+)"', cpp)))
        py = py_path.read_text(encoding="utf-8")
        block = re.search(r"COMMANDS:\s*List\[str\]\s*=\s*(\[.*?\])\n\n", py, re.S)
        if not block:
            raise RuntimeError("Could not locate COMMANDS in engine_client.py")
        py_commands = list(ast.literal_eval(block.group(1)))
        missing = [command for command in cpp_commands if command not in py_commands]
        extra = [command for command in py_commands if command not in cpp_commands]
        return {
            "ok": not missing and not extra,
            "cppCount": len(cpp_commands),
            "sdkCount": len(py_commands),
            "missingInSdk": missing,
            "extraInSdk": extra,
        }

    def _check_python_compile(self) -> Dict[str, Any]:
        files = [
            "Scripts/AIAutomationSDK/engine_client.py",
            "Scripts/AIAutomationSDK/tools.py",
            "Scripts/AIAutomationSDK/workflow_runner.py",
            "Scripts/AIAutomationSDK/verification_runner.py",
            "Scripts/AIAutomationSDK/autonomy_runner.py",
            "Scripts/AIAutomationSDK/llm_bridge.py",
            "Scripts/AIAutomationSDK/mcp_server.py",
            "Scripts/AIAutomationSDK/qa_runner.py",
        ]
        proc = subprocess.run(
            [sys.executable, "-m", "py_compile", *files],
            cwd=self.project_root,
            text=True,
            capture_output=True,
        )
        return {
            "ok": proc.returncode == 0,
            "returncode": proc.returncode,
            "stdout": proc.stdout,
            "stderr": proc.stderr,
        }

    def _check_engine_smoke(self) -> Dict[str, Any]:
        with EngineClient(self.url, timeout=10) as engine:
            ping = engine.ping()
            state = engine.get_engine_state()
        return {"ok": True, "ping": ping, "mode": state.get("mode")}

    def _check_coverage(self) -> Dict[str, Any]:
        with EngineClient(self.url, timeout=10) as engine:
            coverage = EngineTools(engine).coverage_report()
        return {"ok": bool(coverage.get("phase3Complete", False)), "coverage": coverage}

    def _check_recovery_api(self) -> Dict[str, Any]:
        with EngineClient(self.url, timeout=10) as engine:
            state = EngineTools(engine).editor.recovery_state(refresh=True)
        return {"ok": "hasCandidate" in state, "state": state}

    @staticmethod
    def _timestamp(value: Optional[float] = None) -> str:
        return datetime.fromtimestamp(time.time() if value is None else value, tz=timezone.utc).isoformat()


def main(argv: Optional[list[str]] = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Run AI foundation quality gates.")
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--require-engine", action="store_true")
    parser.add_argument("--skip-python-compile", action="store_true")
    parser.add_argument("--report", default="Saved/AI/qa/ai_foundation_quality_gate_latest.json")
    args = parser.parse_args(argv)

    report = QualityGateRunner(url=args.url).run(
        require_engine=args.require_engine,
        run_python_compile=not args.skip_python_compile,
        report_path=args.report,
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report.get("summary", {}).get("ok", False) else 2


if __name__ == "__main__":
    raise SystemExit(main())
