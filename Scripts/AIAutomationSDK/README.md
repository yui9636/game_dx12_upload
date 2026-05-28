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
  Use `ecs.diff(action="help")` when you need the baseline rules. Short version:
  unnamed `diff` is rolling, named `snapshot` + named `diff` compares repeatedly against the same baseline.
- `ecs.watch` — enable once at session start. Keeps a running change log so you can correlate symptoms
  with the actual mutation that caused them.
- `component.vector.*` edits vector fields directly. Use it for `ColliderComponent.elements`,
  `InputActionMapComponent.asset.actions`, and other reflected arrays when you only need one element change.
- `automation.get_manifest(writePath="Saved/AI/automation_manifest.json")` generates the current
  "what should I observe with?" table for agents and tools.
- `render.queue.snapshot(includePackets=True, limit=32)` dumps packet fields for render/depth diagnosis,
  not just queue counts.
- `bone.stream.start(...); bone.stream.pull(...)` keeps bone-world probes inside the engine and avoids
  one WebSocket round trip per bone per frame.
- `log.tail(category="Camera2D")` and `log.pull(category="Camera2D")` use structured log category and
  timestamp fields instead of external prefix parsing.

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
- `input.events.pull` is a durable input-event cursor. Use it after `game.input.tap` when you need
  the exact press/release frames and queue sequence.
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

## New observation APIs (Round 1–3 additions)

### Cursor-based event streaming (idempotent with `peek=true`)
- `gameflow.events.pull` — pull GameFlow events (scene loaded, transitions) since last cursor.
- `collision.events.pull` — pull damage / hit events with `sequence` field (ring-buffer overflow safe).
- `log.pull` — pull logger output since last cursor, filtered by `category`/`minSeverity`/`filterRegex`.
- `ecs.field.watch.pull` — watch one component field across frames.

### Live state inspection
- `gameflow.get_runtime_state` — current node, transitions pending, battle phase, scene path, flags.
- `gameflow.eval_conditions` — what would advance the current node? (lookahead diagnostic)
- `animator.get_state` — playing state, blend, animation index.
- `input.get_resolved_state` — what the input system actually resolved this frame.
- `editor.get_focus` / `editor.get_hierarchy_selection` — which panel / entity has focus.
- `asset.status` — load state of named asset path.

### Geometry / collision
- `bone.list` / `bone.get_world` / `bone.get_world_batch` — bone world transforms (1 or N at once).
- `collision.raycast` / `collision.overlap_sphere` — physics queries via Jolt.
- `collision.is_hitting` — per-frame "do these 2 entities overlap right now" with shape-aware overlap
  (Sphere/Box OBB SAT, Capsule sphere-chain approx) plus bone attachment metadata.

### Visual baselines (CI-friendly regression)
- `visual.find_text` — find rendered text by string (no OCR, reads `TextComponent`).
- `visual.get_pixel_at_screen` — read RGB at exact pixel.
- `visual.compare_capture` — BMP-vs-BMP diff with `matchTolerancePercent`.
- `visual.baseline.save` / `visual.baseline.list` / `visual.baseline.delete` / `visual.baseline.compare`
  — managed baseline workflow under `Saved/Visual_Baselines/<name>.bmp`.

### Mutation + assertion (atomic transactions)
- `editor.mutate_and_assert` — run mutation + run assertions + auto-attach `_visualState`.
  Optional `rollbackOnFail=true` reverts the mutation via UndoSystem when an assertion fails.
  Optional `frameSync=N` runs Hierarchy → Transform → Animator → Camera systems N times
  so world matrices, bone poses and view matrices are observable in the same call.
  `stepFrames=N` + `assertAfterSteps=true` defers assertions until N game frames elapse
  (call `game.poll_pending_steps` or `client.wait_for_steps_completed()` to await).
