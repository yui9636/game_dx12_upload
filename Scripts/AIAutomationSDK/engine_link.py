from __future__ import annotations

import json
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional

try:
    from .world_model import ECSObserver, WorldModel, diff_snapshots
except ImportError:
    from world_model import ECSObserver, WorldModel, diff_snapshots


class EngineLinkValidationError(ValueError):
    pass


def _timestamp(value: Optional[float] = None) -> str:
    dt = datetime.fromtimestamp(time.time() if value is None else value, timezone.utc)
    return dt.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _project_path(project_root: Path, path: str) -> Path:
    target = Path(path)
    return target if target.is_absolute() else project_root / target


def _summarize_world(snapshot: Optional[Mapping[str, Any]]) -> Dict[str, Any]:
    if not snapshot:
        return {"available": False}
    world = WorldModel(snapshot)
    summary = world.semantic_summary()
    role_counts = {
        role: payload.get("count", 0)
        for role, payload in summary.get("roles", {}).items()
        if isinstance(payload, Mapping)
    }
    return {
        "available": True,
        "entityCount": summary.get("entityCount", 0),
        "roleCounts": role_counts,
        "tags": summary.get("tags", {}),
    }


class EngineAffordanceMap:
    """Phase 10: AI-facing capability and intent map over the live engine/world."""

    def __init__(self, tools: Any):
        self.tools = tools

    def build(self, snapshot: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
        registry = self.tools.workflow.registry()
        coverage = self.tools.coverage_report()
        world_summary = _summarize_world(snapshot)
        command_names = set(registry.keys())

        affordances = [
            self._affordance(
                "observe_world",
                "Observe current ECS entities, components, visual state, and semantic roles.",
                ["world.snapshot", "world.semantic_summary"],
                ["world_snapshot", "semantic_summary"],
                command_names,
                always=True,
            ),
            self._affordance(
                "validate_gameplay",
                "Validate whether the world contains a playable loop.",
                ["world.validate_gameplay"],
                ["validation_report"],
                command_names,
                ready=bool(world_summary.get("available")),
            ),
            self._affordance(
                "author_collect_game",
                "Create a visible collect-the-coins scene using high-level autonomy.",
                ["world.make_collect_game", "autonomy.make_simple_game"],
                ["scene", "screenshot", "autonomy_report", "world_report"],
                command_names,
                always=True,
            ),
            self._affordance(
                "repair_gameplay",
                "Create missing gameplay elements from a failed world validation.",
                ["world.repair_plan", "workflow.run", "workflow.checkpoint"],
                ["repair_workflow", "diff", "validation_report"],
                command_names,
                ready=bool(world_summary.get("available")),
            ),
            self._affordance(
                "frame_and_capture",
                "Focus a semantic target and capture the Scene View.",
                ["entity.focus", "editor.capture"],
                ["screenshot", "visual_state"],
                command_names,
                ready=bool(world_summary.get("available")),
            ),
        ]

        return {
            "phase": "10_affordance_map",
            "createdAt": _timestamp(),
            "coverage": coverage,
            "registrySize": len(registry),
            "world": world_summary,
            "affordances": affordances,
            "intents": {
                "inspect": {"affordance": "observe_world"},
                "validate": {"affordance": "validate_gameplay"},
                "collect_game": {"affordance": "author_collect_game"},
                "repair": {"affordance": "repair_gameplay"},
                "capture": {"affordance": "frame_and_capture"},
            },
            "ready": all(item.get("available", False) for item in affordances if item.get("required", True)),
        }

    def _affordance(
        self,
        name: str,
        description: str,
        tools: List[str],
        outputs: List[str],
        command_names: set[str],
        *,
        always: bool = False,
        ready: bool = False,
    ) -> Dict[str, Any]:
        missing = [tool for tool in tools if tool not in command_names and tool != "workflow.run"]
        available = not missing and (always or ready)
        return {
            "name": name,
            "description": description,
            "tools": tools,
            "outputs": outputs,
            "available": available,
            "missingTools": missing,
            "required": True,
        }


class IntentPlanner:
    """Phase 10: compile a user/AI goal into an explicit engine intent plan."""

    def __init__(self, tools: Any):
        self.tools = tools

    def plan(self, goal: Mapping[str, Any], snapshot: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
        spec = self._normalize_goal(goal)
        intent = str(spec.get("intent") or self._infer_intent(spec)).lower()
        variables = dict(spec.get("variables", {})) if isinstance(spec.get("variables"), Mapping) else {}
        game_name = str(variables.get("gameName") or spec.get("gameName") or "AI_LinkedGame")
        coin_count = int(variables.get("coinCount", spec.get("coinCount", 5)))
        hazard_count = int(variables.get("hazardCount", spec.get("hazardCount", 3)))
        affordance_map = EngineAffordanceMap(self.tools).build(snapshot)

        if intent in {"collect", "collect_game", "make_game", "game"}:
            steps = [
                {"id": "observe_before", "kind": "observe", "tool": "world.snapshot"},
                {
                    "id": "author_world",
                    "kind": "author",
                    "tool": "world.make_collect_game",
                    "args": {
                        "game_name": game_name,
                        "coin_count": coin_count,
                        "hazard_count": hazard_count,
                        "max_repair_attempts": int(spec.get("maxRepairAttempts", 1)),
                    },
                },
                {"id": "observe_after", "kind": "observe", "tool": "world.snapshot"},
                {"id": "diff_world", "kind": "diff", "tool": "world.diff"},
                {
                    "id": "validate_world",
                    "kind": "validate",
                    "tool": "world.validate_gameplay",
                    "args": {
                        "min_collectibles": coin_count,
                        "min_hazards": hazard_count,
                        "require_terrain": True,
                        "require_light": True,
                    },
                },
            ]
        elif intent in {"inspect", "observe", "snapshot"}:
            steps = [
                {"id": "observe", "kind": "observe", "tool": "world.snapshot"},
                {"id": "summarize", "kind": "summarize", "tool": "world.semantic_summary"},
            ]
        elif intent in {"validate", "verify"}:
            steps = [
                {"id": "observe", "kind": "observe", "tool": "world.snapshot"},
                {
                    "id": "validate_world",
                    "kind": "validate",
                    "tool": "world.validate_gameplay",
                    "args": {
                        "min_collectibles": coin_count,
                        "min_hazards": hazard_count,
                        "require_terrain": bool(spec.get("requireTerrain", True)),
                        "require_light": bool(spec.get("requireLight", True)),
                    },
                },
            ]
        else:
            raise EngineLinkValidationError(f"Unsupported intent: {intent}")

        return {
            "phase": "10_intent_plan",
            "createdAt": _timestamp(),
            "name": spec.get("name", f"{game_name}_intent"),
            "goal": spec.get("goal", ""),
            "intent": intent,
            "variables": {
                "gameName": game_name,
                "coinCount": coin_count,
                "hazardCount": hazard_count,
            },
            "affordanceMap": affordance_map,
            "steps": steps,
        }

    def _normalize_goal(self, goal: Mapping[str, Any]) -> Dict[str, Any]:
        if not isinstance(goal, Mapping):
            raise EngineLinkValidationError("Engine link goal must be an object.")
        spec = dict(goal)
        spec.setdefault("name", "engine_link_goal")
        spec.setdefault("goal", "Use the AI-engine link to inspect, author, and validate the scene.")
        spec.setdefault("variables", {})
        return spec

    def _infer_intent(self, spec: Mapping[str, Any]) -> str:
        text = f"{spec.get('name', '')} {spec.get('goal', '')} {spec.get('template', '')}".lower()
        if any(word in text for word in ("collect", "coin", "game", "scene", "author", "create")):
            return "collect_game"
        if any(word in text for word in ("validate", "verify", "check")):
            return "validate"
        return "inspect"


class EngineSessionJournal:
    """Phase 11: persistent AI-engine session timeline."""

    def __init__(self, name: str, *, project_root: Optional[Path] = None):
        self.name = name
        self.project_root = Path.cwd() if project_root is None else Path(project_root)
        self.started = time.time()
        self.events: List[Dict[str, Any]] = []

    def record(self, kind: str, payload: Mapping[str, Any]) -> Dict[str, Any]:
        event = {
            "index": len(self.events),
            "time": _timestamp(),
            "kind": kind,
            "payload": dict(payload),
        }
        self.events.append(event)
        return event

    def report(self, *, summary: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
        return {
            "phase": "11_session_journal",
            "name": self.name,
            "startedAt": _timestamp(self.started),
            "finishedAt": _timestamp(),
            "durationMs": int((time.time() - self.started) * 1000),
            "summary": dict(summary or {}),
            "events": self.events,
        }

    def write(self, path: str, *, summary: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
        report = self.report(summary=summary)
        target = _project_path(self.project_root, path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        report["reportPath"] = path
        return report


class EngineLinkRunner:
    """Phase 12: goal-level AI-engine orchestration using affordances, journal, world model, and repair."""

    def __init__(self, tools: Any, project_root: Optional[Path] = None):
        self.tools = tools
        self.project_root = Path.cwd() if project_root is None else Path(project_root)
        self.observer = ECSObserver(tools, self.project_root)
        self.planner = IntentPlanner(tools)

    def run(
        self,
        goal: Mapping[str, Any],
        *,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        spec = self._normalize_goal(goal)
        variables = dict(spec.get("variables", {})) if isinstance(spec.get("variables"), Mapping) else {}
        game_name = str(variables.get("gameName") or spec.get("gameName") or "AI_LinkedGame")
        report_path = report_path or str(spec.get("reportPath") or f"Saved/AI/link/{game_name}_link_latest.json")
        journal_path = str(spec.get("journalPath") or f"Saved/AI/link/{game_name}_journal_latest.json")
        started = time.time()
        journal = EngineSessionJournal(f"{game_name}_session", project_root=self.project_root)

        report: Dict[str, Any] = {
            "phase": "12_engine_link_orchestration",
            "name": spec.get("name", f"{game_name}_link_goal"),
            "goal": spec.get("goal", ""),
            "dryRun": dry_run,
            "startedAt": _timestamp(started),
            "summary": {"ok": False, "planOk": False, "executionOk": False, "worldOk": False, "repairAttempts": 0},
            "iterations": [],
        }

        before = None
        if not dry_run:
            before = self.observer.snapshot(include_details=True)
            journal.record("snapshot.before", {"summary": _summarize_world(before), "snapshot": before})

        plan = self.planner.plan(spec, before)
        journal.record("plan", plan)
        report["plan"] = plan

        if dry_run:
            report["summary"].update({"ok": True, "planOk": True, "executionOk": True, "worldOk": True})
            journal_report = journal.write(journal_path, summary=report["summary"])
            report["journal"] = journal_report
            return self._finish(report, started, report_path, dry_run=True)

        execution: Dict[str, Any] = {}
        world_validation: Dict[str, Any] = {}
        after = before
        intent = str(plan.get("intent") or "")
        if intent in {"collect", "collect_game", "make_game", "game"}:
            args = self._step_args(plan, "author_world")
            execution = self.tools.world.make_collect_game(
                game_name=str(args.get("game_name") or game_name),
                coin_count=int(args.get("coin_count", 5)),
                hazard_count=int(args.get("hazard_count", 3)),
                dry_run=False,
                max_repair_attempts=int(args.get("max_repair_attempts", max_repair_attempts)),
            )
            journal.record("execution.author_world", {"summary": execution.get("summary", {}), "result": execution})
            after = self.observer.snapshot(include_details=True)
            world_diff = diff_snapshots(before or {"entities": []}, after)
            world_validation = WorldModel(after).validate_gameplay_loop(
                style="collect",
                min_collectibles=int(args.get("coin_count", 5)),
                min_hazards=int(args.get("hazard_count", 3)),
                require_terrain=True,
                require_light=True,
            )
            journal.record("snapshot.after", {"summary": _summarize_world(after), "snapshot": after})
            journal.record("diff", world_diff)
            journal.record("validation", world_validation)
            report["iterations"].append({
                "kind": "primary",
                "execution": execution,
                "after": after,
                "diff": world_diff,
                "worldValidation": world_validation,
            })
        elif intent in {"inspect", "observe", "snapshot"}:
            after = self.observer.snapshot(include_details=True)
            execution = {"summary": {"ok": True}, "snapshot": after}
            world_validation = {"summary": {"ok": True}}
            journal.record("snapshot.inspect", {"summary": _summarize_world(after), "snapshot": after})
            report["iterations"].append({"kind": "inspect", "snapshot": after})
        elif intent in {"validate", "verify"}:
            after = self.observer.snapshot(include_details=True)
            args = self._step_args(plan, "validate_world")
            world_validation = WorldModel(after).validate_gameplay_loop(**args)
            execution = {"summary": {"ok": bool(world_validation.get("summary", {}).get("ok", False))}}
            journal.record("validation", world_validation)
            report["iterations"].append({"kind": "validate", "snapshot": after, "worldValidation": world_validation})

        execution_ok = bool(execution.get("summary", {}).get("ok", False))
        world_ok = bool(world_validation.get("summary", {}).get("ok", False))
        repair_attempts = 0

        while not world_ok and repair_attempts < max_repair_attempts and after is not None:
            repair_attempts += 1
            repair_plan = WorldModel(after).repair_plan(
                world_validation,
                game_name=game_name,
                coin_count=int(variables.get("coinCount", 5)),
                hazard_count=int(variables.get("hazardCount", 3)),
            )
            repair = self.tools.workflow.execute_workflow(repair_plan, continue_on_error=True)
            repaired = self.observer.snapshot(include_details=True)
            repair_diff = diff_snapshots(after, repaired)
            world_validation = WorldModel(repaired).validate_gameplay_loop(
                style="collect",
                min_collectibles=int(variables.get("coinCount", 5)),
                min_hazards=int(variables.get("hazardCount", 3)),
                require_terrain=True,
                require_light=True,
            )
            world_ok = bool(world_validation.get("summary", {}).get("ok", False))
            journal.record("repair", {"attempt": repair_attempts, "workflow": repair, "diff": repair_diff, "validation": world_validation})
            report["iterations"].append({
                "kind": "repair",
                "attempt": repair_attempts,
                "workflow": repair,
                "after": repaired,
                "diff": repair_diff,
                "worldValidation": world_validation,
            })
            after = repaired

        report["summary"].update({
            "ok": execution_ok and world_ok,
            "planOk": True,
            "executionOk": execution_ok,
            "worldOk": world_ok,
            "repairAttempts": repair_attempts,
        })
        report["journal"] = journal.write(journal_path, summary=report["summary"])
        return self._finish(report, started, report_path, dry_run=False)

    def plan(self, goal: Mapping[str, Any], *, include_snapshot: bool = False) -> Dict[str, Any]:
        snapshot = self.observer.snapshot(include_details=True) if include_snapshot else None
        return self.planner.plan(self._normalize_goal(goal), snapshot)

    def affordances(self, *, include_snapshot: bool = False) -> Dict[str, Any]:
        snapshot = self.observer.snapshot(include_details=True) if include_snapshot else None
        return EngineAffordanceMap(self.tools).build(snapshot)

    def _normalize_goal(self, goal: Mapping[str, Any]) -> Dict[str, Any]:
        if not isinstance(goal, Mapping):
            raise EngineLinkValidationError("Engine link goal must be an object.")
        spec = dict(goal)
        spec.setdefault("name", "engine_link_goal")
        spec.setdefault("goal", "Use the AI-engine link to inspect, author, and validate the scene.")
        spec.setdefault("variables", {})
        return spec

    def _step_args(self, plan: Mapping[str, Any], step_id: str) -> Dict[str, Any]:
        for step in plan.get("steps", []):
            if isinstance(step, Mapping) and step.get("id") == step_id:
                args = step.get("args", {})
                return dict(args) if isinstance(args, Mapping) else {}
        return {}

    def _finish(self, report: Dict[str, Any], started: float, report_path: str, *, dry_run: bool) -> Dict[str, Any]:
        report["finishedAt"] = _timestamp()
        report["durationMs"] = int((time.time() - started) * 1000)
        if report_path:
            if not dry_run:
                target = _project_path(self.project_root, report_path)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            report["reportPath"] = report_path
        return report
