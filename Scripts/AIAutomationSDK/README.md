# AI Automation SDK

Phase 2 client SDK for the MyEngine automation WebSocket server.

The engine exposes automation commands on:

```text
ws://127.0.0.1:9876
```

This SDK hides WebSocket and JSON envelopes behind Python functions. It uses only the Python standard library.

---

## ⚠️ MANDATE FOR AI AGENTS: USE THE FULL OBSERVATION LAYER

**Do NOT default to `list_entities` + `capture_screenshot` + manual PNG inspection.**
That loop is slow, noisy, and you WILL miss state changes between calls.

Before you touch anything, plan which observation window fits the current question.
Use them aggressively, in combination, and re-evaluate every time the question changes.

### ECS observation
- `ecs.query` — filter by component signature (e.g. all entities with `PlayerTagComponent + HealthComponent`).
  Use this instead of `list_entities` whenever you care about a specific role/system.
- `ecs.hierarchy` — full scene tree in one call. Use before/after `load_scene`, `instantiate_prefab`, `delete_entity`.
- `ecs.diff` — snapshot/diff. Take a snapshot before any mutation (`set_transform`, `set_component_fields`,
  `add_component`, `game.play`, gameflow transition), mutate, then diff. This is how you catch
  "something else also changed" (physics drift, component dropouts, prefab side effects, etc.).
- `ecs.watch` — enable once at session start. Keeps a running change log so you can correlate symptoms
  with the actual mutation that caused them.

### Visual observation (use BEFORE manual screenshots)
- `visual.verify_entity` / `visual.verify_entity_game_view` — boolean "is this entity actually rendered
  on screen right now". This replaces the "screenshot → eyeball → guess" loop for visibility checks.
- `visual.assert_entities_visible` — batch check multiple entities at once.
- `visual.evaluate_capture` — programmatic image analysis (brightness, regions, etc). Use this
  instead of converting BMP→PNG and reading the pixels yourself.
- `visual.capture_review_set` — captures + auto-evaluation packaged together.
- `effect_editor.assert_preview_visible` — equivalent for the effect editor preview.

### Session-level observation
- `ai_session.status` — file backup diff, ECS revision count, command count since session start.
  Run this whenever you're confused about "what did I actually change".
- `ai_session.rollback` — safe rollback when a mutation cascade broke things. Cheaper than
  reconstructing state by hand.
- `editor.recovery.get_state` — engine-side recovery info if the editor entered a degraded state.

### Engine state
- `get_engine_state` — current scene path, play/edit mode, frame count, view rects. Cheap; call it
  often when you suspect mode/path drift.
- `get_visual_state` — camera + view geometry without touching the back buffer.

### Decision rule
- "Is this entity on screen?" → `visual.verify_entity_game_view`, NOT a screenshot.
- "Did X change after Y?" → `ecs.diff`, NOT two `get_component` calls compared by eye.
- "Where did all the Players go?" → `ecs.query` by `PlayerTagComponent`, NOT scrolling `list_entities`.
- "Did I really save the scene?" → `ai_session.status` file backup list, NOT re-reading the file.
- "Is the engine in Play mode and on the right scene?" → `get_engine_state`, NOT inferring from
  the last command's response.

If you find yourself reaching for `capture_screenshot` + PowerShell BMP conversion + `Read` on the PNG,
**stop**. There is almost always a `visual.*` or `ecs.*` call that answers the same question in one step.

---

## Quick Start

Start the engine, then run:

```powershell
python Scripts\AIAutomationSDK\cli.py command ping
python Scripts\AIAutomationSDK\cli.py smoke
```

Call a command with params:

```powershell
python Scripts\AIAutomationSDK\cli.py command create_empty --params "{""name"":""SDK_Test"",""position"":[0,0,0]}"
```

Run an existing sample command:

```powershell
python Scripts\AIAutomationSDK\cli.py command --file Docs\AI_COMMAND_SAMPLES\001_ping.json
```

