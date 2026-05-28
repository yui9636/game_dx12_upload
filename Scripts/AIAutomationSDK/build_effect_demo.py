"""Build a small spark/burst effect end-to-end via observation layer.

Demonstrates the R1+R2+R3 observation APIs working together:
  - effect_editor.apply_preset (skip raw node graph building)
  - effect_editor.get_state with autoCompileIfDirty + hints
  - effect_editor.assert_preview_visible (programmatic visibility check, no screenshot)
  - visual.baseline.save (capture for regression)
  - session.assert_invariant (engine_mode + scene path sanity)
"""
from __future__ import annotations
import json
import sys
import time
from engine_client import EngineClient, EngineCommandError

URL = "ws://127.0.0.1:9876"
ASSET_PATH = "Data/Effects/AI_R3_Spark.effectgraph.json"
BASELINE_NAME = "ai_r3_spark_preview"


def banner(t: str) -> None:
    print(f"\n{'='*60}\n{t}\n{'='*60}")


def show(label: str, data) -> None:
    print(f"[{label}] {json.dumps(data, indent=2)[:600]}")


def main() -> int:
    with EngineClient(URL, timeout=60.0) as c:
        banner("0. Sanity: engine state + session")
        state = c.command("get_engine_state")
        show("engine", state)

        # 必ず session を始めて cursor を綺麗にする (R2 fix #3)
        try:
            c.command("ai_session.begin", {
                "name": "EffectDemoR3",
                "goal": "Build spark effect via observation layer"
            })
        except EngineCommandError as e:
            # 既に session 中なら force で上書き
            if "session_active" in str(e):
                c.command("ai_session.end", {})
                c.command("ai_session.begin", {
                    "name": "EffectDemoR3", "force": True
                })
            else:
                raise

        banner("1. Effect editor: create asset")
        try:
            r = c.command("effect_editor.create_asset", {
                "path": ASSET_PATH, "name": "AI_R3_Spark", "overwrite": True
            })
            show("create_asset", r)
        except EngineCommandError as e:
            print(f"create_asset failed (may already exist): {e}")

        banner("2. Open workspace")
        r = c.command("effect_editor.open_workspace", {"path": ASSET_PATH})
        show("open_workspace", r)

        banner("3. Apply 'spark' preset")
        # preset で 1 call で sphere emitter + lifetime + sprite renderer を生成
        r = c.command("effect_editor.apply_preset", {
            "path": ASSET_PATH,
            "preset": "spark",
            "shape": "sphere",
            "spawnRate": 60.0,
            "particleLifetime": 1.2,
            "speed": 2.5,
            "startSize": 0.15,
            "endSize": 0.02,
        })
        show("apply_preset", r)

        banner("4. get_state with autoCompileIfDirty (R3#18 surfaces compile errors)")
        r = c.command("effect_editor.get_state", {
            "autoCompileIfDirty": True,
            "includeGraph": False,
        })
        hints = r.get("hints", {})
        print(f"compileTriggered = {hints.get('compileTriggered')}")
        print(f"compileOk        = {hints.get('compileOk')}")
        print(f"compileClean     = {hints.get('compileClean')}")
        print(f"errorCount       = {hints.get('compileErrorCount', 0)}")
        if hints.get("compileErrors"):
            print(f"!! ERRORS: {hints['compileErrors']}")
            return 1

        state = r.get("state", {})
        print(f"compileDirty after  = {state.get('compileDirty')}")
        print(f"documentPath        = {state.get('documentPath')}")
        print(f"effectEditorActive  = {state.get('effectEditorActive')}")

        banner("5. Play timeline")
        r = c.command("effect_editor.timeline_play", {})
        show("timeline_play", r)
        time.sleep(0.5)  # let particles spawn

        banner("6. assert_preview_visible (no screenshot needed)")
        try:
            r = c.command("effect_editor.assert_preview_visible", {})
            show("preview_visible", r)
        except EngineCommandError as e:
            print(f"preview_visible failed: {e}")

        banner("7. session.assert_invariant (multiple checks in 1 call)")
        r = c.command("session.assert_invariant", {
            "checks": [
                {"kind": "engine_mode", "mode": "Editor"},
                {"op": "ne",
                 "lhs": {"ecs": {"entity": "0", "component": "TransformComponent", "field": "localPosition"}},
                 "rhs": [0, 0, 0]}  # this entity probably doesn't exist; just exercising shorthand
            ]
        })
        print(f"allPassed={r.get('allPassed')}, passed={r.get('passed')}/{r.get('total')}")
        # We expect 1 pass (engine_mode) + 1 fail (bogus entity)
        # So allPassed=False is OK here, we just verify the shape

        banner("8. Save visual baseline (R2 fix #14)")
        try:
            r = c.command("visual.baseline.save", {
                "name": BASELINE_NAME, "target": "window", "overwrite": True
            })
            show("baseline_saved", r)
        except EngineCommandError as e:
            print(f"baseline save failed: {e}")

        banner("9. Compare baseline against itself (sanity)")
        try:
            r = c.command("visual.baseline.compare", {
                "name": BASELINE_NAME, "target": "window",
                "matchTolerancePercent": 0.5,
            })
            print(f"match={r.get('match')} strict={r.get('strictMatch')} pctDiff={r.get('percentDiff'):.4f}%")
        except EngineCommandError as e:
            print(f"baseline compare failed: {e}")

        banner("10. effect_editor state final")
        r = c.command("effect_editor.get_state", {"includeGraph": True})
        state = r.get("state", {})
        compiled = state.get("compiled", {})
        if compiled:
            print(f"compiled.valid      = {compiled.get('valid')}")
            print(f"compiled.duration   = {compiled.get('duration')}")
            print(f"renderer            = mesh:{compiled.get('meshRenderer', {}).get('enabled')}, "
                  f"particle:{compiled.get('particleRenderer', {}).get('enabled')}")
            print(f"executionPlan nodes = spawn:{len(compiled.get('executionPlan',{}).get('spawnNodeIds',[]))}, "
                  f"update:{len(compiled.get('executionPlan',{}).get('updateNodeIds',[]))}, "
                  f"render:{len(compiled.get('executionPlan',{}).get('renderNodeIds',[]))}")

        # asset summary (R3 includeGraph=True)
        asset = state.get("asset")
        if asset:
            print(f"asset nodes         = {len(asset.get('nodes', []))}")
            print(f"asset links         = {len(asset.get('links', []))}")
            for n in asset.get("nodes", [])[:8]:
                print(f"  - node id={n.get('id')} type={n.get('type')} name={n.get('displayName','?')}")

        banner("Done. Cleanup baseline.")
        try:
            c.command("visual.baseline.delete", {"name": BASELINE_NAME})
        except Exception:
            pass

        c.command("ai_session.end", {})
        return 0


if __name__ == "__main__":
    sys.exit(main())
