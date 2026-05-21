from __future__ import annotations

import json
import re
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, List, Mapping, Optional


ToolRegistry = Mapping[str, Callable[..., Any]]


class WorkflowValidationError(ValueError):
    pass


class WorkflowExecutionError(RuntimeError):
    pass


class WorkflowRunner:
    """Declarative Phase 4 workflow runner for high-level automation tools."""

    _INTERPOLATION = re.compile(r"\$\{([^}]+)\}")

    def __init__(self, registry: ToolRegistry):
        self.registry = registry

    def validate(self, workflow: Any) -> Dict[str, Any]:
        spec = self._normalize_workflow(workflow)
        errors: List[str] = []
        warnings: List[str] = []
        ids: set[str] = set()

        for phase, steps in self._phase_steps(spec).items():
            for index, step in enumerate(steps):
                if not isinstance(step, Mapping):
                    errors.append(f"{phase}[{index}] must be an object.")
                    continue
                step_id = str(step.get("id") or f"{phase}_{index}")
                if step_id in ids:
                    errors.append(f"Duplicate step id: {step_id}")
                ids.add(step_id)
                tool_name = str(step.get("tool") or step.get("name") or "")
                if not tool_name:
                    errors.append(f"{step_id}: tool is required.")
                elif tool_name not in self.registry:
                    errors.append(f"{step_id}: unknown tool {tool_name!r}.")
                args = step.get("args", {})
                if args is not None and not isinstance(args, Mapping):
                    errors.append(f"{step_id}: args must be an object.")
                for number_key in ("retries", "retryDelay"):
                    if number_key in step and not isinstance(step[number_key], (int, float)):
                        errors.append(f"{step_id}: {number_key} must be a number.")
                if "when" in step and step["when"] in ({}, []):
                    warnings.append(f"{step_id}: empty when condition always skips or passes ambiguously.")

        return {
            "ok": not errors,
            "errors": errors,
            "warnings": warnings,
            "stepCount": sum(len(steps) for steps in self._phase_steps(spec).values()),
            "tools": sorted(self.registry),
        }

    def run(
        self,
        workflow: Any,
        *,
        dry_run: bool = False,
        continue_on_error: bool = False,
        variables: Optional[Mapping[str, Any]] = None,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        spec = self._normalize_workflow(workflow)
        validation = self.validate(spec)
        if not validation["ok"]:
            raise WorkflowValidationError("; ".join(validation["errors"]))

        started = time.time()
        defaults = dict(spec.get("defaults", {})) if isinstance(spec.get("defaults"), Mapping) else {}
        context: Dict[str, Any] = {
            "vars": dict(spec.get("variables", {})) if isinstance(spec.get("variables"), Mapping) else {},
            "steps": {},
            "stepList": [],
            "last": None,
            "workflow": {
                "name": spec.get("name", "workflow"),
                "version": spec.get("version", 1),
                "dryRun": dry_run,
            },
        }
        if variables:
            context["vars"].update(dict(variables))

        report: Dict[str, Any] = {
            "name": context["workflow"]["name"],
            "version": context["workflow"]["version"],
            "dryRun": dry_run,
            "startedAt": self._timestamp(started),
            "validation": validation,
            "steps": [],
            "summary": {
                "ok": True,
                "executed": 0,
                "skipped": 0,
                "failed": 0,
            },
        }

        should_continue = bool(spec.get("continueOnError", continue_on_error) or continue_on_error)
        phases = self._phase_steps(spec)
        stopped = False
        for phase_name in ("steps", "verify", "checkpoint"):
            if stopped:
                break
            for index, raw_step in enumerate(phases.get(phase_name, [])):
                step_report = self._run_step(
                    raw_step,
                    phase=phase_name,
                    index=index,
                    context=context,
                    defaults=defaults,
                    dry_run=dry_run,
                )
                report["steps"].append(step_report)
                if step_report.get("skipped"):
                    report["summary"]["skipped"] += 1
                elif step_report.get("ok"):
                    report["summary"]["executed"] += 0 if dry_run else 1
                else:
                    report["summary"]["failed"] += 1
                    report["summary"]["ok"] = False
                    if not should_continue and not step_report.get("continueOnError"):
                        stopped = True
                        break

        finished = time.time()
        report["finishedAt"] = self._timestamp(finished)
        report["durationMs"] = int((finished - started) * 1000)
        report["variables"] = context["vars"]
        report["summary"]["ok"] = report["summary"]["ok"] and report["summary"]["failed"] == 0

        target_path = report_path or spec.get("reportPath")
        if target_path:
            resolved_report_path = str(self._resolve(target_path, context, strict=not dry_run))
            self.write_report(report, resolved_report_path, dry_run=dry_run)
            report["reportPath"] = resolved_report_path
        return report

    def write_report(self, report: Mapping[str, Any], path: str, *, dry_run: bool = False) -> None:
        if dry_run:
            return
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    def _run_step(
        self,
        step: Mapping[str, Any],
        *,
        phase: str,
        index: int,
        context: Dict[str, Any],
        defaults: Mapping[str, Any],
        dry_run: bool,
    ) -> Dict[str, Any]:
        step_id = str(step.get("id") or f"{phase}_{index}")
        tool_name = str(step.get("tool") or step.get("name") or "")
        raw_args = step.get("args", {})
        retries = int(step.get("retries", defaults.get("retries", 0)))
        retry_delay = float(step.get("retryDelay", defaults.get("retryDelay", 0.0)))
        continue_on_error = bool(step.get("continueOnError", defaults.get("continueOnError", False)))
        started = time.time()

        base_report: Dict[str, Any] = {
            "id": step_id,
            "phase": phase,
            "index": index,
            "tool": tool_name,
            "startedAt": self._timestamp(started),
            "continueOnError": continue_on_error,
        }

        try:
            when = step.get("when", True)
            if not self._evaluate_condition(when, context, strict=not dry_run):
                base_report.update({
                    "ok": True,
                    "skipped": True,
                    "reason": "when condition evaluated to false",
                    "finishedAt": self._timestamp(),
                    "durationMs": int((time.time() - started) * 1000),
                })
                context["steps"][step_id] = base_report
                context["stepList"].append(base_report)
                context["last"] = base_report
                return base_report

            args = self._resolve(raw_args, context, strict=not dry_run)
            if not isinstance(args, Mapping):
                raise WorkflowExecutionError(f"{step_id}: resolved args must be an object.")
            base_report["args"] = dict(args)
            base_report["attempts"] = []

            if dry_run:
                base_report.update({
                    "ok": True,
                    "dryRun": True,
                    "finishedAt": self._timestamp(),
                    "durationMs": int((time.time() - started) * 1000),
                })
                context["steps"][step_id] = base_report
                context["stepList"].append(base_report)
                context["last"] = base_report
                return base_report

            value = None
            last_error = None
            for attempt in range(retries + 1):
                attempt_started = time.time()
                try:
                    value = self.registry[tool_name](**dict(args))
                    base_report["attempts"].append({
                        "attempt": attempt + 1,
                        "ok": True,
                        "durationMs": int((time.time() - attempt_started) * 1000),
                    })
                    last_error = None
                    break
                except Exception as exc:
                    last_error = exc
                    base_report["attempts"].append({
                        "attempt": attempt + 1,
                        "ok": False,
                        "error": str(exc),
                        "durationMs": int((time.time() - attempt_started) * 1000),
                    })
                    if attempt < retries and retry_delay > 0.0:
                        time.sleep(retry_delay)
            if last_error is not None:
                raise last_error

            base_report["result"] = value
            context["steps"][step_id] = base_report
            context["stepList"].append(base_report)
            context["last"] = base_report
            save_as = step.get("saveAs")
            if isinstance(save_as, str) and save_as:
                context["vars"][save_as] = value

            expect = step.get("expect", True)
            if not self._evaluate_condition(expect, context, strict=True):
                raise WorkflowExecutionError(f"{step_id}: expect condition failed.")

            base_report.update({
                "ok": True,
                "finishedAt": self._timestamp(),
                "durationMs": int((time.time() - started) * 1000),
            })
            return base_report
        except Exception as exc:
            base_report.update({
                "ok": False,
                "error": str(exc),
                "finishedAt": self._timestamp(),
                "durationMs": int((time.time() - started) * 1000),
            })
            context["steps"][step_id] = base_report
            context["stepList"].append(base_report)
            context["last"] = base_report
            return base_report

    def _normalize_workflow(self, workflow: Any) -> Dict[str, Any]:
        if isinstance(workflow, list):
            return {"version": 1, "name": "workflow", "steps": workflow}
        if not isinstance(workflow, Mapping):
            raise WorkflowValidationError("Workflow must be an object or a list of steps.")
        spec = dict(workflow)
        if "steps" not in spec:
            raise WorkflowValidationError("Workflow object must contain a steps array.")
        if not isinstance(spec["steps"], list):
            raise WorkflowValidationError("Workflow steps must be an array.")
        return spec

    def _phase_steps(self, spec: Mapping[str, Any]) -> Dict[str, List[Mapping[str, Any]]]:
        phases: Dict[str, List[Mapping[str, Any]]] = {
            "steps": list(spec.get("steps", [])),
            "verify": [],
            "checkpoint": [],
        }
        verify = spec.get("verify", [])
        if isinstance(verify, list):
            phases["verify"] = [self._normalize_step(item, "workflow.verify") for item in verify]
        elif isinstance(verify, Mapping):
            phases["verify"] = [self._normalize_step(verify, "workflow.verify")]
        checkpoint = spec.get("checkpoint")
        if isinstance(checkpoint, Mapping):
            phases["checkpoint"] = [self._normalize_step(checkpoint, "workflow.checkpoint")]
        return phases

    def _normalize_step(self, step: Mapping[str, Any], default_tool: str) -> Mapping[str, Any]:
        if "tool" in step or "name" in step:
            return step
        return {"tool": default_tool, "args": dict(step)}

    def _resolve(self, value: Any, context: Mapping[str, Any], *, strict: bool) -> Any:
        if isinstance(value, str):
            exact = self._INTERPOLATION.fullmatch(value)
            if exact:
                return self._resolve_path(exact.group(1).strip(), context, strict=strict)

            if value.startswith("$") and not value.startswith("${") and len(value) > 1:
                return self._resolve_path(value[1:].strip(), context, strict=strict)

            def replace(match: re.Match[str]) -> str:
                resolved = self._resolve_path(match.group(1).strip(), context, strict=strict)
                return json.dumps(resolved, ensure_ascii=False) if isinstance(resolved, (dict, list)) else str(resolved)

            return self._INTERPOLATION.sub(replace, value)
        if isinstance(value, list):
            return [self._resolve(item, context, strict=strict) for item in value]
        if isinstance(value, Mapping):
            return {str(k): self._resolve(v, context, strict=strict) for k, v in value.items()}
        return value

    def _resolve_path(self, path: str, context: Mapping[str, Any], *, strict: bool) -> Any:
        current: Any = context
        for part in path.split("."):
            if part == "":
                continue
            if isinstance(current, Mapping) and part in current:
                current = current[part]
                continue
            if isinstance(current, list):
                try:
                    current = current[int(part)]
                    continue
                except (ValueError, IndexError):
                    pass
            if strict:
                raise WorkflowExecutionError(f"Unable to resolve workflow reference: {path}")
            return f"<unresolved:{path}>"
        return current

    def _evaluate_condition(self, condition: Any, context: Mapping[str, Any], *, strict: bool) -> bool:
        if condition is None:
            return True
        if isinstance(condition, bool):
            return condition
        if isinstance(condition, list):
            return all(self._evaluate_condition(item, context, strict=strict) for item in condition)
        if not isinstance(condition, Mapping):
            return bool(self._resolve(condition, context, strict=strict))

        if "all" in condition:
            return all(self._evaluate_condition(item, context, strict=strict) for item in condition["all"])
        if "any" in condition:
            return any(self._evaluate_condition(item, context, strict=strict) for item in condition["any"])
        if "not" in condition:
            return not self._evaluate_condition(condition["not"], context, strict=strict)
        if "exists" in condition:
            value = self._resolve(condition["exists"], context, strict=strict)
            return value is not None and value != "" and value != []
        if "equals" in condition:
            left, right = self._resolve_pair(condition["equals"], context, strict=strict)
            return left == right
        if "notEquals" in condition:
            left, right = self._resolve_pair(condition["notEquals"], context, strict=strict)
            return left != right
        if "contains" in condition:
            container, item = self._resolve_pair(condition["contains"], context, strict=strict)
            return item in container
        if "greaterOrEqual" in condition:
            left, right = self._resolve_pair(condition["greaterOrEqual"], context, strict=strict)
            return float(left) >= float(right)
        if "lessOrEqual" in condition:
            left, right = self._resolve_pair(condition["lessOrEqual"], context, strict=strict)
            return float(left) <= float(right)
        return bool(self._resolve(condition, context, strict=strict))

    def _resolve_pair(self, value: Any, context: Mapping[str, Any], *, strict: bool) -> tuple[Any, Any]:
        resolved = self._resolve(value, context, strict=strict)
        if not isinstance(resolved, list) or len(resolved) != 2:
            raise WorkflowExecutionError("Condition pair must resolve to a two-item list.")
        return resolved[0], resolved[1]

    def _timestamp(self, value: Optional[float] = None) -> str:
        dt = datetime.fromtimestamp(time.time() if value is None else value, timezone.utc)
        return dt.isoformat(timespec="milliseconds").replace("+00:00", "Z")