## Python Usage

```python
from Scripts.AIAutomationSDK import EngineClient

with EngineClient() as engine:
    print(engine.ping())
    print(engine.get_engine_state())
    entity = engine.create_empty(name="SDK_Test", position=[0, 0, 0])
    print(entity)
```

Every public command in `AIAutomationService.cpp` is available as a thin wrapper. Dots in command names become underscores:

```python
engine.scene_view_get_state()
engine.asset_browser_search(query="player")
engine.effect_editor_create_asset(path="Data/Effects/Test.effectgraph")
```

For ambiguous parameters, pass a dict:

```python
engine.command("sequencer.set_params", {"fps": 60})
```

## Phase 3 Tools

`EngineTools` groups low-level SDK calls into authoring tasks:

```python
from Scripts.AIAutomationSDK import EngineClient, EngineTools

with EngineClient() as engine:
    tools = EngineTools(engine)
    tools.inspect_scene()
    tools.assets.search("player", type_filter="Model")
    tools.entities.find_by_name("Player")
    tools.materials.create_toon("Data/Materials/AI_Player.material")
    tools.terrain.create(name="AI_Terrain")
    tools.ui.create_template("player_hp")
    tools.game_loop.validate()
    tools.place_entity(name="PlayerStart", position=[0, 0, -4])
    tools.setup_scene_view(position=[0, 5, -10], target=[0, 0, 0])
    tools.create_battle_layout(name="AI_BattleLayout", enemy_count=3, capture=True)
```

Phase 3 command-family coverage can be checked without starting the engine:

```powershell
python -c "from Scripts.AIAutomationSDK.engine_client import EngineClient; from Scripts.AIAutomationSDK.tools import EngineTools; print(EngineTools(EngineClient()).coverage_report())"
```

The high-level tool families cover every public automation command:

```text
runtime, assets, entities, materials, terrain, editor, lighting,
effects, player, ui, sequencer, game_loop, serializer, workflow,
verify, autonomy, world, link
```

Tool CLI examples:

```powershell
python Scripts\AIAutomationSDK\tools_cli.py inspect
python Scripts\AIAutomationSDK\tools_cli.py coverage
python Scripts\AIAutomationSDK\tools_cli.py preflight
python Scripts\AIAutomationSDK\tools_cli.py asset-search --query player
python Scripts\AIAutomationSDK\tools_cli.py entity-find --name Player
python Scripts\AIAutomationSDK\tools_cli.py material-create --path Data\Materials\AI_Test.material --fields "{""baseColor"":[1,0.8,0.4,1]}"
python Scripts\AIAutomationSDK\tools_cli.py terrain-create --name AI_Terrain --resolution 256
python Scripts\AIAutomationSDK\tools_cli.py ui-template --kind player_hp --canvas
python Scripts\AIAutomationSDK\tools_cli.py game-loop-validate
python Scripts\AIAutomationSDK\tools_cli.py place-empty --name PlayerStart --position 0 0 -4
python Scripts\AIAutomationSDK\tools_cli.py battle-layout --name AI_TestLayout --enemy-count 3 --capture
```

## Phase 4-6 Flow

Phase 4 is workflow execution. Put intent-level steps in a JSON file and let the tool runner execute them through the high-level API. A workflow can use variables, `${...}` references, conditions, expectations, retries, verification steps, checkpoints, and JSON execution reports:

