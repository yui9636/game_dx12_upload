from __future__ import annotations

import math
import time
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, List, Mapping, Optional, Sequence

try:
    from .autonomy_runner import AutonomyRunner, AutonomyValidationError
    from .engine_link import EngineAffordanceMap, EngineLinkRunner, EngineLinkValidationError, IntentPlanner
    from .engine_client import COMMANDS, EngineClient
    from .verification_runner import VerificationRunner, VerificationValidationError
    from .world_model import ECSObserver, WorldAuthoringRunner, WorldModel, WorldModelValidationError, diff_snapshots
    from .workflow_runner import WorkflowRunner, WorkflowValidationError
except ImportError:
    from autonomy_runner import AutonomyRunner, AutonomyValidationError
    from engine_link import EngineAffordanceMap, EngineLinkRunner, EngineLinkValidationError, IntentPlanner
    from engine_client import COMMANDS, EngineClient
    from verification_runner import VerificationRunner, VerificationValidationError
    from world_model import ECSObserver, WorldAuthoringRunner, WorldModel, WorldModelValidationError, diff_snapshots
    from workflow_runner import WorkflowRunner, WorkflowValidationError


Vector3 = Sequence[float]
Quaternion = Sequence[float]


def _vec3(value: Optional[Vector3], fallback: Vector3 = (0.0, 0.0, 0.0)) -> List[float]:
    source = fallback if value is None else value
    if len(source) < 3:
        raise ValueError("Vector3 values must contain at least three numbers.")
    return [float(source[0]), float(source[1]), float(source[2])]


def _vec2(value: Optional[Sequence[float]], fallback: Sequence[float] = (0.0, 0.0)) -> List[float]:
    source = fallback if value is None else value
    if len(source) < 2:
        raise ValueError("Vector2 values must contain at least two numbers.")
    return [float(source[0]), float(source[1])]


def _quat(value: Optional[Quaternion], fallback: Quaternion = (0.0, 0.0, 0.0, 1.0)) -> List[float]:
    source = fallback if value is None else value
    if len(source) < 4:
        raise ValueError("Quaternion values must contain at least four numbers.")
    return [float(source[0]), float(source[1]), float(source[2]), float(source[3])]


def _entity_id(result: Mapping[str, Any]) -> str:
    entity = result.get("entity") or result.get("entityId") or result.get("id")
    if entity is None:
        raise RuntimeError(f"Tool expected an entity id in result: {result}")
    return str(entity)


def _asset_kind(path: str) -> str:
    suffix = Path(path).suffix.lower()
    if suffix == ".prefab":
        return "prefab"
    if suffix in {".fbx", ".obj", ".gltf", ".glb", ".model"}:
        return "model"
    if suffix in {".material", ".mat"}:
        return "material"
    if suffix == ".terrain":
        return "terrain"
    if suffix in {".effectgraph", ".effect", ".vfx"}:
        return "effect"
    raise ValueError(f"Unsupported asset extension for automatic placement: {path}")


def _entries(result: Any, *keys: str) -> List[Mapping[str, Any]]:
    if isinstance(result, list):
        return [item for item in result if isinstance(item, Mapping)]
    if isinstance(result, Mapping):
        for key in keys:
            value = result.get(key)
            if isinstance(value, list):
                return [item for item in value if isinstance(item, Mapping)]
    return []


def _entity_name(entity: Mapping[str, Any]) -> str:
    value = entity.get("name")
    if isinstance(value, Mapping):
        value = value.get("name")
    return str(value or entity.get("displayName") or "")


def _merge(base: Optional[Mapping[str, Any]] = None, **kwargs: Any) -> Dict[str, Any]:
    result = dict(base or {})
    for key, value in kwargs.items():
        if value is not None:
            result[key] = value
    return result


