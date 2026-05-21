from __future__ import annotations

import json
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional


class AutonomyValidationError(ValueError):
    pass


class AutonomyRunner:
    """Phase 6 closed-loop runner: inspect, plan, execute, verify, repair, checkpoint."""

    def __init__(self, tools: Any, project_root: Optional[Path] = None):
        self.tools = tools
        self.project_root = Path.cwd() if project_root is None else Path(project_root)

    def validate(self, goal: Any) -> Dict[str, Any]:
        spec = self._normalize_goal(goal)
        errors: List[str] = []
        template = str(spec.get("template", "simple_collect_game"))
        if template not in self.supported_templates():
            errors.append(f"Unsupported autonomy template: {template}")
        plan = None
        if not errors:
            plan = self.plan(spec)
            workflow_validation = self.tools.workflow.validate_workflow(plan["workflow"])
            verification_validation = self.tools.verify.validate(plan["verification"])
            if not workflow_validation.get("ok", False):
                errors.extend(f"workflow: {item}" for item in workflow_validation.get("errors", []))
            if not verification_validation.get("ok", False):
                errors.extend(f"verification: {item}" for item in verification_validation.get("errors", []))
        return {
            "ok": not errors,
            "errors": errors,
            "template": template,
            "plan": plan,
        }

    def run(
        self,
        goal: Any,
        *,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        spec = self._normalize_goal(goal)
        validation = self.validate(spec)
        if not validation["ok"]:
            raise AutonomyValidationError("; ".join(validation["errors"]))

        started = time.time()
        plan = validation["plan"] or self.plan(spec)
        variables = dict(spec.get("variables", {}))
        report: Dict[str, Any] = {
            "name": spec.get("name", "autonomy_run"),
            "goal": spec.get("goal", ""),
            "template": spec.get("template", "simple_collect_game"),
            "dryRun": dry_run,
            "startedAt": self._timestamp(started),
            "validation": {key: value for key, value in validation.items() if key != "plan"},
            "plan": plan,
            "iterations": [],
            "summary": {
                "ok": False,
                "workflowOk": False,
                "verificationOk": False,
                "repairAttempts": 0,
            },
        }

        if dry_run:
            workflow = self.tools.workflow.execute_workflow(plan["workflow"], dry_run=True, variables=variables)
            verification = self.tools.verify.run(plan["verification"], dry_run=True, variables=variables)
            report["iterations"].append({
                "kind": "dry_run",
                "workflow": workflow,
                "verification": verification,
            })
            report["summary"].update({"ok": True, "workflowOk": True, "verificationOk": True})
            return self._finish_report(report, started, report_path or spec.get("reportPath"), dry_run=True)

        preflight = self.tools.workflow.preflight()
        workflow = self.tools.workflow.execute_workflow(plan["workflow"], variables=variables)
        verification = self.tools.verify.run(plan["verification"], variables=variables)
        report["iterations"].append({
            "kind": "primary",
            "preflight": preflight,
            "workflow": workflow,
            "verification": verification,
        })

        workflow_ok = bool(workflow.get("summary", {}).get("ok", False))
        verification_ok = bool(verification.get("summary", {}).get("ok", False))
        repair_attempts = 0

        while (not workflow_ok or not verification_ok) and repair_attempts < max_repair_attempts:
            repair_attempts += 1
            repair_workflow = self.repair_plan(spec, verification)
            repair_result = self.tools.workflow.execute_workflow(repair_workflow, variables=variables, continue_on_error=True)
            verification = self.tools.verify.run(plan["verification"], variables=variables)
            workflow_ok = workflow_ok or bool(repair_result.get("summary", {}).get("ok", False))
            verification_ok = bool(verification.get("summary", {}).get("ok", False))
            report["iterations"].append({
                "kind": "repair",
                "attempt": repair_attempts,
                "workflow": repair_result,
                "verification": verification,
            })

        report["summary"].update({
            "ok": workflow_ok and verification_ok,
            "workflowOk": workflow_ok,
            "verificationOk": verification_ok,
            "repairAttempts": repair_attempts,
        })
        return self._finish_report(report, started, report_path or spec.get("reportPath"), dry_run=False)

    def supported_templates(self) -> set[str]:
        return {"simple_collect_game"}

    def plan(self, goal: Mapping[str, Any]) -> Dict[str, Any]:
        template = str(goal.get("template", "simple_collect_game"))
        if template == "simple_collect_game":
            return self._plan_simple_collect_game(goal)
        raise AutonomyValidationError(f"Unsupported autonomy template: {template}")

    def repair_plan(self, goal: Mapping[str, Any], verification: Mapping[str, Any]) -> Dict[str, Any]:
        variables = self._simple_game_variables(goal)
        game_name = variables["gameName"]
        marker_name = f"{game_name}_PlayerStart"
        capture_path = variables["capturePath"]
        checkpoint_path = variables["checkpointPath"]
        scene_path = variables["scenePath"]
        return {
            "version": 1,
            "name": f"{game_name}_repair",
            "variables": variables,
            "steps": [
                {
                    "id": "repair_select_player",
                    "tool": "entity.select_by_name",
                    "continueOnError": True,
                    "args": {"name": marker_name, "exact": True},
                },
                {
                    "id": "repair_view",
                    "tool": "editor.configure_scene_view",
                    "continueOnError": True,
                    "args": {
                        "position": [8.0, 7.0, -10.0],
                        "target": [0.0, 0.0, 0.0],
                        "shading": "lit",
                        "mode": "3D",
                    },
                },
                {
                    "id": "repair_focus_player",
                    "tool": "entity.focus",
                    "continueOnError": True,
                    "args": {"entity": "${steps.repair_select_player.result.selected}", "distance": 12.0},
                },
                {
                    "id": "repair_capture",
                    "tool": "editor.capture",
                    "continueOnError": True,
                    "args": {"path": capture_path},
                },
                {
                    "id": "repair_checkpoint",
                    "tool": "workflow.checkpoint",
                    "continueOnError": True,
                    "args": {
                        "scene_path": scene_path,
                        "screenshot_path": checkpoint_path,
                    },
                },
            ],
            "reportPath": f"Saved/AI/autonomy/{game_name}_repair_latest.json",
        }

    def _plan_simple_collect_game(self, goal: Mapping[str, Any]) -> Dict[str, Any]:
        variables = self._simple_game_variables(goal)
        game_name = variables["gameName"]
        scene_path = variables["scenePath"]
        capture_path = variables["capturePath"]
        checkpoint_path = variables["checkpointPath"]
        workflow_report = variables["workflowReport"]
        verification_report = variables["verificationReport"]
        root_name = f"{game_name}_Root"
        player_name = f"{game_name}_PlayerStart"
        goal_name = f"{game_name}_Goal"
        terrain_name = f"{game_name}_Terrain"
        camera_name = f"{game_name}_Camera"
        coin_count = int(variables["coinCount"])
        hazard_count = int(variables["hazardCount"])

        steps: List[Dict[str, Any]] = [
            {
                "id": "preflight",
                "tool": "workflow.preflight",
                "expect": {"equals": ["${steps.preflight.result.coverage.phase3Complete}", True]},
            },
            {
                "id": "create_root",
                "tool": "entity.create_empty",
                "saveAs": "root",
                "args": {"name": root_name, "position": [0.0, 0.0, 0.0], "select": False},
            },
            {
                "id": "create_terrain",
                "tool": "terrain.create",
                "saveAs": "terrain",
                "args": {
                    "name": terrain_name,
                    "position": [0.0, -0.05, 0.0],
                    "resolution": 128,
                    "world_size": [26.0, 26.0],
                    "height_scale": 1.0,
                    "chunk_count": [2, 2],
                    "generate_noise": False,
                    "add_grass": False,
                    "select": False,
                },
            },
            {
                "id": "create_player",
                "tool": "entity.create_model",
                "saveAs": "player",
                "args": {
                    "name": player_name,
                    "model_path": "${vars.playerModel}",
                    "position": [0.0, 0.35, -7.0],
                    "scale": [0.8, 1.4, 0.8],
                    "parent": "${steps.create_root.result.entity}",
                    "select": True,
                },
                "expect": {"exists": "${steps.create_player.result.entity}"},
            },
            {
                "id": "create_goal",
                "tool": "entity.create_model",
                "saveAs": "goal",
                "args": {
                    "name": goal_name,
                    "model_path": "${vars.goalModel}",
                    "position": [0.0, 0.5, 7.0],
                    "scale": [1.4, 1.4, 1.4],
                    "parent": "${steps.create_root.result.entity}",
                    "select": False,
                },
            },
        ]

        for index in range(coin_count):
            x = (index - (coin_count - 1) * 0.5) * 2.4
            z = -3.5 + index * (7.0 / max(coin_count, 1))
            steps.append({
                "id": f"create_coin_{index + 1:02d}",
                "tool": "entity.create_model",
                "args": {
                    "name": f"{game_name}_Coin_{index + 1:02d}",
                    "model_path": "${vars.coinModel}",
                    "position": [round(x, 3), 0.45, round(z, 3)],
                    "scale": [0.45, 0.45, 0.45],
                    "parent": "${steps.create_root.result.entity}",
                    "select": False,
                },
            })

        for index in range(hazard_count):
            x = -4.0 if index % 2 == 0 else 4.0
            z = -2.0 + index * 3.2
            steps.append({
                "id": f"create_hazard_{index + 1:02d}",
                "tool": "entity.create_model",
                "args": {
                    "name": f"{game_name}_Hazard_{index + 1:02d}",
                    "model_path": "${vars.hazardModel}",
                    "position": [x, 0.35, z],
                    "scale": [1.2, 0.7, 1.2],
                    "parent": "${steps.create_root.result.entity}",
                    "select": False,
                },
            })

        steps.extend([
            {"id": "create_lights", "tool": "lighting.rig", "args": {"name": f"{game_name}_LightRig"}},
            {
                "id": "create_camera",
                "tool": "lighting.create_camera",
                "args": {
                    "name": camera_name,
                    "position": [0.0, 7.5, -10.5],
                    "fovY": 0.785398,
                    "nearZ": 0.1,
                    "farZ": 100000.0,
                    "main": True,
                    "select": False,
                },
            },
            {"id": "create_ui_canvas", "tool": "ui.create_canvas", "continueOnError": True},
            {"id": "create_player_hp_ui", "tool": "ui.create_template", "continueOnError": True, "args": {"kind": "player_hp"}},
            {
                "id": "set_scene_view",
                "tool": "editor.configure_scene_view",
                "args": {
                    "position": [8.0, 7.0, -10.0],
                    "target": [0.0, 0.0, 0.0],
                    "shading": "lit",
                    "mode": "3D",
                    "grid": {"visible": True, "cellSize": 1.0, "halfLineCount": 14},
                    "visibility": {"gizmo": True, "lightIcons": True, "cameraIcons": True},
                },
            },
            {
                "id": "select_player",
                "tool": "entity.select_by_name",
                "args": {"name": player_name, "exact": True},
                "expect": {"exists": "${steps.select_player.result.selected}"},
            },
            {
                "id": "focus_player",
                "tool": "entity.focus",
                "continueOnError": True,
                "args": {"entity": "${steps.select_player.result.selected}", "distance": 12.0},
            },
            {"id": "capture_scene", "tool": "editor.capture", "args": {"path": capture_path}},
            {"id": "save_scene", "tool": "editor.save_scene", "args": {"path": scene_path}},
        ])

        inline_checks = [
            check for check in self._simple_game_checks(variables)
            if check.get("id") != "player_visible"
        ]
        workflow = {
            "version": 1,
            "name": f"{game_name}_workflow",
            "variables": variables,
            "defaults": {"retries": 1, "retryDelay": 0.1},
            "steps": steps,
            "verify": [{
                "id": "verify_simple_game",
                "tool": "workflow.verify",
                "continueOnError": True,
                "args": {"name": f"{game_name}_inline_verify", "checks": inline_checks},
            }],
            "checkpoint": {"scene_path": scene_path, "screenshot_path": checkpoint_path},
            "reportPath": workflow_report,
        }
        verification = {
            "name": f"{game_name}_verification",
            "variables": variables,
            "checks": self._simple_game_checks(variables) + [
                {"id": "checkpoint_image", "type": "image.bmp", "path": checkpoint_path, "minBytes": 1024, "minWidth": 64, "minHeight": 64, "minUniqueColors": 2, "minNonBlackRatio": 0.01},
                {"id": "workflow_report", "type": "workflow.report", "path": workflow_report},
            ],
            "reportPath": verification_report,
        }
        return {"workflow": workflow, "verification": verification}

    def _simple_game_checks(self, variables: Mapping[str, Any]) -> List[Dict[str, Any]]:
        game_name = str(variables["gameName"])
        scene_path = str(variables["scenePath"])
        capture_path = str(variables["capturePath"])
        checks: List[Dict[str, Any]] = [
            {"id": "engine_responds", "type": "engine.ping"},
            {"id": "phase3_coverage", "type": "coverage", "phase3Complete": True},
            {"id": "player_model_asset", "type": "asset.exists", "path": str(variables["playerModel"]), "minBytes": 128},
            {"id": "goal_model_asset", "type": "asset.exists", "path": str(variables["goalModel"]), "minBytes": 128},
            {"id": "coin_model_asset", "type": "asset.exists", "path": str(variables["coinModel"]), "minBytes": 128},
            {"id": "hazard_model_asset", "type": "asset.exists", "path": str(variables["hazardModel"]), "minBytes": 128},
            {"id": "root_exists", "type": "entity.exists", "name": f"{game_name}_Root", "exact": True, "min": 1},
            {"id": "player_exists", "type": "entity.exists", "name": f"{game_name}_PlayerStart", "exact": True, "min": 1},
            {"id": "goal_exists", "type": "entity.exists", "name": f"{game_name}_Goal", "exact": True, "min": 1},
            {"id": "player_visible", "type": "visual.selected_visible", "name": f"{game_name}_PlayerStart", "exact": True, "minVisible": 1},
            {"id": "capture_image", "type": "image.bmp", "path": capture_path, "minBytes": 1024, "minWidth": 64, "minHeight": 64, "minUniqueColors": 2, "minNonBlackRatio": 0.01},
            {"id": "scene_saved", "type": "file.exists", "path": scene_path, "minBytes": 128},
        ]
        for index in range(int(variables["coinCount"])):
            checks.append({"id": f"coin_{index + 1:02d}_exists", "type": "entity.exists", "name": f"{game_name}_Coin_{index + 1:02d}", "exact": True, "min": 1})
        for index in range(int(variables["hazardCount"])):
            checks.append({"id": f"hazard_{index + 1:02d}_exists", "type": "entity.exists", "name": f"{game_name}_Hazard_{index + 1:02d}", "exact": True, "min": 1})
        return checks

    def _simple_game_variables(self, goal: Mapping[str, Any]) -> Dict[str, Any]:
        variables = dict(goal.get("variables", {})) if isinstance(goal.get("variables"), Mapping) else {}
        game_name = str(variables.get("gameName") or goal.get("gameName") or "AI_MiniCollect")
        variables.setdefault("gameName", game_name)
        variables.setdefault("coinCount", int(goal.get("coinCount", variables.get("coinCount", 5))))
        variables.setdefault("hazardCount", int(goal.get("hazardCount", variables.get("hazardCount", 3))))
        variables.setdefault("scenePath", f"Data/Scene/{game_name}.scene")
        variables.setdefault("capturePath", f"Saved/AI/screenshots/{game_name}_scene.bmp")
        variables.setdefault("checkpointPath", f"Saved/AI/screenshots/{game_name}_checkpoint.bmp")
        variables.setdefault("workflowReport", f"Saved/AI/autonomy/{game_name}_workflow_latest.json")
        variables.setdefault("verificationReport", f"Saved/AI/autonomy/{game_name}_verification_latest.json")
        variables.setdefault("playerModel", "Data/Model/Cube/Cube.fbx")
        variables.setdefault("goalModel", "Data/Model/crystal/fbx_crystal_001.fbx")
        variables.setdefault("coinModel", "Data/Model/ring/fbx_ring_001.fbx")
        variables.setdefault("hazardModel", "Data/Model/cylinder/fbx_cylinder_001.fbx")
        return variables

    def _normalize_goal(self, goal: Any) -> Dict[str, Any]:
        if isinstance(goal, str):
            return {
                "name": "simple_collect_game",
                "goal": goal,
                "template": "simple_collect_game",
                "variables": {},
            }
        if not isinstance(goal, Mapping):
            raise AutonomyValidationError("Autonomy goal must be a string or an object.")
        spec = dict(goal)
        spec.setdefault("name", "simple_collect_game")
        spec.setdefault("goal", "Create a simple collect-the-coins game scene.")
        spec.setdefault("template", "simple_collect_game")
        spec.setdefault("variables", {})
        return spec

    def _finish_report(self, report: Dict[str, Any], started: float, report_path: Optional[str], *, dry_run: bool) -> Dict[str, Any]:
        finished = time.time()
        report["finishedAt"] = self._timestamp(finished)
        report["durationMs"] = int((finished - started) * 1000)
        if report_path:
            if not dry_run:
                path = self._project_path(report_path)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            report["reportPath"] = report_path
        return report

    def _project_path(self, path: str) -> Path:
        target = Path(path)
        if target.is_absolute():
            return target
        return self.project_root / target

    def _timestamp(self, value: Optional[float] = None) -> str:
        dt = datetime.fromtimestamp(time.time() if value is None else value, timezone.utc)
        return dt.isoformat(timespec="milliseconds").replace("+00:00", "Z")