```json
{
  "version": 1,
  "name": "phase4_sample_authoring_workflow",
  "variables": {
    "markerName": "AI_PlayerStart"
  },
  "defaults": {
    "retries": 1,
    "retryDelay": 0.1
  },
  "steps": [
    {
      "id": "preflight",
      "tool": "workflow.preflight",
      "expect": { "equals": ["${steps.preflight.result.coverage.phase3Complete}", true] }
    },
    {
      "id": "marker",
      "tool": "entity.create_empty",
      "args": { "name": "${vars.markerName}", "position": [0, 0, -4] },
      "expect": { "exists": "${steps.marker.result.entity}" }
    },
    { "id": "lights", "tool": "lighting.rig", "args": { "name": "AI_KeyFill" } }
  ],
  "verify": [
    { "id": "verify_entities", "tool": "workflow.verify", "args": { "min_entities": 1 } }
  ],
  "checkpoint": {
    "screenshot_path": "Saved/AI/screenshots/phase4_checkpoint.bmp"
  },
  "reportPath": "Saved/AI/workflows/phase4_latest.json"
}
```

Run it:

```powershell
python Scripts\AIAutomationSDK\tools_cli.py workflow-validate --file Scripts\AIAutomationSDK\examples\workflow_sample.json
python Scripts\AIAutomationSDK\tools_cli.py workflow --file Scripts\AIAutomationSDK\examples\workflow_sample.json --dry-run
python Scripts\AIAutomationSDK\tools_cli.py workflow --file Scripts\AIAutomationSDK\examples\workflow_sample.json
python Scripts\AIAutomationSDK\tools_cli.py workflow --file Scripts\AIAutomationSDK\examples\workflow_sample.json --var markerName="""AI_AltStart"""
```

Phase 5 is verification quality. The verifier can check engine reachability, command coverage, entity existence, component presence, asset/file existence, BMP screenshot validity, selected-object visibility, Scene View/Game View state, and workflow reports:

```python
with EngineClient() as engine:
    tools = EngineTools(engine)
    print(tools.verify.run({
        "name": "phase5_verify",
        "checks": [
            { "type": "engine.ping" },
            { "type": "coverage", "phase3Complete": True },
            { "type": "entity.exists", "name": "AI_PlayerStart", "exact": True },
            { "type": "image.bmp", "path": "Saved/AI/screenshots/phase4_checkpoint.bmp",
              "minWidth": 64, "minHeight": 64, "minUniqueColors": 2 }
        ]
    }))
```

CLI:

```powershell
python Scripts\AIAutomationSDK\tools_cli.py verify-validate --file Scripts\AIAutomationSDK\examples\verification_sample.json
python Scripts\AIAutomationSDK\tools_cli.py verify --file Scripts\AIAutomationSDK\examples\verification_sample.json
python Scripts\AIAutomationSDK\tools_cli.py verify-image --path Saved\AI\screenshots\phase4_checkpoint.bmp --min-unique-colors 2
```

Phase 6 is closed-loop autonomy: inspect, plan, execute, verify, repair, then checkpoint. The SDK includes an autonomy runner for template-based goals. The first built-in template creates a small collect-the-coins scene with a root, terrain, player start, goal, coins, hazards, camera, light rig, UI, saved scene, screenshots, verification report, and autonomy report.

```powershell
python Scripts\AIAutomationSDK\tools_cli.py autonomy-validate --file Scripts\AIAutomationSDK\examples\autonomy_simple_game.json
python Scripts\AIAutomationSDK\tools_cli.py autonomy --file Scripts\AIAutomationSDK\examples\autonomy_simple_game.json --dry-run
python Scripts\AIAutomationSDK\tools_cli.py autonomy --file Scripts\AIAutomationSDK\examples\autonomy_simple_game.json
python Scripts\AIAutomationSDK\tools_cli.py make-simple-game --name AI_MiniCollect --coins 5 --hazards 3
```

Python:

```python
with EngineClient() as engine:
    tools = EngineTools(engine)
    result = tools.autonomy.make_simple_game(
        game_name="AI_MiniCollect",
        coin_count=5,
        hazard_count=3,
    )
    print(result["summary"])
```

## Phase 7-9 World Model

Phase 7 adds an ECS observation layer. It reads entity summaries, per-entity component data, Scene View state, and visual state, then normalizes them into an AI-facing world snapshot.

