from __future__ import annotations

import json
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional


class WorldModelValidationError(ValueError):
    pass


def _timestamp(value: Optional[float] = None) -> str:
    dt = datetime.fromtimestamp(time.time() if value is None else value, timezone.utc)
    return dt.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _entries(value: Any, *keys: str) -> List[Mapping[str, Any]]:
    if isinstance(value, list):
        return [item for item in value if isinstance(item, Mapping)]
    if isinstance(value, Mapping):
        for key in keys:
            items = value.get(key)
            if isinstance(items, list):
                return [item for item in items if isinstance(item, Mapping)]
    return []


def _entity_payload(value: Any) -> Dict[str, Any]:
    if isinstance(value, Mapping) and isinstance(value.get("entity"), Mapping):
        return dict(value["entity"])
    if isinstance(value, Mapping):
        return dict(value)
    return {}


def _entity_id(entity: Mapping[str, Any]) -> str:
    value = entity.get("entity") or entity.get("id") or entity.get("entityId")
    return str(value) if value is not None else ""


def _entity_name(entity: Mapping[str, Any]) -> str:
    return str(entity.get("name") or entity.get("label") or _entity_id(entity) or "Entity")


def _component_names(entity: Mapping[str, Any]) -> List[str]:
    components = entity.get("components", [])
    if isinstance(components, list):
        return sorted({str(item) for item in components})
    return []


def _component_data(entity: Mapping[str, Any]) -> Dict[str, Any]:
    value = entity.get("componentData", {})
    return dict(value) if isinstance(value, Mapping) else {}


def _transform(entity: Mapping[str, Any]) -> Dict[str, Any]:
    data = _component_data(entity)
    transform = data.get("TransformComponent")
    return dict(transform) if isinstance(transform, Mapping) else {}


def _mesh(entity: Mapping[str, Any]) -> Dict[str, Any]:
    data = _component_data(entity)
    mesh = data.get("MeshComponent")
    return dict(mesh) if isinstance(mesh, Mapping) else {}


def _world_position(entity: Mapping[str, Any]) -> Optional[List[float]]:
    transform = _transform(entity)
    value = transform.get("worldPosition") or transform.get("localPosition")
    if isinstance(value, list) and len(value) >= 3:
        return [float(value[0]), float(value[1]), float(value[2])]
    return None


def _lower_text(*items: Any) -> str:
    return " ".join(str(item or "") for item in items).lower()


def _contains_any(text: str, words: Iterable[str]) -> bool:
    return any(word in text for word in words)


def _small_entity(entity: Mapping[str, Any]) -> Dict[str, Any]:
    return {
        "entity": _entity_id(entity),
        "name": _entity_name(entity),
        "role": entity.get("semantic", {}).get("role", "entity") if isinstance(entity.get("semantic"), Mapping) else "entity",
        "tags": list(entity.get("semantic", {}).get("tags", [])) if isinstance(entity.get("semantic"), Mapping) else [],
        "components": _component_names(entity),
        "worldPosition": _world_position(entity),
    }