class AssetTools:
    """High-level asset-browser operations."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def list(self, path: str = "Data", type_filter: str = "") -> Dict[str, Any]:
        params = {"path": path}
        if type_filter:
            params["type"] = type_filter
        return self.engine.asset_browser_list(params)

    def search(self, query: str, *, root: str = "Data", type_filter: str = "", limit: int = 100) -> Dict[str, Any]:
        params: Dict[str, Any] = {"query": query, "root": root, "limit": int(limit)}
        if type_filter:
            params["type"] = type_filter
        return self.engine.asset_browser_search(params)

    def find_one(self, query: str, *, root: str = "Data", type_filter: str = "", limit: int = 100) -> Optional[Mapping[str, Any]]:
        results = _entries(self.search(query, root=root, type_filter=type_filter, limit=limit), "results")
        return results[0] if results else None

    def ensure_folder(self, path: str) -> Dict[str, Any]:
        clean = path.replace("\\", "/").rstrip("/")
        parent, _, name = clean.rpartition("/")
        return self.engine.asset_browser_create_folder(parent=parent or "Data", name=name or clean)

    def copy(self, source: str, destination: str) -> Dict[str, Any]:
        return self.engine.asset_browser_copy(source=source, destination=destination)

    def move(self, source: str, destination: str) -> Dict[str, Any]:
        return self.engine.asset_browser_move(source=source, destination=destination)

    def rename(self, path: str, new_name: str) -> Dict[str, Any]:
        return self.engine.asset_browser_rename(path=path, newName=new_name)

    def trash(self, path: str) -> Dict[str, Any]:
        return self.engine.asset_browser_delete(path=path, permanent=False)

    def delete_permanently(self, path: str) -> Dict[str, Any]:
        return self.engine.asset_browser_delete(path=path, permanent=True)

    def place(
        self,
        path: str,
        *,
        name: Optional[str] = None,
        position: Vector3 = (0.0, 0.0, 0.0),
        rotation: Quaternion = (0.0, 0.0, 0.0, 1.0),
        scale: Vector3 = (1.0, 1.0, 1.0),
        parent: Optional[str] = None,
        select: bool = True,
    ) -> Dict[str, Any]:
        return self.root.place_asset(
            path=path,
            name=name,
            position=position,
            rotation=rotation,
            scale=scale,
            parent=parent,
            select=select,
        )


class EntityTools:
    """Entity, hierarchy, component, prefab, and transform operations."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def list(self) -> Any:
        return self.engine.list_entities()

    def all(self) -> List[Mapping[str, Any]]:
        return _entries(self.list(), "entities", "items", "data")

    def get(self, entity: str) -> Dict[str, Any]:
        return self.engine.get_entity(entity=entity)

    def find_by_name(self, name: str, *, exact: bool = False) -> List[Mapping[str, Any]]:
        needle = name.lower()
        found: List[Mapping[str, Any]] = []
        for entity in self.all():
            current = _entity_name(entity)
            haystack = current.lower()
            if (haystack == needle) if exact else (needle in haystack):
                found.append(entity)
        return found

    def first_by_name(self, name: str, *, exact: bool = False) -> Optional[Mapping[str, Any]]:
        matches = self.find_by_name(name, exact=exact)
        return matches[0] if matches else None

    def id_by_name(self, name: str, *, exact: bool = False) -> Optional[str]:
        entity = self.first_by_name(name, exact=exact)
        if not entity:
            return None
        value = entity.get("entity") or entity.get("id") or entity.get("entityId")
        return str(value) if value is not None else None

    def select(self, entity: str) -> Dict[str, Any]:
        return self.engine.select_entity(entity=entity)

    def select_by_name(self, name: str, *, exact: bool = False) -> Dict[str, Any]:
        entity = self.id_by_name(name, exact=exact)
        if not entity:
            raise RuntimeError(f"Entity not found by name: {name}")
        return self.select(entity)

    def create_empty(
        self,
        name: str,
        *,
        position: Vector3 = (0.0, 0.0, 0.0),
        rotation: Quaternion = (0.0, 0.0, 0.0, 1.0),
        scale: Vector3 = (1.0, 1.0, 1.0),
        parent: Optional[str] = None,
        select: bool = True,
    ) -> Dict[str, Any]:
        return self.root.place_entity(
            name=name,
            position=position,
            rotation=rotation,
            scale=scale,
            parent=parent,
            select=select,
        )

    def create_model(self, name: str, model_path: str, **kwargs: Any) -> Dict[str, Any]:
        return self.root.place_entity(name=name, model_path=model_path, **kwargs)

    def instantiate_prefab(self, name: str, prefab_path: str, **kwargs: Any) -> Dict[str, Any]:
        return self.root.place_entity(name=name, prefab_path=prefab_path, **kwargs)

    def set_transform(
        self,
        entity: str,
        *,
        position: Optional[Vector3] = None,
        rotation: Optional[Quaternion] = None,
        scale: Optional[Vector3] = None,
        record_undo: bool = True,
    ) -> Dict[str, Any]:
        params: Dict[str, Any] = {"entity": entity, "recordUndo": record_undo}
        if position is not None:
            params["position"] = _vec3(position)
        if rotation is not None:
            params["rotation"] = _quat(rotation)
        if scale is not None:
            params["scale"] = _vec3(scale, (1.0, 1.0, 1.0))
        return self.engine.set_transform(params)

    def add_component(self, entity: str, component_type: str, fields: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
        return self.engine.add_component(entity=entity, componentType=component_type, fields=dict(fields or {}))

    def remove_component(self, entity: str, component_type: str) -> Dict[str, Any]:
        return self.engine.remove_component(entity=entity, componentType=component_type)

    def get_component(self, entity: str, component_type: str) -> Dict[str, Any]:
        return self.engine.get_component(entity=entity, componentType=component_type)

    def component_schema(self, component_type: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.get_component_schema(_merge(componentType=component_type))

    def set_component_fields(self, entity: str, component_type: str, fields: Mapping[str, Any]) -> Dict[str, Any]:
        return self.engine.set_component_fields(entity=entity, componentType=component_type, fields=dict(fields))

    def duplicate(self, entity: str, *, select: bool = True) -> Dict[str, Any]:
        return self.engine.duplicate_entity(entity=entity, select=select)

    def reparent(self, entity: str, parent: Optional[str]) -> Dict[str, Any]:
        return self.engine.reparent_entity(entity=entity, parent=parent)

    def delete(self, entity: str, *, record_undo: bool = True) -> Dict[str, Any]:
        return self.engine.delete_entity(entity=entity, recordUndo=record_undo)

    def save_prefab(self, entity: str, *, path: str = "", directory: str = "Data/Prefabs") -> Dict[str, Any]:
        params = {"entity": entity}
        if path:
            params["path"] = path
        else:
            params["directory"] = directory
        return self.engine.prefab_save(params)

    def apply_prefab(self, entity: str) -> Dict[str, Any]:
        return self.engine.prefab_apply(entity=entity)

    def unpack_prefab(self, entity: str) -> Dict[str, Any]:
        return self.engine.prefab_unpack(entity=entity)

    def focus(self, entity: str, *, distance: Optional[float] = None) -> Dict[str, Any]:
        return self.engine.focus_entity(_merge(entity=entity, distance=distance))

    def frame_selection(self, *, distance: Optional[float] = None) -> Dict[str, Any]:
        return self.engine.frame_selection(_merge(distance=distance))

    def raycast_scene_view(self, screen_position: Sequence[float]) -> Dict[str, Any]:
        return self.engine.raycast_scene_view(screenPosition=_vec2(screen_position))

    def place_asset_at_cursor(self, path: str, *, screen_position: Optional[Sequence[float]] = None) -> Dict[str, Any]:
        return self.engine.place_asset_at_cursor(_merge(path=path, screenPosition=_vec2(screen_position) if screen_position else None))


class MaterialTools:
    """Material asset creation, update, and assignment helpers."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def create(self, path: str, fields: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
        return self.engine.material_create(path=path, fields=dict(fields or {}))

    def get(self, path: str) -> Dict[str, Any]:
        return self.engine.material_get(path=path)

    def set(self, path: str, fields: Mapping[str, Any]) -> Dict[str, Any]:
        return self.engine.material_set(path=path, fields=dict(fields))

    def create_or_update(self, path: str, fields: Mapping[str, Any]) -> Dict[str, Any]:
        try:
            return self.set(path, fields)
        except Exception:
            return self.create(path, fields)

    def assign(self, entity: str, path: str, *, mesh_index: Optional[int] = None) -> Dict[str, Any]:
        params = {"entity": entity, "path": path}
        if mesh_index is not None:
            params["meshIndex"] = int(mesh_index)
        return self.engine.material_assign(params)

    def create_toon(
        self,
        path: str,
        *,
        base_color: Sequence[float] = (1.0, 1.0, 1.0, 1.0),
        shadow_tint: Vector3 = (0.45, 0.48, 0.62),
        outline: bool = True,
    ) -> Dict[str, Any]:
        fields = {
            "baseColor": list(base_color),
            "toonShadingMode": 1,
            "toonShadowTint": _vec3(shadow_tint),
            "toonOutlineEnabled": bool(outline),
            "roughness": 0.78,
            "metallic": 0.0,
        }
        return self.create_or_update(path, fields)


class TerrainTools:
    """Terrain creation, sculpting, paint, foliage, and asset operations."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def create(
        self,
        *,
        name: str = "Terrain",
        position: Vector3 = (0.0, 0.0, 0.0),
        resolution: int = 256,
        world_size: Sequence[float] = (80.0, 80.0),
        height_scale: float = 18.0,
        chunk_count: Sequence[int] = (4, 4),
        generate_noise: bool = True,
        add_grass: bool = True,
        select: bool = True,
    ) -> Dict[str, Any]:
        params = {
            "name": name,
            "position": _vec3(position),
            "resolution": int(resolution),
            "worldSizeX": float(world_size[0]),
            "worldSizeZ": float(world_size[1]),
            "heightScale": float(height_scale),
            "chunkCountX": int(chunk_count[0]),
            "chunkCountZ": int(chunk_count[1]),
            "generateNoise": bool(generate_noise),
            "addGrass": bool(add_grass),
            "select": bool(select),
        }
        return self.engine.terrain_create(params)

    def list(self) -> Dict[str, Any]:
        return self.engine.terrain_list()

    def get(self, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_get(_merge(entity=entity))

    def open_editor(self, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_open(_merge(entity=entity))

    def set_brush(
        self,
        *,
        mode: str = "raise",
        radius: float = 4.0,
        strength: float = 0.5,
        falloff: Optional[float] = None,
        layer_index: Optional[int] = None,
        target_height: Optional[float] = None,
        enable: bool = True,
    ) -> Dict[str, Any]:
        return self.engine.terrain_set_brush(_merge(
            mode=mode,
            radius=radius,
            strength=strength,
            falloff=falloff,
            layerIndex=layer_index,
            targetHeight=target_height,
            enable=enable,
        ))

    def apply_brush(
        self,
        position: Vector3,
        *,
        entity: Optional[str] = None,
        mode: str = "raise",
        radius: float = 4.0,
        strength: float = 0.5,
        falloff: float = 1.0,
        layer_index: int = 0,
        target_height: float = 0.5,
    ) -> Dict[str, Any]:
        return self.engine.terrain_apply_brush(_merge(
            entity=entity,
            position=_vec3(position),
            mode=mode,
            radius=radius,
            strength=strength,
            falloff=falloff,
            layerIndex=layer_index,
            targetHeight=target_height,
        ))

    def configure(
        self,
        entity: Optional[str] = None,
        *,
        dimensions: Optional[Mapping[str, Any]] = None,
        noise: Optional[Mapping[str, Any]] = None,
        erosion: Optional[Mapping[str, Any]] = None,
        auto_splat: Optional[Mapping[str, Any]] = None,
        water: Optional[Mapping[str, Any]] = None,
        regenerate: bool = False,
    ) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        base = _merge(entity=entity)
        if dimensions:
            result["dimensions"] = self.engine.terrain_set_dimensions(_merge(base, **dict(dimensions)))
        if noise:
            result["noise"] = self.engine.terrain_set_noise(_merge(base, **dict(noise)))
        if erosion:
            result["erosion"] = self.engine.terrain_set_erosion(_merge(base, **dict(erosion)))
        if auto_splat:
            result["autoSplat"] = self.engine.terrain_set_auto_splat(_merge(base, **dict(auto_splat)))
        if water:
            result["water"] = self.engine.terrain_set_water(_merge(base, **dict(water)))
        if regenerate:
            result["regenerate"] = self.engine.terrain_regenerate(base)
        return result

    def set_dimensions(self, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_dimensions(_merge(entity=entity, **dict(fields)))

    def set_noise(self, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_noise(_merge(entity=entity, **dict(fields)))

    def set_erosion(self, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_erosion(_merge(entity=entity, **dict(fields)))

    def run_erosion(self, *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_run_erosion(_merge(entity=entity))

    def set_auto_splat(self, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_auto_splat(_merge(entity=entity, **dict(fields)))

    def set_water(self, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_water(_merge(entity=entity, **dict(fields)))

    def fit_water(self, *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_fit_water(_merge(entity=entity))

    def regenerate(self, *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_regenerate(_merge(entity=entity))

    def save(self, path: str, *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_save(_merge(entity=entity, path=path))

    def load(self, path: str, *, name: Optional[str] = None, select: bool = True) -> Dict[str, Any]:
        return self.engine.terrain_load(_merge(path=path, name=name, select=select))

    def apply_preset(self, preset: str, *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_apply_preset(entity=entity, preset=preset)

    def set_layer(self, index: int, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_layer(_merge(entity=entity, index=int(index), **dict(fields)))

    def get_foliage(self, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_get_foliage(_merge(entity=entity))

    def set_foliage(self, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_foliage(_merge(entity=entity, **dict(fields)))

    def add_foliage_layer(self, fields: Optional[Mapping[str, Any]] = None, *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_add_foliage_layer(_merge(entity=entity, **dict(fields or {})))

    def set_foliage_layer(self, index: int, fields: Mapping[str, Any], *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_set_foliage_layer(_merge(entity=entity, index=int(index), **dict(fields)))

    def delete_foliage_layer(self, index: int, *, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_delete_foliage_layer(_merge(entity=entity, index=int(index)))

    def reset_foliage(self, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.terrain_reset_foliage(_merge(entity=entity))


class EditorTools:
    """Editor panel, scene-view, game-view, undo, save, and capture helpers."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def panels(self) -> Dict[str, Any]:
        return self.engine.editor_get_panels()

    def focus(self, panel: str) -> Dict[str, Any]:
        return self.engine.editor_focus_panel(panel=panel)

    def show(self, panel: str, visible: bool = True) -> Dict[str, Any]:
        return self.engine.editor_show_panel(panel=panel, visible=visible)

    def undo(self) -> Dict[str, Any]:
        return self.engine.editor_undo()

    def redo(self) -> Dict[str, Any]:
        return self.engine.editor_redo()

    def new_scene(self, mode: str = "3D") -> Dict[str, Any]:
        return self.engine.scene_new(mode=mode)

    def save_scene(self, path: str) -> Dict[str, Any]:
        return self.engine.save_scene(path=path)

    def load_scene(self, path: str) -> Dict[str, Any]:
        return self.engine.load_scene(path=path)

    def scene_view_state(self) -> Dict[str, Any]:
        return self.engine.scene_view_get_state()

    def visual_state(self) -> Dict[str, Any]:
        return self.engine.get_visual_state()

    def engine_state(self) -> Dict[str, Any]:
        return self.engine.get_engine_state()

    def configure_scene_view(
        self,
        *,
        position: Optional[Vector3] = None,
        target: Optional[Vector3] = None,
        operation: Optional[str] = None,
        space: Optional[str] = None,
        shading: Optional[str] = None,
        mode: Optional[str] = None,
        visibility: Optional[Mapping[str, Any]] = None,
        grid: Optional[Mapping[str, Any]] = None,
        snap: Optional[Mapping[str, Any]] = None,
    ) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        if position is not None and target is not None:
            result["lookAt"] = self.engine.scene_view_set_look_at(position=_vec3(position), target=_vec3(target))
        elif position is not None:
            result["camera"] = self.engine.scene_view_set_camera(position=_vec3(position))
        if operation is not None:
            result["operation"] = self.engine.scene_view_set_gizmo_operation(operation=operation)
        if space is not None:
            result["space"] = self.engine.scene_view_set_gizmo_space(space=space)
        if shading is not None:
            result["shading"] = self.engine.scene_view_set_shading_mode(mode=shading)
        if mode is not None:
            result["mode"] = self.engine.scene_view_set_mode(mode=mode)
        if visibility:
            result["visibility"] = self.engine.scene_view_set_visibility(dict(visibility))
        if grid:
            result["grid"] = self.engine.scene_view_set_grid(dict(grid))
        if snap:
            result["snap"] = self.engine.scene_view_set_snap(dict(snap))
        result["state"] = self.scene_view_state()
        return result

    def bookmark(self, slot: int, *, save: bool = True) -> Dict[str, Any]:
        if save:
            return self.engine.scene_view_save_bookmark(slot=int(slot))
        return self.engine.scene_view_load_bookmark(slot=int(slot))

    def get_snap(self) -> Dict[str, Any]:
        return self.engine.scene_view_get_snap()

    def set_snap(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.scene_view_set_snap(fields)

    def set_grid(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.scene_view_set_grid(fields)

    def set_2d_view(self, *, center: Sequence[float] = (0.0, 0.0), zoom: float = 1.0) -> Dict[str, Any]:
        center2 = _vec2(center)
        return self.engine.scene_view_set_2d_view(centerX=center2[0], centerY=center2[1], zoom=float(zoom))

    def align_camera(self) -> Dict[str, Any]:
        return self.engine.scene_view_align_camera()

    def game_view_state(self) -> Dict[str, Any]:
        return self.engine.game_view_get_state()

    def set_game_view(
        self,
        *,
        resolution: str = "free",
        aspect: str = "fit",
        scale: str = "auto_fit",
        width: Optional[int] = None,
        height: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.engine.game_view_set_resolution(_merge(
            resolution=resolution,
            aspectPolicy=aspect,
            scalePolicy=scale,
            width=width,
            height=height,
        ))

    def set_game_overlay(self, **kwargs: Any) -> Dict[str, Any]:
        return self.engine.game_view_set_overlay(kwargs)

    def capture(self, path: Optional[str] = None, *, target: str = "scene_view") -> Dict[str, Any]:
        return self.root.frame_and_capture(path=path, target=target)


class LightingTools:
    """Lighting window, environment, post effects, and common light rigs."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def open(self) -> Dict[str, Any]:
        return self.engine.lighting_open()

    def get(self) -> Dict[str, Any]:
        return self.engine.lighting_get()

    def set_environment(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.lighting_set_environment(fields)

    def set_post_effects(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.lighting_set_post_effects(fields)

    def create_light_rig(self, **kwargs: Any) -> Dict[str, Any]:
        return self.root.create_light_rig(**kwargs)

    def create_light(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.light_create(fields)

    def create_camera(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.camera_create(fields)


class EffectEditorTools:
    """Effect graph authoring helpers."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def list_node_types(self) -> Dict[str, Any]:
        return self.engine.effect_editor_list_node_types()

    def create_asset(self, path: str, **fields: Any) -> Dict[str, Any]:
        return self.engine.effect_editor_create_asset(_merge(path=path, **fields))

    def open(self, path: str) -> Dict[str, Any]:
        return self.engine.effect_editor_open_workspace(path=path)

    def get(self) -> Dict[str, Any]:
        return self.engine.effect_editor_get_asset()

    def set_asset(self, fields: Mapping[str, Any]) -> Dict[str, Any]:
        return self.engine.effect_editor_set_asset(dict(fields))

    def add_node(
        self,
        node_type: str,
        *,
        position: Sequence[float] = (0.0, 0.0),
        fields: Optional[Mapping[str, Any]] = None,
    ) -> Dict[str, Any]:
        return self.engine.effect_editor_add_node(_merge(type=node_type, position=_vec2(position), **dict(fields or {})))

    def set_node(self, node_id: int, fields: Mapping[str, Any]) -> Dict[str, Any]:
        return self.engine.effect_editor_set_node(_merge(id=int(node_id), **dict(fields)))

    def delete_node(self, node_id: int) -> Dict[str, Any]:
        return self.engine.effect_editor_delete_node(id=int(node_id))

    def connect(self, from_node: int, from_pin: str, to_node: int, to_pin: str) -> Dict[str, Any]:
        return self.engine.effect_editor_connect(fromNode=int(from_node), fromPin=from_pin, toNode=int(to_node), toPin=to_pin)

    def disconnect(self, from_node: int, from_pin: str, to_node: int, to_pin: str) -> Dict[str, Any]:
        return self.engine.effect_editor_disconnect(fromNode=int(from_node), fromPin=from_pin, toNode=int(to_node), toPin=to_pin)

    def compile(self) -> Dict[str, Any]:
        return self.engine.effect_editor_compile()

    def preview_spawn(self, position: Vector3 = (0.0, 0.0, 0.0)) -> Dict[str, Any]:
        return self.engine.effect_editor_preview_spawn(position=_vec3(position))

    def timeline_play(self) -> Dict[str, Any]:
        return self.engine.effect_editor_timeline_play()

    def timeline_stop(self) -> Dict[str, Any]:
        return self.engine.effect_editor_timeline_stop()


class PlayerEditorTools:
    """Player editor, state machine, animation timeline, sockets, colliders, and input map."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def open(self, model_path: str = "", actor_mode: str = "Player") -> Dict[str, Any]:
        return self.engine.player_editor_open(_merge(modelPath=model_path, actorMode=actor_mode))

    def status(self) -> Dict[str, Any]:
        return self.engine.player_editor_get_status()

    def load_model(self, path: str) -> Dict[str, Any]:
        return self.engine.player_editor_load_model(path=path)

    def state_machine(self) -> Dict[str, Any]:
        return self.engine.player_editor_get_state_machine()

    def add_state(self, name: str, state_type: str = "Custom", *, position: Sequence[float] = (0.0, 0.0), **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_add_state(_merge(name=name, type=state_type, position=_vec2(position), **fields))

    def set_state(self, state_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_set_state(_merge(id=int(state_id), **fields))

    def delete_state(self, state_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_delete_state(id=int(state_id))

    def add_transition(self, from_state: int, to_state: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_add_transition(_merge(fromState=int(from_state), toState=int(to_state), **fields))

    def set_transition(self, transition_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_set_transition(_merge(id=int(transition_id), **fields))

    def delete_transition(self, transition_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_delete_transition(id=int(transition_id))

    def set_default_state(self, state_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_set_default_state(id=int(state_id))

    def timeline(self) -> Dict[str, Any]:
        return self.engine.player_editor_get_timeline()

    def add_track(self, track_type: str = "Custom", name: Optional[str] = None, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_add_track(_merge(type=track_type, name=name, **fields))

    def delete_track(self, track_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_delete_track(id=int(track_id))

    def select(
        self,
        *,
        state_id: Optional[int] = None,
        transition_id: Optional[int] = None,
        track_id: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.engine.player_editor_select(_merge(stateId=state_id, transitionId=transition_id, trackId=track_id))

    def set_playhead(self, frame: int) -> Dict[str, Any]:
        return self.engine.player_editor_set_playhead(frame=int(frame))

    def timeline_play(self, play: bool = True) -> Dict[str, Any]:
        return self.engine.player_editor_timeline_play(play=play)

    def save_prefab(self, save_as: bool = False) -> Dict[str, Any]:
        return self.engine.player_editor_save_prefab(saveAs=save_as)

    def sockets(self) -> Dict[str, Any]:
        return self.engine.player_editor_get_sockets()

    def add_socket(self, name: str, bone: str = "", **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_add_socket(_merge(name=name, bone=bone, **fields))

    def set_socket(self, socket_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_set_socket(_merge(id=int(socket_id), **fields))

    def delete_socket(self, socket_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_delete_socket(id=int(socket_id))

    def colliders(self) -> Dict[str, Any]:
        return self.engine.player_editor_get_colliders()

    def add_collider(self, name: str, shape: str = "Capsule", **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_add_collider(_merge(name=name, shape=shape, **fields))

    def set_collider(self, collider_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_set_collider(_merge(id=int(collider_id), **fields))

    def delete_collider(self, collider_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_delete_collider(id=int(collider_id))

    def animations(self) -> Dict[str, Any]:
        return self.engine.player_editor_get_animations()

    def set_animator(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_set_animator(fields)

    def input_map(self) -> Dict[str, Any]:
        return self.engine.player_editor_get_input_map()

    def add_action_binding(self, action: str, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_add_action_binding(_merge(action=action, **fields))

    def set_action_binding(self, binding_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_set_action_binding(_merge(id=int(binding_id), **fields))

    def delete_action_binding(self, binding_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_delete_action_binding(id=int(binding_id))

    def add_axis_binding(self, axis: str, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_add_axis_binding(_merge(axis=axis, **fields))

    def set_axis_binding(self, binding_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.player_editor_set_axis_binding(_merge(id=int(binding_id), **fields))

    def delete_axis_binding(self, binding_id: int) -> Dict[str, Any]:
        return self.engine.player_editor_delete_axis_binding(id=int(binding_id))


class UIEditorTools:
    """UI editor canvas, templates, parts, prefabs, and view controls."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def open(self) -> Dict[str, Any]:
        return self.engine.ui_editor_open()

    def state(self) -> Dict[str, Any]:
        return self.engine.ui_editor_get_state()

    def select(self, entity: str) -> Dict[str, Any]:
        return self.engine.ui_editor_select(entity=entity)

    def create_canvas(self) -> Dict[str, Any]:
        return self.engine.ui_editor_create_canvas()

    def create_template(self, kind: str = "player_hp") -> Dict[str, Any]:
        return self.engine.ui_editor_create_template(kind=kind)

    def create_part(self, kind: str = "image") -> Dict[str, Any]:
        return self.engine.ui_editor_create_part(kind=kind)

    def set_view(
        self,
        *,
        zoom: Optional[float] = None,
        pan: Optional[Sequence[float]] = None,
        grid_size: Optional[float] = None,
        show_grid: Optional[bool] = None,
        show_safe_area: Optional[bool] = None,
        snap_to_grid: Optional[bool] = None,
        pixel_snap: Optional[bool] = None,
    ) -> Dict[str, Any]:
        return self.engine.ui_editor_set_view(_merge(
            zoom=zoom,
            pan=_vec2(pan) if pan is not None else None,
            gridSize=grid_size,
            showGrid=show_grid,
            showSafeArea=show_safe_area,
            snapToGrid=snap_to_grid,
            pixelSnap=pixel_snap,
        ))

    def prefab(self, action: str = "save") -> Dict[str, Any]:
        if action == "save":
            return self.engine.ui_editor_save_prefab()
        if action == "apply":
            return self.engine.ui_editor_apply_prefab()
        if action == "revert":
            return self.engine.ui_editor_revert_prefab()
        if action == "unpack":
            return self.engine.ui_editor_unpack_prefab()
        raise ValueError(f"Unsupported UI prefab action: {action}")


class SequencerTools:
    """Cinematic sequencer operations."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def open(self) -> Dict[str, Any]:
        return self.engine.sequencer_open()

    def new(self) -> Dict[str, Any]:
        return self.engine.sequencer_new()

    def load(self, path: str) -> Dict[str, Any]:
        return self.engine.sequencer_load(path=path)

    def save(self, path: str = "") -> Dict[str, Any]:
        return self.engine.sequencer_save(path=path)

    def get(self) -> Dict[str, Any]:
        return self.engine.sequencer_get()

    def set_playback(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.sequencer_set_playback(fields)

    def set_params(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.sequencer_set_params(fields)

    def add_binding(self, entity: Optional[str] = None) -> Dict[str, Any]:
        return self.engine.sequencer_add_binding(_merge(entity=entity))

    def add_track(self, binding_id: int, track_type: str = "transform") -> Dict[str, Any]:
        return self.engine.sequencer_add_track(bindingId=int(binding_id), type=track_type)

    def add_master_track(self, track_type: str = "event") -> Dict[str, Any]:
        return self.engine.sequencer_add_master_track(type=track_type)

    def add_section(self, track_id: int, *, binding_id: int = 0, master: bool = False) -> Dict[str, Any]:
        return self.engine.sequencer_add_section(trackId=int(track_id), bindingId=int(binding_id), master=master)

    def select(self, *, binding_id: Optional[int] = None, track_id: Optional[int] = None, section_id: Optional[int] = None) -> Dict[str, Any]:
        return self.engine.sequencer_select(_merge(bindingId=binding_id, trackId=track_id, sectionId=section_id))


class GameLoopTools:
    """Game loop editor graph operations."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def open(self, path: str = "") -> Dict[str, Any]:
        return self.engine.game_loop_editor_open(_merge(path=path))

    def status(self) -> Dict[str, Any]:
        return self.engine.game_loop_editor_get_status()

    def get_asset(self) -> Dict[str, Any]:
        return self.engine.game_loop_editor_get_asset()

    def load(self, path: str) -> Dict[str, Any]:
        return self.engine.game_loop_editor_load(path=path)

    def save(self) -> Dict[str, Any]:
        return self.engine.game_loop_editor_save()

    def validate(self) -> Dict[str, Any]:
        return self.engine.game_loop_editor_validate()

    def add_node(
        self,
        name: str,
        node_type: str = "State",
        *,
        scene_path: str = "",
        position: Sequence[float] = (200.0, 200.0),
    ) -> Dict[str, Any]:
        return self.engine.game_loop_editor_add_node(name=name, type=node_type, scenePath=scene_path, position=_vec2(position))

    def set_node(self, node_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.game_loop_editor_set_node(_merge(id=int(node_id), **fields))

    def delete_node(self, node_id: int) -> Dict[str, Any]:
        return self.engine.game_loop_editor_delete_node(id=int(node_id))

    def add_transition(self, from_node_id: int, to_node_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.game_loop_editor_add_transition(_merge(fromNodeId=int(from_node_id), toNodeId=int(to_node_id), **fields))

    def set_transition(self, transition_id: int, **fields: Any) -> Dict[str, Any]:
        return self.engine.game_loop_editor_set_transition(_merge(id=int(transition_id), **fields))

    def delete_transition(self, transition_id: int) -> Dict[str, Any]:
        return self.engine.game_loop_editor_delete_transition(id=int(transition_id))

    def select(self, *, node_id: Optional[int] = None, transition_id: Optional[int] = None) -> Dict[str, Any]:
        return self.engine.game_loop_editor_select(_merge(nodeId=node_id, transitionId=transition_id))

    def set_start_node(self, node_id: int) -> Dict[str, Any]:
        return self.engine.game_loop_editor_set_start_node(nodeId=int(node_id))

    def reverse_transition(self, transition_id: int) -> Dict[str, Any]:
        return self.engine.game_loop_editor_reverse_transition(id=int(transition_id))

    def fit_graph(self) -> Dict[str, Any]:
        return self.engine.game_loop_editor_fit_graph()


class SerializerTools:
    """Model serializer entry point."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def build(self, **fields: Any) -> Dict[str, Any]:
        return self.engine.model_serializer_build(fields)


class RuntimeTools:
    """Play mode and stepping controls."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def ping(self) -> Dict[str, Any]:
        return self.engine.ping()

    def play(self) -> Dict[str, Any]:
        return self.engine.play()

    def stop(self) -> Dict[str, Any]:
        return self.engine.stop()

    def pause(self) -> Dict[str, Any]:
        return self.engine.pause()

    def step(self) -> Dict[str, Any]:
        return self.engine.step()


class VerificationTools:
    """Phase 5 verification entry point."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def validate(self, spec: Any) -> Dict[str, Any]:
        return VerificationRunner(self.root).validate(spec)

    def run(
        self,
        spec: Any,
        *,
        dry_run: bool = False,
        variables: Optional[Mapping[str, Any]] = None,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        return VerificationRunner(self.root).run(
            spec,
            dry_run=dry_run,
            variables=variables,
            report_path=report_path,
        )

    def image(self, path: str, **expectations: Any) -> Dict[str, Any]:
        return self.run({"checks": [{ "type": "image.bmp", "path": path, **expectations }]})

    def entity(self, name: str, *, exact: bool = False, minimum: int = 1) -> Dict[str, Any]:
        return self.run({"checks": [{ "type": "entity.exists", "name": name, "exact": exact, "min": minimum }]})

    def selected_visible(self, name: str = "", *, entity: str = "", exact: bool = False) -> Dict[str, Any]:
        check: Dict[str, Any] = {"type": "visual.selected_visible", "exact": exact}
        if name:
            check["name"] = name
        if entity:
            check["entity"] = entity
        return self.run({"checks": [check]})


class AutonomyTools:
    """Phase 6 autonomy entry point."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def validate(self, goal: Any) -> Dict[str, Any]:
        return AutonomyRunner(self.root).validate(goal)

    def plan(self, goal: Any) -> Dict[str, Any]:
        return AutonomyRunner(self.root).plan(AutonomyRunner(self.root)._normalize_goal(goal))

    def run(
        self,
        goal: Any,
        *,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        return AutonomyRunner(self.root).run(
            goal,
            dry_run=dry_run,
            max_repair_attempts=max_repair_attempts,
            report_path=report_path,
        )

    def make_simple_game(
        self,
        *,
        game_name: str = "AI_MiniCollect",
        coin_count: int = 5,
        hazard_count: int = 3,
        dry_run: bool = False,
    ) -> Dict[str, Any]:
        return self.run({
            "name": f"{game_name}_autonomy",
            "goal": "Create a simple collect-the-coins game scene with a start marker, goal marker, coins, hazards, camera, light, UI, saved scene, and verification.",
            "template": "simple_collect_game",
            "variables": {
                "gameName": game_name,
                "coinCount": int(coin_count),
                "hazardCount": int(hazard_count),
            },
            "reportPath": f"Saved/AI/autonomy/{game_name}_autonomy_latest.json",
        }, dry_run=dry_run)


class WorldTools:
    """Phase 7-9 ECS observation, semantic world model, and goal authoring tools."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def snapshot(self, *, include_details: bool = True, report_path: Optional[str] = None) -> Dict[str, Any]:
        return ECSObserver(self.root).snapshot(include_details=include_details, report_path=report_path)

    def semantic_summary(self, snapshot: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
        world = WorldModel(snapshot or self.snapshot(include_details=True))
        return world.semantic_summary()

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
        world = WorldModel(self.snapshot(include_details=True))
        return world.find(kind=kind, tag=tag, role=role, name=name, component=component, exact=exact)

    def validate_gameplay(
        self,
        snapshot: Optional[Mapping[str, Any]] = None,
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
        world = WorldModel(snapshot or self.snapshot(include_details=True))
        return world.validate_gameplay_loop(
            style=style,
            min_collectibles=min_collectibles,
            min_hazards=min_hazards,
            require_player=require_player,
            require_goal=require_goal,
            require_camera=require_camera,
            require_terrain=require_terrain,
            require_light=require_light,
        )

    def diff(self, before: Mapping[str, Any], after: Mapping[str, Any]) -> Dict[str, Any]:
        return diff_snapshots(before, after)

    def repair_plan(
        self,
        validation: Mapping[str, Any],
        *,
        snapshot: Optional[Mapping[str, Any]] = None,
        game_name: str = "AI_WorldGame",
        scene_path: Optional[str] = None,
        screenshot_path: Optional[str] = None,
        coin_count: int = 5,
        hazard_count: int = 0,
    ) -> Dict[str, Any]:
        world = WorldModel(snapshot or self.snapshot(include_details=True))
        return world.repair_plan(
            validation,
            game_name=game_name,
            scene_path=scene_path,
            screenshot_path=screenshot_path,
            coin_count=coin_count,
            hazard_count=hazard_count,
        )

    def run_goal(
        self,
        goal: Mapping[str, Any],
        *,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        return WorldAuthoringRunner(self.root).run(
            goal,
            dry_run=dry_run,
            max_repair_attempts=max_repair_attempts,
            report_path=report_path,
        )

    def make_collect_game(
        self,
        *,
        game_name: str = "AI_WorldCollect",
        coin_count: int = 5,
        hazard_count: int = 3,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
    ) -> Dict[str, Any]:
        return self.run_goal({
            "name": f"{game_name}_world_authoring",
            "goal": "Create a collect-the-coins scene and validate it through the ECS world model.",
            "template": "simple_collect_game",
            "variables": {
                "gameName": game_name,
                "coinCount": int(coin_count),
                "hazardCount": int(hazard_count),
            },
            "reportPath": f"Saved/AI/world/{game_name}_world_authoring_latest.json",
        }, dry_run=dry_run, max_repair_attempts=max_repair_attempts)


class EngineLinkTools:
    """Phase 10-12 AI-engine link: affordances, intent planning, session journal, orchestration."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def affordances(self, *, include_snapshot: bool = False) -> Dict[str, Any]:
        return EngineLinkRunner(self.root).affordances(include_snapshot=include_snapshot)

    def plan(self, goal: Mapping[str, Any], *, include_snapshot: bool = False) -> Dict[str, Any]:
        return EngineLinkRunner(self.root).plan(goal, include_snapshot=include_snapshot)

    def run(
        self,
        goal: Mapping[str, Any],
        *,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        return EngineLinkRunner(self.root).run(
            goal,
            dry_run=dry_run,
            max_repair_attempts=max_repair_attempts,
            report_path=report_path,
        )

    def make_collect_game(
        self,
        *,
        game_name: str = "AI_LinkedCollect",
        coin_count: int = 5,
        hazard_count: int = 3,
        dry_run: bool = False,
        max_repair_attempts: int = 1,
    ) -> Dict[str, Any]:
        return self.run({
            "name": f"{game_name}_engine_link",
            "goal": "Create a collect-the-coins scene through the Phase 12 AI-engine link.",
            "intent": "collect_game",
            "variables": {
                "gameName": game_name,
                "coinCount": int(coin_count),
                "hazardCount": int(hazard_count),
            },
            "reportPath": f"Saved/AI/link/{game_name}_link_latest.json",
            "journalPath": f"Saved/AI/link/{game_name}_journal_latest.json",
        }, dry_run=dry_run, max_repair_attempts=max_repair_attempts)


class WorkflowTools:
    """Phase 4-6 execution helpers: plan, run, verify, and checkpoint."""

    def __init__(self, root: "EngineTools"):
        self.root = root
        self.engine = root.engine

    def preflight(self) -> Dict[str, Any]:
        report = self.root.coverage_report()
        result: Dict[str, Any] = {
            "ping": self.engine.ping(),
            "coverage": report,
            "engine": self.engine.get_engine_state(),
            "sceneView": self.engine.scene_view_get_state(),
        }
        try:
            result["panels"] = self.engine.editor_get_panels()
        except Exception as exc:
            result["panelsError"] = str(exc)
        try:
            result["gameView"] = self.engine.game_view_get_state()
        except Exception as exc:
            result["gameViewError"] = str(exc)
        return result

    def snapshot(self, *, capture: bool = False, path: Optional[str] = None) -> Dict[str, Any]:
        result = self.root.inspect_scene()
        result["visual"] = self.engine.get_visual_state()
        if capture:
            result["capture"] = self.root.frame_and_capture(path=path)
        return result

    def registry(self) -> Dict[str, Callable[..., Any]]:
        registry: Dict[str, Callable[..., Any]] = {}
        families: Dict[str, Any] = {
            "asset": self.root.assets,
            "entity": self.root.entities,
            "material": self.root.materials,
            "terrain": self.root.terrain,
            "editor": self.root.editor,
            "lighting": self.root.lighting,
            "effect": self.root.effects,
            "player": self.root.player,
            "ui": self.root.ui,
            "sequencer": self.root.sequencer,
            "game_loop": self.root.game_loop,
            "serializer": self.root.serializer,
            "runtime": self.root.runtime,
            "verify": self.root.verify,
            "autonomy": self.root.autonomy,
            "world": self.root.world,
            "link": self.root.link,
        }
        for family_name, family in families.items():
            for method_name in dir(family):
                if method_name.startswith("_"):
                    continue
                method = getattr(family, method_name)
                if callable(method):
                    registry[f"{family_name}.{method_name}"] = method

        registry.update({
            "assets.list": self.root.assets.list,
            "assets.search": self.root.assets.search,
            "entities.create_empty": self.root.entities.create_empty,
            "entities.select_by_name": self.root.entities.select_by_name,
            "materials.create": self.root.materials.create,
            "materials.assign": self.root.materials.assign,
            "effects.open": self.root.effects.open,
            "lighting.rig": self.root.lighting.create_light_rig,
            "workflow.preflight": self.preflight,
            "workflow.snapshot": self.snapshot,
            "workflow.verify": self.verify_authoring_pass,
            "workflow.verify_authoring_pass": self.verify_authoring_pass,
            "workflow.checkpoint": self.checkpoint,
            "verification.run": self.root.verify.run,
            "verification.validate": self.root.verify.validate,
            "autonomy.run": self.root.autonomy.run,
            "autonomy.make_simple_game": self.root.autonomy.make_simple_game,
            "world.snapshot": self.root.world.snapshot,
            "world.semantic_summary": self.root.world.semantic_summary,
            "world.validate_gameplay": self.root.world.validate_gameplay,
            "world.run_goal": self.root.world.run_goal,
            "world.make_collect_game": self.root.world.make_collect_game,
            "link.affordances": self.root.link.affordances,
            "link.plan": self.root.link.plan,
            "link.run": self.root.link.run,
            "link.make_collect_game": self.root.link.make_collect_game,
        })
        return registry

    def validate_workflow(self, workflow: Any) -> Dict[str, Any]:
        return WorkflowRunner(self.registry()).validate(workflow)

    def execute_workflow(
        self,
        workflow: Any,
        *,
        dry_run: bool = False,
        continue_on_error: bool = False,
        variables: Optional[Mapping[str, Any]] = None,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        return WorkflowRunner(self.registry()).run(
            workflow,
            dry_run=dry_run,
            continue_on_error=continue_on_error,
            variables=variables,
            report_path=report_path,
        )

    def execute_steps(
        self,
        steps: Iterable[Mapping[str, Any]],
        *,
        dry_run: bool = False,
        continue_on_error: bool = False,
        variables: Optional[Mapping[str, Any]] = None,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        return self.execute_workflow(
            {"version": 1, "name": "steps", "steps": list(steps)},
            dry_run=dry_run,
            continue_on_error=continue_on_error,
            variables=variables,
            report_path=report_path,
        )

    def verify_authoring_pass(
        self,
        *,
        min_entities: int = 1,
        capture: bool = False,
        checks: Optional[List[Mapping[str, Any]]] = None,
        spec: Optional[Mapping[str, Any]] = None,
        name: str = "authoring_pass",
        variables: Optional[Mapping[str, Any]] = None,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        if spec is not None:
            return self.root.verify.run(spec, variables=variables, report_path=report_path)
        if checks is not None:
            return self.root.verify.run(
                {"name": name, "checks": checks},
                variables=variables,
                report_path=report_path,
            )
        entities = self.root.entities.all()
        checks = {
            "hasEntities": len(entities) >= min_entities,
            "entityCount": len(entities),
            "minEntities": min_entities,
        }
        result: Dict[str, Any] = {"checks": checks, "ok": all(bool(v) for k, v in checks.items() if k != "entityCount" and k != "minEntities")}
        if capture:
            result["capture"] = self.root.frame_and_capture()
        return result

    def checkpoint(
        self,
        *,
        scene_path: Optional[str] = None,
        screenshot_path: Optional[str] = None,
        target: str = "scene_view",
    ) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        if screenshot_path:
            result["capture"] = self.root.frame_and_capture(path=screenshot_path, target=target)
        if scene_path:
            result["saveScene"] = self.engine.save_scene(path=scene_path)
        return result


class EngineTools:
    """High-level authoring tools built on top of EngineClient."""

    def __init__(self, engine: EngineClient):
        self.engine = engine
        self.assets = AssetTools(self)
        self.entities = EntityTools(self)
        self.materials = MaterialTools(self)
        self.terrain = TerrainTools(self)
        self.editor = EditorTools(self)
        self.lighting = LightingTools(self)
        self.effects = EffectEditorTools(self)
        self.player = PlayerEditorTools(self)
        self.ui = UIEditorTools(self)
        self.sequencer = SequencerTools(self)
        self.game_loop = GameLoopTools(self)
        self.serializer = SerializerTools(self)
        self.runtime = RuntimeTools(self)
        self.verify = VerificationTools(self)
        self.autonomy = AutonomyTools(self)
        self.world = WorldTools(self)
        self.link = EngineLinkTools(self)
        self.workflow = WorkflowTools(self)

    def coverage_report(self) -> Dict[str, Any]:
        groups: Dict[str, List[str]] = {
            "core": ["ping", "play", "stop", "pause", "step"],
            "inspection": ["get_engine_state", "get_visual_state", "get_component_schema", "capture_screenshot"],
            "assets": [c for c in COMMANDS if c.startswith("asset_browser.")],
            "entities": [
                "list_entities", "get_entity", "get_component", "select_entity",
                "add_component", "remove_component", "set_component_fields",
                "create_empty", "create_model_entity", "set_transform", "delete_entity",
                "duplicate_entity", "reparent_entity", "instantiate_prefab",
                "focus_entity", "frame_selection", "raycast_scene_view", "place_asset_at_cursor",
            ],
            "prefab": [c for c in COMMANDS if c.startswith("prefab.")],
            "materials": [c for c in COMMANDS if c.startswith("material.")],
            "lighting": [c for c in COMMANDS if c.startswith("lighting.") or c in {"light.create", "camera.create"}],
            "terrain": [c for c in COMMANDS if c.startswith("terrain.")],
            "effects": [c for c in COMMANDS if c.startswith("effect_editor.")],
            "player": [c for c in COMMANDS if c.startswith("player_editor.")],
            "ui": [c for c in COMMANDS if c.startswith("ui_editor.")],
            "sequencer": [c for c in COMMANDS if c.startswith("sequencer.")],
            "game_loop": [c for c in COMMANDS if c.startswith("game_loop_editor.")],
            "editor": [c for c in COMMANDS if c.startswith("editor.") or c.startswith("scene_view.") or c.startswith("game_view.") or c in {"scene.new", "save_scene", "load_scene"}],
            "serializer": [c for c in COMMANDS if c.startswith("model_serializer.")],
        }
        covered = {command for commands in groups.values() for command in commands}
        missing = [command for command in COMMANDS if command not in covered]
        return {
            "totalCommands": len(COMMANDS),
            "coveredCommands": len(covered & set(COMMANDS)),
            "missingCommands": missing,
            "families": {name: len(commands) for name, commands in groups.items()},
            "phase3Complete": not missing,
        }

    def inspect_scene(self) -> Dict[str, Any]:
        """Return the core state needed before an AI edit pass."""
        return {
            "engine": self.engine.get_engine_state(),
            "sceneView": self.engine.scene_view_get_state(),
            "entities": self.engine.list_entities(),
        }

    def place_entity(
        self,
        *,
        name: str,
        position: Vector3 = (0.0, 0.0, 0.0),
        rotation: Quaternion = (0.0, 0.0, 0.0, 1.0),
        scale: Vector3 = (1.0, 1.0, 1.0),
        model_path: Optional[str] = None,
        prefab_path: Optional[str] = None,
        parent: Optional[str] = None,
        select: bool = True,
        record_undo: bool = True,
    ) -> Dict[str, Any]:
        """Place an empty, model, or prefab entity and normalize its transform."""
        pos = _vec3(position)
        rot = _quat(rotation)
        scl = _vec3(scale, (1.0, 1.0, 1.0))

        if prefab_path and model_path:
            raise ValueError("Specify either prefab_path or model_path, not both.")

        if prefab_path:
            created = self.engine.instantiate_prefab({
                "path": prefab_path,
                "position": pos,
                "parent": parent,
                "select": select,
            })
            kind = "prefab"
        elif model_path:
            created = self.engine.create_model_entity({
                "name": name,
                "modelFilePath": model_path,
                "position": pos,
                "rotation": rot,
                "scale": scl,
                "parent": parent,
                "select": select,
                "recordUndo": record_undo,
            })
            kind = "model"
        else:
            created = self.engine.create_empty({
                "name": name,
                "position": pos,
                "rotation": rot,
                "scale": scl,
                "parent": parent,
                "select": select,
                "recordUndo": record_undo,
            })
            kind = "empty"

        entity = _entity_id(created)
        transform = self.engine.set_transform({
            "entity": entity,
            "position": pos,
            "rotation": rot,
            "scale": scl,
            "recordUndo": record_undo,
        })
        if select:
            self.engine.select_entity(entity=entity)

        return {
            "kind": kind,
            "name": name,
            "entity": entity,
            "created": created,
            "transform": transform,
        }

    def place_asset(
        self,
        *,
        path: str,
        name: Optional[str] = None,
        position: Vector3 = (0.0, 0.0, 0.0),
        rotation: Quaternion = (0.0, 0.0, 0.0, 1.0),
        scale: Vector3 = (1.0, 1.0, 1.0),
        parent: Optional[str] = None,
        select: bool = True,
    ) -> Dict[str, Any]:
        """Place a prefab or model by inferring the asset type from its extension."""
        kind = _asset_kind(path)
        label = name or Path(path).stem
        if kind == "prefab":
            return self.place_entity(
                name=label,
                prefab_path=path,
                position=position,
                rotation=rotation,
                scale=scale,
                parent=parent,
                select=select,
            )
        return self.place_entity(
            name=label,
            model_path=path,
            position=position,
            rotation=rotation,
            scale=scale,
            parent=parent,
            select=select,
        )

    def create_marker_group(
        self,
        *,
        root_name: str,
        markers: Iterable[Mapping[str, Any]],
        position: Vector3 = (0.0, 0.0, 0.0),
        select_last: bool = False,
    ) -> Dict[str, Any]:
        """Create a parent empty and child marker empties."""
        root = self.place_entity(name=root_name, position=position, select=False)
        root_id = root["entity"]
        children = []
        for marker in markers:
            marker_name = str(marker.get("name", "Marker"))
            marker_pos = _vec3(marker.get("position"), (0.0, 0.0, 0.0))
            child = self.place_entity(
                name=marker_name,
                position=marker_pos,
                parent=root_id,
                select=select_last,
            )
            children.append(child)
        return {"root": root, "markers": children}

    def create_light_rig(
        self,
        *,
        name: str = "AI_LightRig",
        key_color: Vector3 = (1.0, 0.92, 0.78),
        key_intensity: float = 2.0,
        fill_color: Vector3 = (0.55, 0.65, 1.0),
        fill_intensity: float = 0.55,
        select: bool = False,
    ) -> Dict[str, Any]:
        """Create a small key/fill lighting setup."""
        key = self.engine.light_create({
            "name": f"{name}_Key",
            "type": "Directional",
            "color": _vec3(key_color, (1.0, 0.92, 0.78)),
            "intensity": float(key_intensity),
            "castShadow": True,
            "select": select,
        })
        fill = self.engine.light_create({
            "name": f"{name}_Fill",
            "type": "Point",
            "position": [0.0, 4.0, -5.0],
            "color": _vec3(fill_color, (0.55, 0.65, 1.0)),
            "intensity": float(fill_intensity),
            "range": 18.0,
            "castShadow": False,
            "select": select,
        })
        return {"key": key, "fill": fill}

    def setup_scene_view(
        self,
        *,
        position: Vector3 = (0.0, 5.0, -10.0),
        target: Vector3 = (0.0, 0.0, 0.0),
        shading_mode: str = "lit",
        mode: str = "3D",
        move_speed: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Set the editor scene view to a useful inspectable camera."""
        look = self.engine.scene_view_set_look_at({
            "position": _vec3(position),
            "target": _vec3(target),
        })
        shading = self.engine.scene_view_set_shading_mode(mode=shading_mode)
        view_mode = self.engine.scene_view_set_mode(mode=mode)
        speed = None
        if move_speed is not None:
            speed = self.engine.scene_view_set_camera({
                "position": _vec3(position),
                "moveSpeed": float(move_speed),
            })
        return {
            "lookAt": look,
            "shading": shading,
            "mode": view_mode,
            "moveSpeed": speed,
            "state": self.engine.scene_view_get_state(),
        }

    def frame_and_capture(
        self,
        *,
        entity: Optional[str] = None,
        path: Optional[str] = None,
        target: str = "scene_view",
        distance: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Optionally frame an entity, then capture the current view."""
        focused = None
        if entity is not None:
            params: Dict[str, Any] = {"entity": entity}
            if distance is not None:
                params["distance"] = float(distance)
            focused = self.engine.focus_entity(params)

        if path is None:
            stamp = int(time.time() * 1000)
            path = f"Saved/AI/screenshots/tool_capture_{stamp}.bmp"

        capture = self.engine.capture_screenshot({
            "target": target,
            "path": path,
        })
        return {
            "focused": focused,
            "capture": capture,
            "state": self.engine.get_visual_state(),
        }

    def create_battle_layout(
        self,
        *,
        name: str = "AI_BattleLayout",
        center: Vector3 = (0.0, 0.0, 0.0),
        radius: float = 6.0,
        enemy_count: int = 3,
        create_camera: bool = True,
        create_lights: bool = True,
        capture: bool = False,
    ) -> Dict[str, Any]:
        """Create a simple editable combat layout made from named marker entities."""
        cx, cy, cz = _vec3(center)
        enemy_total = max(0, int(enemy_count))

        markers: List[Dict[str, Any]] = [
            {"name": f"{name}_PlayerStart", "position": [0.0, 0.0, -radius * 0.55]},
            {"name": f"{name}_CameraAnchor", "position": [0.0, 3.0, -radius]},
        ]
        for i in range(enemy_total):
            angle = (math.tau * i / max(enemy_total, 1)) + math.pi * 0.25
            markers.append({
                "name": f"{name}_EnemySpawn_{i + 1:02d}",
                "position": [
                    math.cos(angle) * radius,
                    0.0,
                    math.sin(angle) * radius,
                ],
            })

        group = self.create_marker_group(root_name=name, markers=markers, position=center)

        camera = None
        if create_camera:
            camera = self.engine.camera_create({
                "name": f"{name}_PreviewCamera",
                "position": [cx, cy + radius * 0.65, cz - radius * 1.25],
                "fovY": 0.785398,
                "nearZ": 0.1,
                "farZ": 100000.0,
                "main": True,
                "select": False,
            })

        lights = self.create_light_rig(name=f"{name}_LightRig", select=False) if create_lights else None

        view = self.setup_scene_view(
            position=[cx, cy + radius * 0.75, cz - radius * 1.35],
            target=[cx, cy, cz],
            shading_mode="lit",
            mode="3D",
        )

        capture_result = None
        if capture:
            capture_result = self.frame_and_capture(path=f"Saved/AI/screenshots/{name}_layout.bmp")

        return {
            "layout": name,
            "root": group["root"],
            "markers": group["markers"],
            "camera": camera,
            "lights": lights,
            "sceneView": view,
            "capture": capture_result,
        }