Phase 8 adds semantic classification and diffing. Entities are tagged as roles such as `player`, `goal`, `collectible`, `hazard`, `terrain`, `camera`, `light`, `ui`, `root`, or `marker`. The world model can validate a simple gameplay loop without the AI manually inspecting raw command JSON.

Phase 9 wraps the Phase 6 autonomy runner with ECS snapshots before and after authoring, semantic gameplay validation, world diffs, and repair plans.

```powershell
python Scripts\AIAutomationSDK\tools_cli.py world-snapshot --report Saved\AI\world\snapshot_latest.json
python Scripts\AIAutomationSDK\tools_cli.py world-summary
python Scripts\AIAutomationSDK\tools_cli.py world-validate --min-collectibles 5 --min-hazards 3 --require-terrain --require-light
python Scripts\AIAutomationSDK\tools_cli.py world-goal --file Scripts\AIAutomationSDK\examples\world_goal_collect_game.json --dry-run
python Scripts\AIAutomationSDK\tools_cli.py world-goal --file Scripts\AIAutomationSDK\examples\world_goal_collect_game.json
python Scripts\AIAutomationSDK\tools_cli.py make-world-game --name AI_Phase9_WorldCollect --coins 5 --hazards 3
```

Python:

```python
with EngineClient() as engine:
    tools = EngineTools(engine)
    snapshot = tools.world.snapshot()
    print(tools.world.semantic_summary(snapshot))
    print(tools.world.validate_gameplay(
        snapshot,
        min_collectibles=5,
        min_hazards=3,
        require_terrain=True,
        require_light=True,
    )["summary"])
    result = tools.world.make_collect_game(
        game_name="AI_Phase9_WorldCollect",
        coin_count=5,
        hazard_count=3,
    )
    print(result["summary"])
```

## Phase 10-12 AI-Engine Link

Phase 10 adds an affordance and intent layer. The SDK can now describe what the engine can do from the current world state, then compile a goal into an explicit intent plan.

Phase 11 adds a session journal. Every important observation, plan, execution result, diff, validation, and repair is stored as a timeline so later AI passes can reason from history instead of a single command result.

Phase 12 adds the linked orchestrator. It combines affordance mapping, intent planning, world authoring, ECS snapshots, semantic diffing, gameplay validation, repair, and journal/report writing.

```powershell
python Scripts\AIAutomationSDK\tools_cli.py link-affordances
python Scripts\AIAutomationSDK\tools_cli.py link-plan --file Scripts\AIAutomationSDK\examples\link_goal_collect_game.json
python Scripts\AIAutomationSDK\tools_cli.py link-run --file Scripts\AIAutomationSDK\examples\link_goal_collect_game.json --dry-run
python Scripts\AIAutomationSDK\tools_cli.py link-run --file Scripts\AIAutomationSDK\examples\link_goal_collect_game.json
python Scripts\AIAutomationSDK\tools_cli.py make-linked-game --name AI_Phase12_LinkedCollect --coins 5 --hazards 3
```

Python:

```python
with EngineClient() as engine:
    tools = EngineTools(engine)
    print(tools.link.affordances()["affordances"])
    plan = tools.link.plan({
        "intent": "collect_game",
        "variables": {"gameName": "AI_Phase12_LinkedCollect", "coinCount": 5, "hazardCount": 3}
    })
    print(plan["steps"])
    result = tools.link.make_collect_game(
        game_name="AI_Phase12_LinkedCollect",
        coin_count=5,
        hazard_count=3,
    )
    print(result["summary"])
```

## CLI

List all commands and generated method names:

```powershell
python Scripts\AIAutomationSDK\cli.py methods
```

Print the full response envelope instead of only `result`:

```powershell
python Scripts\AIAutomationSDK\cli.py command ping --raw-response
```

## Error Model

If the engine returns `{ "ok": false }`, the SDK raises `EngineCommandError`. The CLI prints the full response and exits with code `2`.

If the engine is not running or the WebSocket server is unreachable, the CLI exits with code `3`.
