# AI Automation SDK

Phase 2 client SDK for the MyEngine automation WebSocket server.

The engine exposes automation commands on:

```text
ws://127.0.0.1:9876
```

This SDK hides WebSocket and JSON envelopes behind Python functions. It uses only the Python standard library.

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
effects, player, ui, sequencer, game_loop, serializer, workflow
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
