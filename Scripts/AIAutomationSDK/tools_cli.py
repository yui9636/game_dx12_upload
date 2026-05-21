from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Optional

from autonomy_runner import AutonomyValidationError
from engine_client import DEFAULT_TIMEOUT, DEFAULT_URL, EngineClient
from engine_client import EngineCommandError, EngineConnectionError
from tools import EngineTools
from verification_runner import VerificationValidationError
from workflow_runner import WorkflowValidationError


def vec3(values: list[str]) -> list[float]:
    if len(values) != 3:
        raise argparse.ArgumentTypeError("Expected exactly three numbers.")
    return [float(v) for v in values]


def print_json(value: Any) -> None:
    print(json.dumps(value, ensure_ascii=False, indent=2))


def json_object(text: Optional[str]) -> dict[str, Any]:
    if not text:
        return {}
    value = json.loads(text)
    if not isinstance(value, dict):
        raise argparse.ArgumentTypeError("Expected a JSON object.")
    return value


def load_json_value(path: str) -> Any:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def parse_variables(values: Optional[list[str]]) -> dict[str, Any]:
    parsed: dict[str, Any] = {}
    for item in values or []:
        if "=" not in item:
            raise argparse.ArgumentTypeError("--var values must use KEY=VALUE.")
        key, raw_value = item.split("=", 1)
        key = key.strip()
        if not key:
            raise argparse.ArgumentTypeError("--var key cannot be empty.")
        try:
            parsed[key] = json.loads(raw_value)
        except json.JSONDecodeError:
            parsed[key] = raw_value
    return parsed


def run_inspect(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).inspect_scene())
    return 0


def run_coverage(args: argparse.Namespace) -> int:
    engine = EngineClient(args.url, timeout=args.timeout)
    print_json(EngineTools(engine).coverage_report())
    return 0


def run_preflight(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).workflow.preflight())
    return 0


def run_place_empty(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        result = tools.place_entity(
            name=args.name,
            position=args.position,
            select=not args.no_select,
        )
        print_json(result)
    return 0


def run_place_asset(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        result = tools.place_asset(
            path=args.path,
            name=args.name,
            position=args.position,
            scale=args.scale,
            select=not args.no_select,
        )
        print_json(result)
    return 0


def run_camera(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        result = tools.setup_scene_view(
            position=args.position,
            target=args.target,
            shading_mode=args.shading,
            mode=args.mode,
            move_speed=args.move_speed,
        )
        print_json(result)
    return 0


def run_capture(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        result = tools.frame_and_capture(
            entity=args.entity,
            path=args.path,
            target=args.target,
            distance=args.distance,
        )
        print_json(result)
    return 0


def run_battle_layout(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        result = tools.create_battle_layout(
            name=args.name,
            center=args.center,
            radius=args.radius,
            enemy_count=args.enemy_count,
            create_camera=not args.no_camera,
            create_lights=not args.no_lights,
            capture=args.capture,
        )
        print_json(result)
    return 0


def run_asset_list(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).assets.list(args.path, type_filter=args.type))
    return 0


def run_asset_search(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).assets.search(args.query, root=args.root, type_filter=args.type, limit=args.limit))
    return 0


def run_asset_folder(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).assets.ensure_folder(args.path))
    return 0


def run_entity_find(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        print_json(tools.entities.find_by_name(args.name, exact=args.exact))
    return 0


def run_entity_select(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        print_json(tools.entities.select_by_name(args.name, exact=args.exact))
    return 0


def run_material_create(args: argparse.Namespace) -> int:
    fields = json_object(args.fields)
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).materials.create_or_update(args.path, fields))
    return 0


def run_material_assign(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).materials.assign(args.entity, args.path, mesh_index=args.mesh_index))
    return 0


def run_terrain_create(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).terrain.create(
            name=args.name,
            position=args.position,
            resolution=args.resolution,
            world_size=args.world_size,
            height_scale=args.height_scale,
            chunk_count=args.chunk_count,
            generate_noise=not args.no_noise,
            add_grass=not args.no_grass,
            select=not args.no_select,
        ))
    return 0


def run_terrain_brush(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).terrain.apply_brush(
            args.position,
            entity=args.entity,
            mode=args.mode,
            radius=args.radius,
            strength=args.strength,
            falloff=args.falloff,
            layer_index=args.layer_index,
            target_height=args.target_height,
        ))
    return 0