def normalize_entity(summary: Mapping[str, Any], detail: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
    merged: Dict[str, Any] = dict(summary)
    if detail:
        merged.update(_entity_payload(detail))
    merged["entity"] = _entity_id(merged)
    merged["name"] = _entity_name(merged)
    merged["components"] = _component_names(merged)
    if "componentData" not in merged:
        merged["componentData"] = {}
    transform = _transform(merged)
    if transform:
        merged["transform"] = transform
    mesh = _mesh(merged)
    if mesh:
        merged["mesh"] = mesh
    return merged


class SemanticClassifier:
    """Name/component/model based ECS semantic classifier for AI world reasoning."""

    ROLE_PRIORITY = [
        "player",
        "goal",
        "collectible",
        "hazard",
        "enemy",
        "camera",
        "light",
        "terrain",
        "ui",
        "root",
        "marker",
        "entity",
    ]

    KEYWORDS = {
        "player": ["player", "hero", "avatar", "character", "playable"],
        "start": ["start", "spawn", "entry"],
        "goal": ["goal", "exit", "finish", "win", "crystal"],
        "collectible": ["coin", "collect", "pickup", "gem", "ring", "heal"],
        "hazard": ["hazard", "trap", "spike", "damage", "danger", "lava"],
        "enemy": ["enemy", "boss", "mob", "mutant"],
        "camera": ["camera", "cam"],
        "light": ["light", "directional", "pointlight", "spotlight"],
        "terrain": ["terrain", "ground", "floor"],
        "ui": ["ui", "canvas", "widget", "hud", "hp"],
        "root": ["root", "scene_root", "gameplayroot"],
    }

    def classify(self, entity: Mapping[str, Any]) -> Dict[str, Any]:
        name = _entity_name(entity)
        components = set(_component_names(entity))
        mesh = _mesh(entity)
        model_path = str(mesh.get("modelFilePath") or "")
        text = _lower_text(name, model_path)
        tags = set()

        if "TransformComponent" in components:
            tags.add("transform")
        if "MeshComponent" in components:
            tags.add("mesh")
        if "TerrainComponent" in components or _contains_any(text, self.KEYWORDS["terrain"]):
            tags.add("terrain")
        if "CameraComponent" in components or _contains_any(text, self.KEYWORDS["camera"]):
            tags.add("camera")
        if "LightComponent" in components or _contains_any(text, self.KEYWORDS["light"]):
            tags.add("light")
        if "RectTransformComponent" in components or _contains_any(text, self.KEYWORDS["ui"]):
            tags.add("ui")
        if _contains_any(text, self.KEYWORDS["root"]) or (not entity.get("parent") and entity.get("children")):
            tags.add("root")
        if _contains_any(text, self.KEYWORDS["start"]):
            tags.add("start")
        if _contains_any(text, self.KEYWORDS["player"]) or "PlayerControllerComponent" in components:
            tags.add("player")
        if _contains_any(text, self.KEYWORDS["goal"]):
            tags.add("goal")
        if _contains_any(text, self.KEYWORDS["collectible"]):
            tags.add("collectible")
        if _contains_any(text, self.KEYWORDS["hazard"]) or "DamageComponent" in components:
            tags.add("hazard")
        if _contains_any(text, self.KEYWORDS["enemy"]) or "AIControllerComponent" in components:
            tags.add("enemy")

        if not tags and "MeshComponent" not in components:
            tags.add("marker")

        role = "entity"
        for candidate in self.ROLE_PRIORITY:
            if candidate in tags:
                role = candidate
                break

        confidence = 0.35
        if role != "entity":
            confidence += 0.25
        if "MeshComponent" in components or "TerrainComponent" in components:
            confidence += 0.15
        if any(word in text for words in self.KEYWORDS.values() for word in words):
            confidence += 0.2

        return {
            "role": role,
            "tags": sorted(tags),
            "confidence": round(min(confidence, 0.99), 3),
        }


class WorldModel:
    """Phase 7-8 ECS world model: normalized entities, semantic queries, diff, validation."""

    def __init__(self, snapshot: Mapping[str, Any]):
        self.snapshot = dict(snapshot)
        self.entities = _entries(self.snapshot.get("entities", []))
        self.by_id = {str(entity.get("entity")): entity for entity in self.entities if entity.get("entity")}

    def find(
        self,
        *,
        kind: str = "",
        tag: str = "",
        role: str = "",
        name: str = "",
        component: str = "",
        exact: bool = False,
    ) -> List[Mapping[str, Any]]:
        results: List[Mapping[str, Any]] = []
        target_kind = role or kind or tag
        needle = name.lower()
        for entity in self.entities:
            semantic = entity.get("semantic", {}) if isinstance(entity.get("semantic"), Mapping) else {}
            tags = set(str(item) for item in semantic.get("tags", [])) if isinstance(semantic.get("tags"), list) else set()
            if target_kind and semantic.get("role") != target_kind and target_kind not in tags:
                continue
            if component and component not in _component_names(entity):
                continue
            if name:
                haystack = _entity_name(entity).lower()
                if (haystack != needle) if exact else (needle not in haystack):
                    continue
            results.append(entity)
        return results

    def semantic_summary(self, *, sample_limit: int = 12) -> Dict[str, Any]:
        roles: Dict[str, List[Dict[str, Any]]] = {}
        tags: Dict[str, int] = {}
        component_counts: Dict[str, int] = {}
        for entity in self.entities:
            semantic = entity.get("semantic", {}) if isinstance(entity.get("semantic"), Mapping) else {}
            role = str(semantic.get("role") or "entity")
            roles.setdefault(role, [])
            if len(roles[role]) < sample_limit:
                roles[role].append(_small_entity(entity))
            for tag in semantic.get("tags", []) if isinstance(semantic.get("tags"), list) else []:
                tags[str(tag)] = tags.get(str(tag), 0) + 1
            for component in _component_names(entity):
                component_counts[component] = component_counts.get(component, 0) + 1

        return {
            "entityCount": len(self.entities),
            "roles": {role: {"count": len(self.find(role=role)), "sample": sample} for role, sample in sorted(roles.items())},
            "tags": dict(sorted(tags.items())),
            "components": dict(sorted(component_counts.items())),
        }

    def validate_gameplay_loop(
        self,
        *,
        style: str = "collect",
        min_collectibles: int = 1,
        min_hazards: int = 0,
        require_player: bool = True,
        require_goal: bool = True,
        require_camera: bool = True,
        require_terrain: bool = False,
        require_light: bool = False,
    ) -> Dict[str, Any]:
        checks: List[Dict[str, Any]] = []

        def add_check(check_id: str, label: str, entities: List[Mapping[str, Any]], minimum: int) -> None:
            checks.append({
                "id": check_id,
                "label": label,
                "ok": len(entities) >= minimum,
                "count": len(entities),
                "minimum": minimum,
                "entities": [_small_entity(entity) for entity in entities[:20]],
            })

        if require_player:
            add_check("player_present", "At least one player/start entity exists.", self.find(kind="player"), 1)
        if require_goal:
            add_check("goal_present", "At least one goal/exit entity exists.", self.find(kind="goal"), 1)
        add_check("collectibles_present", "Collectible entities meet the requested minimum.", self.find(kind="collectible"), int(min_collectibles))
        if min_hazards > 0:
            add_check("hazards_present", "Hazard entities meet the requested minimum.", self.find(kind="hazard"), int(min_hazards))
        if require_camera:
            add_check("camera_present", "At least one camera entity exists.", self.find(kind="camera"), 1)
        if require_terrain:
            add_check("terrain_present", "At least one terrain/ground entity exists.", self.find(kind="terrain"), 1)
        if require_light:
            add_check("light_present", "At least one light entity exists.", self.find(kind="light"), 1)

        player_entities = self.find(kind="player")
        goal_entities = self.find(kind="goal")
        if player_entities and goal_entities:
            player_pos = _world_position(player_entities[0])
            goal_pos = _world_position(goal_entities[0])
            distance_ok = False
            distance = None
            if player_pos and goal_pos:
                dx = goal_pos[0] - player_pos[0]
                dy = goal_pos[1] - player_pos[1]
                dz = goal_pos[2] - player_pos[2]
                distance = (dx * dx + dy * dy + dz * dz) ** 0.5
                distance_ok = distance >= 2.0
            checks.append({
                "id": "player_goal_spacing",
                "label": "Player and goal are separated enough to form a path.",
                "ok": distance_ok,
                "distance": distance,
                "minimum": 2.0,
                "entities": [_small_entity(player_entities[0]), _small_entity(goal_entities[0])],
            })

        failed = [check for check in checks if not check.get("ok")]
        return {
            "name": f"{style}_gameplay_validation",
            "ok": not failed,
            "style": style,
            "checks": checks,
            "summary": {
                "ok": not failed,
                "passed": len(checks) - len(failed),
                "failed": len(failed),
                "total": len(checks),
            },
        }

    def repair_plan(
        self,
        validation: Mapping[str, Any],
        *,
        game_name: str = "AI_WorldGame",
        scene_path: Optional[str] = None,
        screenshot_path: Optional[str] = None,
        coin_count: int = 5,
        hazard_count: int = 0,
    ) -> Dict[str, Any]:
        scene_path = scene_path or f"Data/Scene/{game_name}.scene"
        screenshot_path = screenshot_path or f"Saved/AI/screenshots/{game_name}_world_repair.bmp"
        failed = {
            str(check.get("id")) for check in validation.get("checks", [])
            if isinstance(check, Mapping) and not check.get("ok")
        }
        steps: List[Dict[str, Any]] = []

        if "terrain_present" in failed:
            steps.append({
                "id": "repair_terrain",
                "tool": "terrain.create",
                "continueOnError": True,
                "args": {
                    "name": f"{game_name}_Terrain",
                    "position": [0.0, -0.05, 0.0],
                    "resolution": 128,
                    "world_size": [24.0, 24.0],
                    "height_scale": 1.0,
                    "chunk_count": [2, 2],
                    "generate_noise": False,
                    "add_grass": False,
                    "select": False,
                },
            })
        if "player_present" in failed:
            steps.append({
                "id": "repair_player",
                "tool": "entity.create_model",
                "continueOnError": True,
                "args": {
                    "name": f"{game_name}_PlayerStart",
                    "model_path": "Data/Model/Cube/Cube.fbx",
                    "position": [0.0, 0.35, -6.0],
                    "scale": [0.8, 1.4, 0.8],
                    "select": True,
                },
            })
        if "goal_present" in failed:
            steps.append({
                "id": "repair_goal",
                "tool": "entity.create_model",
                "continueOnError": True,
                "args": {
                    "name": f"{game_name}_Goal",
                    "model_path": "Data/Model/crystal/fbx_crystal_001.fbx",
                    "position": [0.0, 0.5, 6.0],
                    "scale": [1.4, 1.4, 1.4],
                    "select": False,
                },
            })
        if "collectibles_present" in failed:
            existing = len(self.find(kind="collectible"))
            for index in range(existing, max(existing, int(coin_count))):
                steps.append({
                    "id": f"repair_coin_{index + 1:02d}",
                    "tool": "entity.create_model",
                    "continueOnError": True,
                    "args": {
                        "name": f"{game_name}_Coin_{index + 1:02d}",
                        "model_path": "Data/Model/ring/fbx_ring_001.fbx",
                        "position": [round((index - 2) * 2.0, 3), 0.45, round(-2.5 + index * 1.3, 3)],
                        "scale": [0.45, 0.45, 0.45],
                        "select": False,
                    },
                })
        if "hazards_present" in failed:
            existing = len(self.find(kind="hazard"))
            for index in range(existing, max(existing, int(hazard_count))):
                steps.append({
                    "id": f"repair_hazard_{index + 1:02d}",
                    "tool": "entity.create_model",
                    "continueOnError": True,
                    "args": {
                        "name": f"{game_name}_Hazard_{index + 1:02d}",
                        "model_path": "Data/Model/cylinder/fbx_cylinder_001.fbx",
                        "position": [-4.0 if index % 2 == 0 else 4.0, 0.35, round(-1.5 + index * 3.0, 3)],
                        "scale": [1.2, 0.7, 1.2],
                        "select": False,
                    },
                })
        if "camera_present" in failed:
            steps.append({
                "id": "repair_camera",
                "tool": "lighting.create_camera",
                "continueOnError": True,
                "args": {
                    "name": f"{game_name}_Camera",
                    "position": [0.0, 7.5, -10.5],
                    "fovY": 0.785398,
                    "nearZ": 0.1,
                    "farZ": 100000.0,
                    "main": True,
                    "select": False,
                },
            })
        if "light_present" in failed:
            steps.append({
                "id": "repair_light_rig",
                "tool": "lighting.rig",
                "continueOnError": True,
                "args": {"name": f"{game_name}_LightRig"},
            })

        steps.extend([
            {
                "id": "repair_select_player",
                "tool": "entity.select_by_name",
                "continueOnError": True,
                "args": {"name": f"{game_name}_PlayerStart", "exact": True},
            },
            {
                "id": "repair_focus_player",
                "tool": "entity.focus",
                "continueOnError": True,
                "args": {"entity": "${steps.repair_select_player.result.selected}", "distance": 12.0},
            },
            {"id": "repair_capture", "tool": "editor.capture", "continueOnError": True, "args": {"path": screenshot_path}},
            {"id": "repair_save_scene", "tool": "editor.save_scene", "continueOnError": True, "args": {"path": scene_path}},
        ])

        return {
            "version": 1,
            "name": f"{game_name}_world_repair",
            "steps": steps,
            "reportPath": f"Saved/AI/world/{game_name}_repair_latest.json",
        }


def diff_snapshots(before: Mapping[str, Any], after: Mapping[str, Any]) -> Dict[str, Any]:
    before_model = WorldModel(before)
    after_model = WorldModel(after)
    before_ids = set(before_model.by_id)
    after_ids = set(after_model.by_id)
    added_ids = sorted(after_ids - before_ids)
    removed_ids = sorted(before_ids - after_ids)
    changed: List[Dict[str, Any]] = []

    for entity_id in sorted(before_ids & after_ids):
        old = before_model.by_id[entity_id]
        new = after_model.by_id[entity_id]
        changes: Dict[str, Any] = {"entity": entity_id, "name": _entity_name(new)}
        if _entity_name(old) != _entity_name(new):
            changes["nameChanged"] = {"before": _entity_name(old), "after": _entity_name(new)}
        if old.get("parent") != new.get("parent"):
            changes["parentChanged"] = {"before": old.get("parent"), "after": new.get("parent")}
        old_components = set(_component_names(old))
        new_components = set(_component_names(new))
        if old_components != new_components:
            changes["componentsAdded"] = sorted(new_components - old_components)
            changes["componentsRemoved"] = sorted(old_components - new_components)
        if _world_position(old) != _world_position(new):
            changes["worldPositionChanged"] = {"before": _world_position(old), "after": _world_position(new)}
        old_semantic = old.get("semantic", {}) if isinstance(old.get("semantic"), Mapping) else {}
        new_semantic = new.get("semantic", {}) if isinstance(new.get("semantic"), Mapping) else {}
        if old_semantic.get("role") != new_semantic.get("role") or old_semantic.get("tags") != new_semantic.get("tags"):
            changes["semanticChanged"] = {"before": old_semantic, "after": new_semantic}
        if len(changes) > 2:
            changed.append(changes)

    return {
        "summary": {
            "added": len(added_ids),
            "removed": len(removed_ids),
            "changed": len(changed),
            "beforeEntityCount": len(before_model.entities),
            "afterEntityCount": len(after_model.entities),
        },
        "added": [_small_entity(after_model.by_id[entity_id]) for entity_id in added_ids],
        "removed": [_small_entity(before_model.by_id[entity_id]) for entity_id in removed_ids],
        "changed": changed,
    }


class ECSObserver:
    """Phase 7 observation bridge from engine ECS snapshots to AI-friendly world models."""

    def __init__(self, tools: Any, project_root: Optional[Path] = None):
        self.tools = tools
        self.project_root = Path.cwd() if project_root is None else Path(project_root)
        self.classifier = SemanticClassifier()

    def snapshot(self, *, include_details: bool = True, report_path: Optional[str] = None) -> Dict[str, Any]:
        started = time.time()
        raw_entities = _entries(self.tools.engine.list_entities(), "entities", "items", "data")
        entities: List[Dict[str, Any]] = []
        errors: List[Dict[str, Any]] = []

        for summary in raw_entities:
            detail: Optional[Mapping[str, Any]] = None
            entity_id = _entity_id(summary)
            if include_details and entity_id:
                try:
                    detail = self.tools.entities.get(entity_id)
                except Exception as exc:
                    errors.append({"entity": entity_id, "error": str(exc)})
            entity = normalize_entity(summary, detail)
            entity["semantic"] = self.classifier.classify(entity)
            entities.append(entity)

        snapshot: Dict[str, Any] = {
            "version": 1,
            "phase": "7_ecs_observation",
            "createdAt": _timestamp(started),
            "durationMs": int((time.time() - started) * 1000),
            "includeDetails": include_details,
            "engine": self.tools.engine.get_engine_state(),
            "sceneView": self.tools.engine.scene_view_get_state(),
            "visual": self.tools.engine.get_visual_state(),
            "entities": entities,
            "errors": errors,
        }
        snapshot["semantic"] = WorldModel(snapshot).semantic_summary()
        if report_path:
            self.write_report(snapshot, report_path)
            snapshot["reportPath"] = report_path
        return snapshot

    def diff(self, before: Mapping[str, Any], after: Mapping[str, Any]) -> Dict[str, Any]:
        return diff_snapshots(before, after)

    def write_report(self, value: Mapping[str, Any], path: str) -> None:
        target = Path(path)
        if not target.is_absolute():
            target = self.project_root / target
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


class WorldAuthoringRunner:
    """Phase 9 goal runner that wraps autonomy with ECS observation, semantic validation, and repair."""

    def __init__(self, tools: Any, project_root: Optional[Path] = None):
        self.tools = tools
        self.project_root = Path.cwd() if project_root is None else Path(project_root)
        self.observer = ECSObserver(tools, self.project_root)

    def run(
        self,
        goal: Mapping[str, Any],
        *,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        spec = self._normalize_goal(goal)
        started = time.time()
        variables = dict(spec.get("variables", {}))
        game_name = str(variables.get("gameName") or spec.get("gameName") or "AI_WorldGame")
        coin_count = int(variables.get("coinCount", spec.get("coinCount", 5)))
        hazard_count = int(variables.get("hazardCount", spec.get("hazardCount", 3)))
        autonomy_report_path = str(variables.get("autonomyReport") or f"Saved/AI/autonomy/{game_name}_autonomy_latest.json")
        autonomy_spec = dict(spec)
        autonomy_spec["reportPath"] = autonomy_report_path

        report: Dict[str, Any] = {
            "name": spec.get("name", "world_authoring_goal"),
            "goal": spec.get("goal", ""),
            "template": spec.get("template", "simple_collect_game"),
            "phase": "9_world_authoring",
            "dryRun": dry_run,
            "startedAt": _timestamp(started),
            "iterations": [],
            "summary": {"ok": False, "autonomyOk": False, "worldOk": False, "repairAttempts": 0},
        }

        if dry_run:
            autonomy = self.tools.autonomy.run(autonomy_spec, dry_run=True, max_repair_attempts=max_repair_attempts)
            report["iterations"].append({"kind": "dry_run", "autonomy": autonomy})
            report["summary"].update({"ok": True, "autonomyOk": True, "worldOk": True})
            return self._finish(report, started, report_path or spec.get("reportPath"), dry_run=True)

        before = self.observer.snapshot(include_details=True)
        autonomy = self.tools.autonomy.run(autonomy_spec, dry_run=False, max_repair_attempts=max_repair_attempts)
        after = self.observer.snapshot(include_details=True)
        diff = self.observer.diff(before, after)
        validation = WorldModel(after).validate_gameplay_loop(
            style="collect",
            min_collectibles=coin_count,
            min_hazards=hazard_count,
            require_terrain=True,
            require_light=True,
        )
        report["iterations"].append({
            "kind": "primary",
            "before": before,
            "autonomy": autonomy,
            "after": after,
            "diff": diff,
            "worldValidation": validation,
        })

        autonomy_ok = bool(autonomy.get("summary", {}).get("ok", False))
        world_ok = bool(validation.get("summary", {}).get("ok", False))
        repair_attempts = 0

        while (not world_ok) and repair_attempts < max_repair_attempts:
            repair_attempts += 1
            repair_plan = WorldModel(after).repair_plan(
                validation,
                game_name=game_name,
                scene_path=str(variables.get("scenePath") or f"Data/Scene/{game_name}.scene"),
                screenshot_path=str(variables.get("worldRepairScreenshot") or f"Saved/AI/screenshots/{game_name}_world_repair.bmp"),
                coin_count=coin_count,
                hazard_count=hazard_count,
            )
            repair = self.tools.workflow.execute_workflow(repair_plan, continue_on_error=True)
            repaired = self.observer.snapshot(include_details=True)
            repair_diff = self.observer.diff(after, repaired)
            validation = WorldModel(repaired).validate_gameplay_loop(
                style="collect",
                min_collectibles=coin_count,
                min_hazards=hazard_count,
                require_terrain=True,
                require_light=True,
            )
            world_ok = bool(validation.get("summary", {}).get("ok", False))
            report["iterations"].append({
                "kind": "world_repair",
                "attempt": repair_attempts,
                "workflow": repair,
                "after": repaired,
                "diff": repair_diff,
                "worldValidation": validation,
            })
            after = repaired

        report["summary"].update({
            "ok": autonomy_ok and world_ok,
            "autonomyOk": autonomy_ok,
            "worldOk": world_ok,
            "repairAttempts": repair_attempts,
        })
        return self._finish(report, started, report_path or spec.get("reportPath"), dry_run=False)

    def _normalize_goal(self, goal: Mapping[str, Any]) -> Dict[str, Any]:
        if not isinstance(goal, Mapping):
            raise WorldModelValidationError("World authoring goal must be an object.")
        spec = dict(goal)
        spec.setdefault("name", "world_authoring_goal")
        spec.setdefault("goal", "Create and validate a game scene through the ECS world model.")
        spec.setdefault("template", "simple_collect_game")
        spec.setdefault("variables", {})
        if spec["template"] != "simple_collect_game":
            raise WorldModelValidationError(f"Unsupported world authoring template: {spec['template']}")
        return spec

    def _finish(self, report: Dict[str, Any], started: float, report_path: Optional[str], *, dry_run: bool) -> Dict[str, Any]:
        report["finishedAt"] = _timestamp()
        report["durationMs"] = int((time.time() - started) * 1000)
        if report_path:
            if not dry_run:
                target = Path(report_path)
                if not target.is_absolute():
                    target = self.project_root / target
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            report["reportPath"] = report_path
        return report
