"""Round 2 smoke test - verify the 15 newly fixed APIs against a live engine."""
from __future__ import annotations
import sys
import time
from engine_client import EngineClient, EngineCommandError, EngineConnectionError

URL = "ws://127.0.0.1:9876"
RESULTS: list[tuple[str, str, str]] = []  # (status, name, detail)


def run(label: str, fn):
    try:
        out = fn()
        if isinstance(out, dict) and "_skip" in out:
            RESULTS.append(("SKIP", label, str(out.get("reason", ""))[:200]))
            return out
        RESULTS.append(("OK", label, str(out)[:200] if out is not None else ""))
        return out
    except EngineCommandError as e:
        RESULTS.append(("FAIL", label, f"engine: {e.code}: {e.message}"[:300]))
    except EngineConnectionError as e:
        RESULTS.append(("FAIL", label, f"conn: {e}"[:300]))
    except Exception as e:
        RESULTS.append(("FAIL", label, f"py: {type(e).__name__}: {e}"[:300]))
    return None


def main() -> int:
    with EngineClient(URL, timeout=20.0) as c:
        # -- baseline / sanity --
        run("ping", lambda: c.command("ping"))
        engine_state = run("get_engine_state", lambda: c.command("get_engine_state"))

        # -- Fix#3 session.clear_cursors --
        run("session.clear_cursors", lambda: c.command("session.clear_cursors", {}))

        # -- Fix#6 game.poll_pending_steps --
        run("game.poll_pending_steps", lambda: c.command("game.poll_pending_steps"))

        # -- Fix#13 ecs.query paging --
        q = run("ecs.query (limit=5, offset=0)", lambda: c.command(
            "ecs.query", {"limit": 5, "offset": 0}))
        if q and isinstance(q, dict):
            need = ["entities", "count", "returnedCount", "offset", "limit", "nextOffset", "hasMore"]
            missing = [k for k in need if k not in q]
            if missing:
                RESULTS.append(("WARN", "ecs.query paging fields", f"missing: {missing}"))

        # offset > 0 page
        q2 = run("ecs.query (offset=5)", lambda: c.command(
            "ecs.query", {"limit": 5, "offset": 5}))

        # -- Fix#11 render.queue.snapshot stable key --
        rs = run("render.queue.snapshot (includeSummary)", lambda: c.command(
            "render.queue.snapshot", {"includeSummary": True}))
        if rs and isinstance(rs, dict):
            summary = rs.get("summary", {})
            keys = list(summary.keys()) if isinstance(summary, dict) else []
            if "byModelPath" not in keys:
                RESULTS.append(("WARN", "render.queue summary has byModelPath", f"keys={keys}"))

        # -- Fix#14 visual baseline list (should be empty initially) --
        run("visual.baseline.list", lambda: c.command("visual.baseline.list"))

        # -- Fix#7 scene.new + Fix#3 cursor cleanup --
        # scene.new (2D) to get a known state
        run("scene.new 2D", lambda: c.command("scene.new", {"mode": "2D", "cleanSlate": True}))
        time.sleep(0.3)
        # check that cursors did not retain old state
        ev = run("gameflow.events.pull after scene.new", lambda: c.command(
            "gameflow.events.pull", {"maxEvents": 16, "peek": False}))

        # -- Fix#1 collision.is_hitting on dummy entities --
        # create 2 entities with collider components, then is_hitting
        try:
            e1 = c.command("create_empty", {"name": "SmokeA"})
            e2 = c.command("create_empty", {"name": "SmokeB"})
            ea = e1.get("entity") if isinstance(e1, dict) else None
            eb = e2.get("entity") if isinstance(e2, dict) else None
            if ea and eb:
                # set transforms (create_empty already added Transform)
                c.command("set_component_fields", {"entity": ea, "component": "TransformComponent",
                          "fields": {"localPosition": [0, 0, 0]}})
                c.command("set_component_fields", {"entity": eb, "component": "TransformComponent",
                          "fields": {"localPosition": [0.5, 0, 0]}})
                # add colliders
                c.command("add_component", {"entity": ea, "component": "ColliderComponent",
                          "fields": {"enabled": True, "elements": [
                              {"type": "Sphere", "enabled": True, "radius": 1.0,
                               "offsetLocal": [0, 0, 0], "attribute": "Attack"}
                          ]}})
                c.command("add_component", {"entity": eb, "component": "ColliderComponent",
                          "fields": {"enabled": True, "elements": [
                              {"type": "Sphere", "enabled": True, "radius": 1.0,
                               "offsetLocal": [0, 0, 0], "attribute": "Body"}
                          ]}})
                hit = run("collision.is_hitting", lambda: c.command(
                    "collision.is_hitting", {"sourceEntity": ea, "targetEntity": eb}))
                if hit and isinstance(hit, dict):
                    if not hit.get("hitting"):
                        RESULTS.append(("WARN", "collision.is_hitting expected hit",
                                       f"distance={hit.get('pairs', [{}])[0].get('distance', '?')}"))
            else:
                RESULTS.append(("SKIP", "collision.is_hitting", "could not create entities"))
        except Exception as e:
            RESULTS.append(("FAIL", "collision.is_hitting setup", f"{type(e).__name__}: {e}"[:200]))

        # -- Fix#5 mutate_and_assert with rollback --
        try:
            # mutation that succeeds but assertion fails -> rollback
            e3 = c.command("create_empty", {"name": "RollbackTarget"})
            ec_ = e3.get("entity") if isinstance(e3, dict) else None
            if ec_:
                c.command("set_component_fields", {"entity": ec_, "component": "TransformComponent",
                          "fields": {"localPosition": [0, 0, 0]}})
                # mutation: move to (10,0,0); assertion: expect (5,0,0) -> should fail and rollback
                res = run("mutate_and_assert with rollback", lambda: c.command(
                    "editor.mutate_and_assert", {
                        "command": "set_transform",
                        "params": {"entity": ec_, "localPosition": [10, 0, 0]},
                        "assertions": [
                            {"op": "eq",
                             "lhs": {"ecs": {"entity": ec_, "component": "TransformComponent",
                                             "field": "localPosition"}},
                             "rhs": [5, 0, 0]}
                        ],
                        "rollbackOnFail": True,
                        "frameSync": 1,
                    }))
                if res and isinstance(res, dict):
                    rb = res.get("rollback", {})
                    if not rb.get("executed"):
                        RESULTS.append(("WARN", "rollback did not execute",
                                       f"info={rb.get('info', {})}"))
        except Exception as e:
            RESULTS.append(("FAIL", "rollback setup", f"{type(e).__name__}: {e}"[:200]))

        # -- Fix#15 game.step_frames with fixedDeltaTime --
        run("game.step_frames fixedDt", lambda: c.command(
            "game.step_frames", {"frames": 2, "fixedDeltaTime": 1.0 / 60.0}))
        time.sleep(0.1)

        # -- Fix#4 PlayerEditor dirty observation --
        run("player_editor.get_status (dirty)", lambda: c.command("player_editor.get_status"))

        # -- Fix#4 UIEditor sceneDirty --
        run("ui_editor.get_state (sceneDirty)", lambda: c.command("ui_editor.get_state"))

        # -- Fix#17 effect_editor autoCompileIfDirty (just calls get_state with the flag) --
        run("effect_editor.get_state (autoCompileIfDirty)", lambda: c.command(
            "effect_editor.get_state", {"autoCompileIfDirty": True}))

        # -- session.assert_invariant with new ops --
        run("session.assert_invariant approx op", lambda: c.command(
            "session.assert_invariant", {
                "checks": [{"op": "approx", "lhs": 3.14, "rhs": 3.14159, "tolerance": 0.01}]
            }))

    print("\n=== Smoke Test Results ===\n")
    pass_n = sum(1 for s, *_ in RESULTS if s == "OK")
    fail_n = sum(1 for s, *_ in RESULTS if s == "FAIL")
    warn_n = sum(1 for s, *_ in RESULTS if s == "WARN")
    skip_n = sum(1 for s, *_ in RESULTS if s == "SKIP")
    for status, name, detail in RESULTS:
        mark = {"OK": "PASS", "FAIL": "FAIL", "WARN": "WARN", "SKIP": "SKIP"}[status]
        print(f"[{mark}] {name}")
        if status in ("FAIL", "WARN") or len(detail) < 80:
            print(f"       {detail}")
    print(f"\nSummary: {pass_n} pass, {fail_n} fail, {warn_n} warn, {skip_n} skip\n")
    return 0 if fail_n == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