def run_editor_panels(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).editor.panels())
    return 0


def run_editor_focus(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        result = {"show": tools.editor.show(args.panel, True) if args.show else None}
        result["focus"] = tools.editor.focus(args.panel)
        print_json(result)
    return 0


def run_editor_save_scene(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).editor.save_scene(args.path))
    return 0


def run_ui_template(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        print_json({
            "open": tools.ui.open(),
            "canvas": tools.ui.create_canvas() if args.canvas else None,
            "template": tools.ui.create_template(args.kind),
            "state": tools.ui.state(),
        })
    return 0


def run_player_open(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).player.open(model_path=args.model_path or "", actor_mode=args.actor_mode))
    return 0


def run_player_status(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).player.status())
    return 0


def run_game_loop_open(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        print_json({"open": tools.game_loop.open(args.path or ""), "status": tools.game_loop.status()})
    return 0


def run_game_loop_validate(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).game_loop.validate())
    return 0


def run_sequencer(args: argparse.Namespace) -> int:
    with EngineClient(args.url, timeout=args.timeout) as engine:
        tools = EngineTools(engine)
        if args.action == "new":
            print_json({"open": tools.sequencer.open(), "sequence": tools.sequencer.new()})
        else:
            print_json({"open": tools.sequencer.open(), "sequence": tools.sequencer.get()})
    return 0


def run_workflow(args: argparse.Namespace) -> int:
    value = load_json_value(args.file)
    variables = parse_variables(args.var)
    if args.dry_run:
        engine = EngineClient(args.url, timeout=args.timeout)
        print_json(EngineTools(engine).workflow.execute_workflow(
            value,
            dry_run=True,
            variables=variables,
            report_path=args.report,
        ))
        return 0
    with EngineClient(args.url, timeout=args.timeout) as engine:
        print_json(EngineTools(engine).workflow.execute_workflow(
            value,
            dry_run=False,
            continue_on_error=args.continue_on_error,
            variables=variables,
            report_path=args.report,
        ))
    return 0


def run_workflow_validate(args: argparse.Namespace) -> int:
    value = load_json_value(args.file)
    engine = EngineClient(args.url, timeout=args.timeout)
    result = EngineTools(engine).workflow.validate_workflow(value)
    print_json(result)
    return 0 if result.get("ok") else 1


def run_verify(args: argparse.Namespace) -> int:
    value = load_json_value(args.file)
    variables = parse_variables(args.var)
    if args.dry_run:
        engine = EngineClient(args.url, timeout=args.timeout)
        print_json(EngineTools(engine).verify.run(
            value,
            dry_run=True,
            variables=variables,
            report_path=args.report,
        ))
        return 0
    with EngineClient(args.url, timeout=args.timeout) as engine:
        result = EngineTools(engine).verify.run(
            value,
            variables=variables,
            report_path=args.report,
        )
        print_json(result)
        return 0 if result.get("summary", {}).get("ok") else 5


def run_verify_validate(args: argparse.Namespace) -> int:
    value = load_json_value(args.file)
    engine = EngineClient(args.url, timeout=args.timeout)
    result = EngineTools(engine).verify.validate(value)
    print_json(result)
    return 0 if result.get("ok") else 1


def run_verify_image(args: argparse.Namespace) -> int:
    spec = {
        "name": "image_verify",
        "checks": [{
            "type": "image.bmp",
            "path": args.path,
            "minBytes": args.min_bytes,
            "minWidth": args.min_width,
            "minHeight": args.min_height,
            "minUniqueColors": args.min_unique_colors,
            "minNonBlackRatio": args.min_non_black_ratio,
        }],
    }
    with EngineClient(args.url, timeout=args.timeout) as engine:
        result = EngineTools(engine).verify.run(spec)
        print_json(result)
        return 0 if result.get("summary", {}).get("ok") else 5


def run_autonomy(args: argparse.Namespace) -> int:
    value = load_json_value(args.file)
    if args.dry_run:
        engine = EngineClient(args.url, timeout=args.timeout)
        result = EngineTools(engine).autonomy.run(
            value,
            dry_run=True,
            max_repair_attempts=args.max_repair_attempts,
            report_path=args.report,
        )
        print_json(result)
        return 0
    with EngineClient(args.url, timeout=args.timeout) as engine:
        result = EngineTools(engine).autonomy.run(
            value,
            dry_run=False,
            max_repair_attempts=args.max_repair_attempts,
            report_path=args.report,
        )
        print_json(result)
        return 0 if result.get("summary", {}).get("ok") else 6