- `session.assert_invariant` — batch check.
  - Original form: `[{kind:"component_field", entity, component, field, op, value}]`
  - **Shorthand (R2 unified with mutate_and_assert)**:
    `[{op, lhs:{ecs:{entity,component,field}}|<literal>, rhs:..., tolerance?}]`
  - Ops: `eq` / `ne` / `gt` / `ge` / `lt` / `le` / `approx` (vector-aware) / `contains` / `regex` / `in_range`

### Render queue introspection
- `render.queue.snapshot` — counts + per-packet (mesh, material, world matrix). Optional
  `includeSummary=true` aggregates by `modelFilePath` and `materialPath` (stable keys for cross-session diff).
  `includeMeshSources=true` adds the entity → modelFilePath reverse map.

### Editor dirty / hints
- `effect_editor.get_state` — `autoCompileIfDirty=true` recompiles silently. Top-level `hints.compileErrors`
  surfaces compile failures without digging into `state.compiled.errors`.
- `player_editor.get_status.dirty.*` — per-tab (stateMachine, timeline, socket, collider, modelAnimation).
- `ui_editor.get_state.sceneDirty` — scene-level dirty flag for UI authoring.

### Engine control with determinism
- `game.step_frames` — `fixedDeltaTime=1.0/60.0` for deterministic dt during stepping.
  After the step is consumed, `m_stepFixedDt` resets so subsequent steps use real-time dt unless
  explicitly overridden again.
- `game.poll_pending_steps` — counter of queued steps not yet consumed by the Update tick.
  Python helper: `client.wait_for_steps_completed(timeout_sec, poll_interval_sec)`.

### Session lifecycle
- `ai_session.begin` — automatically clears all pull cursors so the new session sees fresh state.
- `session.clear_cursors` — manual clear (collision / gameflow / logs).
- `ai_session.rollback` — restore from session-start file backups + ECS revision.

### Bulk operations
- `entity.batch_create` — create N entities in one call (avoids N round-trips for test fixture setup).
- `batch` — generic envelope: `{commands: [{command, params}, ...]}` runs sub-commands sequentially.

### Naming aliases (R3 — both forms accepted)
- `create_empty` ≡ `entity.create_empty` ≡ `entity.create`
- `set_transform` ≡ `entity.set_transform`
- `delete_entity` ≡ `entity.delete`
- `duplicate_entity` ≡ `entity.duplicate`
- `reparent_entity` ≡ `entity.reparent`
- `get_component` ≡ `component.get`
- `add_component` ≡ `component.add`
- `remove_component` ≡ `component.remove`
- `set_component_fields` ≡ `component.set_fields`
- `list_entities` ≡ `entity.list`
- `get_entity` ≡ `entity.get`
- `select_entity` ≡ `entity.select`
- `player_editor.set_actor_mode` ≡ `player_editor.set_actor_role`

### Filesystem-mutation guard (R3)
`editor.mutate_and_assert` blocks `scene.save`, `material.create`, `asset_browser.*`, `terrain.regenerate`,
`player_editor.save_prefab`, etc. unless `allowFilesystemMutation=true`. These can't be rolled back.

### Client-side resilience (Python SDK)
- Auto-reconnect on socket loss with exponential backoff (`max_reconnect_attempts=3` default).
- Safe-retry whitelist (`SAFE_RETRY_COMMANDS`) + cursor-advance commands with `peek=true` guard.
- `HEAVY_COMMAND_TIMEOUTS` overrides default 5s timeout for known-slow ops (scene.new, terrain.*, etc.).

### Codegen / discoverability
- `Scripts/AIAutomationSDK/gen_command_list.py` — diff C++ `DispatchCommand` vs Python `COMMANDS`
  to detect drift. Run with `--check` to fail CI on missing entries.
- `Docs/API_REGISTRATION_CHECKLIST.md` — what to update when adding a new command.
- `Docs/API_TEST_SPEC_TEMPLATE.md` — smoke / invariant / scenario test template.

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