def run_autonomy_validate(args: argparse.Namespace) -> int:
    value = load_json_value(args.file)
    engine = EngineClient(args.url, timeout=args.timeout)
    result = EngineTools(engine).autonomy.validate(value)
    print_json(result)
    return 0 if result.get("ok") else 1


def run_make_simple_game(args: argparse.Namespace) -> int:
    if args.dry_run:
        engine = EngineClient(args.url, timeout=args.timeout)
        result = EngineTools(engine).autonomy.make_simple_game(
            game_name=args.name,
            coin_count=args.coins,
            hazard_count=args.hazards,
            dry_run=True,
        )
        print_json(result)
        return 0
    with EngineClient(args.url, timeout=args.timeout) as engine:
        result = EngineTools(engine).autonomy.make_simple_game(
            game_name=args.name,
            coin_count=args.coins,
            hazard_count=args.hazards,
            dry_run=False,
        )
        print_json(result)
        return 0 if result.get("summary", {}).get("ok") else 6


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="High-level authoring tools for MyEngine automation.")
    parser.add_argument("--url", default=DEFAULT_URL, help=f"WebSocket URL. Default: {DEFAULT_URL}")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="Socket timeout in seconds.")

    sub = parser.add_subparsers(dest="tool", required=True)

    inspect = sub.add_parser("inspect", help="Read engine, scene view, and entity state.")
    inspect.set_defaults(func=run_inspect)

    coverage = sub.add_parser("coverage", help="Report Phase 3 command-family coverage.")
    coverage.set_defaults(func=run_coverage)

    preflight = sub.add_parser("preflight", help="Run Phase 4-6 preflight checks.")
    preflight.set_defaults(func=run_preflight)

    empty = sub.add_parser("place-empty", help="Place a named empty marker.")
    empty.add_argument("--name", required=True)
    empty.add_argument("--position", nargs=3, type=float, default=[0.0, 0.0, 0.0])
    empty.add_argument("--no-select", action="store_true")
    empty.set_defaults(func=run_place_empty)

    asset = sub.add_parser("place-asset", help="Place a model or prefab asset by extension.")
    asset.add_argument("--path", required=True)
    asset.add_argument("--name")
    asset.add_argument("--position", nargs=3, type=float, default=[0.0, 0.0, 0.0])
    asset.add_argument("--scale", nargs=3, type=float, default=[1.0, 1.0, 1.0])
    asset.add_argument("--no-select", action="store_true")
    asset.set_defaults(func=run_place_asset)

    asset_list = sub.add_parser("asset-list", help="List assets in a folder.")
    asset_list.add_argument("--path", default="Data")
    asset_list.add_argument("--type", default="")
    asset_list.set_defaults(func=run_asset_list)

    asset_search = sub.add_parser("asset-search", help="Search assets by name/path.")
    asset_search.add_argument("--query", required=True)
    asset_search.add_argument("--root", default="Data")
    asset_search.add_argument("--type", default="")
    asset_search.add_argument("--limit", type=int, default=100)
    asset_search.set_defaults(func=run_asset_search)

    asset_folder = sub.add_parser("asset-folder", help="Create an asset folder if missing.")
    asset_folder.add_argument("--path", required=True)
    asset_folder.set_defaults(func=run_asset_folder)

    entity_find = sub.add_parser("entity-find", help="Find entities by name.")
    entity_find.add_argument("--name", required=True)
    entity_find.add_argument("--exact", action="store_true")
    entity_find.set_defaults(func=run_entity_find)

    entity_select = sub.add_parser("entity-select", help="Select the first entity matching a name.")
    entity_select.add_argument("--name", required=True)
    entity_select.add_argument("--exact", action="store_true")
    entity_select.set_defaults(func=run_entity_select)

    material_create = sub.add_parser("material-create", help="Create or update a material asset.")
    material_create.add_argument("--path", required=True)
    material_create.add_argument("--fields", default="{}")
    material_create.set_defaults(func=run_material_create)

    material_assign = sub.add_parser("material-assign", help="Assign a material asset to an entity.")
    material_assign.add_argument("--entity", required=True)
    material_assign.add_argument("--path", required=True)
    material_assign.add_argument("--mesh-index", type=int)
    material_assign.set_defaults(func=run_material_assign)

    terrain_create = sub.add_parser("terrain-create", help="Create a terrain entity.")
    terrain_create.add_argument("--name", default="AI_Terrain")
    terrain_create.add_argument("--position", nargs=3, type=float, default=[0.0, 0.0, 0.0])
    terrain_create.add_argument("--resolution", type=int, default=256)
    terrain_create.add_argument("--world-size", nargs=2, type=float, default=[80.0, 80.0])
    terrain_create.add_argument("--height-scale", type=float, default=18.0)
    terrain_create.add_argument("--chunk-count", nargs=2, type=int, default=[4, 4])
    terrain_create.add_argument("--no-noise", action="store_true")
    terrain_create.add_argument("--no-grass", action="store_true")
    terrain_create.add_argument("--no-select", action="store_true")
    terrain_create.set_defaults(func=run_terrain_create)

    terrain_brush = sub.add_parser("terrain-brush", help="Apply a terrain brush stroke.")
    terrain_brush.add_argument("--entity")
    terrain_brush.add_argument("--position", nargs=3, type=float, required=True)
    terrain_brush.add_argument("--mode", default="raise")
    terrain_brush.add_argument("--radius", type=float, default=4.0)
    terrain_brush.add_argument("--strength", type=float, default=0.5)
    terrain_brush.add_argument("--falloff", type=float, default=1.0)
    terrain_brush.add_argument("--layer-index", type=int, default=0)
    terrain_brush.add_argument("--target-height", type=float, default=0.5)
    terrain_brush.set_defaults(func=run_terrain_brush)

    editor_panels = sub.add_parser("editor-panels", help="List editor panel visibility.")
    editor_panels.set_defaults(func=run_editor_panels)

    editor_focus = sub.add_parser("editor-focus", help="Focus an editor panel.")
    editor_focus.add_argument("--panel", required=True)
    editor_focus.add_argument("--show", action="store_true")
    editor_focus.set_defaults(func=run_editor_focus)

    editor_save_scene = sub.add_parser("save-scene", help="Save the current scene.")
    editor_save_scene.add_argument("--path", required=True)
    editor_save_scene.set_defaults(func=run_editor_save_scene)

    camera = sub.add_parser("camera-look-at", help="Set Scene View camera to look at a point.")
    camera.add_argument("--position", nargs=3, type=float, default=[0.0, 5.0, -10.0])
    camera.add_argument("--target", nargs=3, type=float, default=[0.0, 0.0, 0.0])
    camera.add_argument("--shading", default="lit")
    camera.add_argument("--mode", default="3D")
    camera.add_argument("--move-speed", type=float)
    camera.set_defaults(func=run_camera)

    capture = sub.add_parser("capture", help="Frame an optional entity and capture the view.")
    capture.add_argument("--entity")
    capture.add_argument("--path")
    capture.add_argument("--target", default="scene_view")
    capture.add_argument("--distance", type=float)
    capture.set_defaults(func=run_capture)

    layout = sub.add_parser("battle-layout", help="Create a simple editable combat layout.")
    layout.add_argument("--name", default="AI_BattleLayout")
    layout.add_argument("--center", nargs=3, type=float, default=[0.0, 0.0, 0.0])
    layout.add_argument("--radius", type=float, default=6.0)
    layout.add_argument("--enemy-count", type=int, default=3)
    layout.add_argument("--no-camera", action="store_true")
    layout.add_argument("--no-lights", action="store_true")
    layout.add_argument("--capture", action="store_true")
    layout.set_defaults(func=run_battle_layout)

    ui_template = sub.add_parser("ui-template", help="Open UI editor and create a template.")
    ui_template.add_argument("--kind", default="player_hp")
    ui_template.add_argument("--canvas", action="store_true")
    ui_template.set_defaults(func=run_ui_template)

    player_open = sub.add_parser("player-open", help="Open Player Editor.")
    player_open.add_argument("--model-path")
    player_open.add_argument("--actor-mode", default="Player")
    player_open.set_defaults(func=run_player_open)

    player_status = sub.add_parser("player-status", help="Read Player Editor status.")
    player_status.set_defaults(func=run_player_status)

    game_loop_open = sub.add_parser("game-loop-open", help="Open Game Loop Editor.")
    game_loop_open.add_argument("--path")
    game_loop_open.set_defaults(func=run_game_loop_open)

    game_loop_validate = sub.add_parser("game-loop-validate", help="Validate the current Game Loop graph.")
    game_loop_validate.set_defaults(func=run_game_loop_validate)

    sequencer = sub.add_parser("sequencer", help="Open or create a cinematic sequence.")
    sequencer.add_argument("action", choices=["new", "get"])
    sequencer.set_defaults(func=run_sequencer)

    workflow = sub.add_parser("workflow", help="Run a high-level workflow JSON file.")
    workflow.add_argument("--file", required=True)
    workflow.add_argument("--dry-run", action="store_true")
    workflow.add_argument("--continue-on-error", action="store_true")
    workflow.add_argument("--report", help="Optional path for a JSON execution report.")
    workflow.add_argument("--var", action="append", help="Override workflow variables with KEY=JSON_VALUE.")
    workflow.set_defaults(func=run_workflow)

    workflow_validate = sub.add_parser("workflow-validate", help="Validate a workflow JSON file without running it.")
    workflow_validate.add_argument("--file", required=True)
    workflow_validate.set_defaults(func=run_workflow_validate)

    verify = sub.add_parser("verify", help="Run a Phase 5 verification JSON file.")
    verify.add_argument("--file", required=True)
    verify.add_argument("--dry-run", action="store_true")
    verify.add_argument("--report", help="Optional path for a JSON verification report.")
    verify.add_argument("--var", action="append", help="Override verification variables with KEY=JSON_VALUE.")
    verify.set_defaults(func=run_verify)

    verify_validate = sub.add_parser("verify-validate", help="Validate a Phase 5 verification JSON file.")
    verify_validate.add_argument("--file", required=True)
    verify_validate.set_defaults(func=run_verify_validate)

    verify_image = sub.add_parser("verify-image", help="Verify a BMP screenshot file.")
    verify_image.add_argument("--path", required=True)
    verify_image.add_argument("--min-bytes", type=int, default=1024)
    verify_image.add_argument("--min-width", type=int, default=1)
    verify_image.add_argument("--min-height", type=int, default=1)
    verify_image.add_argument("--min-unique-colors", type=int, default=1)
    verify_image.add_argument("--min-non-black-ratio", type=float, default=0.0)
    verify_image.set_defaults(func=run_verify_image)

    autonomy = sub.add_parser("autonomy", help="Run a Phase 6 autonomy goal JSON file.")
    autonomy.add_argument("--file", required=True)
    autonomy.add_argument("--dry-run", action="store_true")
    autonomy.add_argument("--report", help="Optional path for a JSON autonomy report.")
    autonomy.add_argument("--max-repair-attempts", type=int, default=1)
    autonomy.set_defaults(func=run_autonomy)

    autonomy_validate = sub.add_parser("autonomy-validate", help="Validate a Phase 6 autonomy goal JSON file.")
    autonomy_validate.add_argument("--file", required=True)
    autonomy_validate.set_defaults(func=run_autonomy_validate)

    simple_game = sub.add_parser("make-simple-game", help="Use Phase 6 autonomy to create a small collect game scene.")
    simple_game.add_argument("--name", default="AI_MiniCollect")
    simple_game.add_argument("--coins", type=int, default=5)
    simple_game.add_argument("--hazards", type=int, default=3)
    simple_game.add_argument("--dry-run", action="store_true")
    simple_game.set_defaults(func=run_make_simple_game)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.func(args))
    except EngineCommandError as exc:
        print_json(exc.response)
        return 2
    except WorkflowValidationError as exc:
        print(f"Workflow validation failed: {exc}", file=sys.stderr)
        return 4
    except VerificationValidationError as exc:
        print(f"Verification validation failed: {exc}", file=sys.stderr)
        return 4
    except AutonomyValidationError as exc:
        print(f"Autonomy validation failed: {exc}", file=sys.stderr)
        return 4
    except (EngineConnectionError, OSError) as exc:
        print(f"Connection failed: {exc}", file=sys.stderr)
        return 3
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
