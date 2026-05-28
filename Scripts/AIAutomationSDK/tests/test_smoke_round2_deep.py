"""Deep assertions for Round 2 fixes - find insufficiencies via real behavior checks."""
from __future__ import annotations
import sys
import time
from engine_client import EngineClient, EngineCommandError

URL = "ws://127.0.0.1:9876"
ISSUES: list[tuple[str, str]] = []


def issue(severity: str, msg: str) -> None:
    ISSUES.append((severity, msg))
    print(f"[{severity}] {msg}")


def main() -> int:
    with EngineClient(URL, timeout=20.0) as c:
        # Reset to 2D scene
        c.command("scene.new", {"mode": "2D", "cleanSlate": True})
        time.sleep(0.3)
        c.command("session.clear_cursors", {})

        # === Deep#1: ecs.query paging correctness ===
        # Make 12 entities, then page through with limit=5
        created = []
        for i in range(12):
            r = c.command("create_empty", {"name": f"PagingTest_{i}"})
            if isinstance(r, dict) and r.get("entity"):
                created.append(r["entity"])
        print(f"\n--- Deep#1 ecs.query paging ({len(created)} entities created) ---")

        page1 = c.command("ecs.query", {"nameContains": "PagingTest_", "limit": 5, "offset": 0})
        print(f"  page1: count={page1.get('count')}, returned={page1.get('returnedCount')}, "
              f"nextOffset={page1.get('nextOffset')}, hasMore={page1.get('hasMore')}")
        if page1.get("count") != 12:
            issue("WARN", f"ecs.query: expected count=12, got {page1.get('count')}")
        if page1.get("returnedCount") != 5:
            issue("WARN", f"ecs.query: expected returnedCount=5, got {page1.get('returnedCount')}")
        if not page1.get("hasMore"):
            issue("WARN", "ecs.query: expected hasMore=true on page 1")

        page2 = c.command("ecs.query", {"nameContains": "PagingTest_", "limit": 5, "offset": 5})
        print(f"  page2: count={page2.get('count')}, returned={page2.get('returnedCount')}, "
              f"nextOffset={page2.get('nextOffset')}, hasMore={page2.get('hasMore')}")

        page3 = c.command("ecs.query", {"nameContains": "PagingTest_", "limit": 5, "offset": 10})
        print(f"  page3: count={page3.get('count')}, returned={page3.get('returnedCount')}, "
              f"nextOffset={page3.get('nextOffset')}, hasMore={page3.get('hasMore')}")
        if page3.get("hasMore"):
            issue("WARN", "ecs.query: expected hasMore=false on last page")
        if page3.get("returnedCount") != 2:
            issue("WARN", f"ecs.query: expected returnedCount=2 on last page, got {page3.get('returnedCount')}")

        # Verify page entities don't overlap
        ent1 = {e["entity"] for e in page1.get("entities", [])}
        ent2 = {e["entity"] for e in page2.get("entities", [])}
        if ent1 & ent2:
            issue("FAIL", f"ecs.query: page1/page2 overlap: {ent1 & ent2}")

        # === Deep#2: rollback actually reverts the state ===
        print(f"\n--- Deep#2 mutate_and_assert rollback semantics ---")
        rb_entity = c.command("create_empty", {"name": "RollbackProbe"}).get("entity")
        c.command("set_component_fields", {
            "entity": rb_entity, "component": "TransformComponent",
            "fields": {"localPosition": [1, 2, 3]}
        })
        before = c.command("ecs.field.get", {
            "entity": rb_entity, "component": "TransformComponent", "field": "localPosition"
        })
        print(f"  before: {before.get('value')}")

        # Mutation that succeeds + assertion that fails
        r = c.command("editor.mutate_and_assert", {
            "command": "set_transform",
            "params": {"entity": rb_entity, "localPosition": [99, 99, 99]},
            "assertions": [
                {"op": "eq",
                 "lhs": {"ecs": {"entity": rb_entity, "component": "TransformComponent",
                                 "field": "localPosition"}},
                 "rhs": [5, 5, 5]}
            ],
            "rollbackOnFail": True,
            "frameSync": 1,
        })
        rb_info = r.get("rollback", {})
        print(f"  rollback executed={rb_info.get('executed')} actions={rb_info.get('info', {}).get('actionsReverted')}")

        after = c.command("ecs.field.get", {
            "entity": rb_entity, "component": "TransformComponent", "field": "localPosition"
        })
        print(f"  after rollback: {after.get('value')}")
        if after.get("value") != before.get("value"):
            issue("FAIL", f"rollback did NOT restore state: before={before.get('value')}, "
                          f"after={after.get('value')}")

        # === Deep#3: frameSync actually updates world matrices ===
        print(f"\n--- Deep#3 frameSync world matrix sync ---")
        # Create entity, set local position via mutate_and_assert with assertion that reads worldMatrix
        sync_entity = c.command("create_empty", {"name": "SyncProbe"}).get("entity")
        # mutate to (10,20,30) and assert worldPosition equals (10,20,30) after frameSync
        r = c.command("editor.mutate_and_assert", {
            "command": "set_transform",
            "params": {"entity": sync_entity, "localPosition": [10, 20, 30]},
            "assertions": [
                {"op": "approx",
                 "lhs": {"ecs": {"entity": sync_entity, "component": "TransformComponent",
                                 "field": "worldPosition"}},
                 "rhs": [10, 20, 30], "tolerance": 0.01}
            ],
            "frameSync": 1,
        })
        a = r.get("assertions", {})
        ap = a.get("allPassed")
        summary = a.get("summary", {})
        print(f"  worldPosition assertion allPassed={ap}")
        if not ap:
            details = summary.get("checks", []) if isinstance(summary, dict) else []
            issue("WARN", f"frameSync: worldPosition not updated after 1 frame. "
                          f"details={details}")

        # === Deep#4: collision.is_hitting with bone-attached collider ===
        # Skip if no skinned mesh — just check that the API returns boneAttached flag
        print(f"\n--- Deep#4 collision.is_hitting bone metadata ---")
        a_ent = c.command("create_empty", {"name": "BoneTestA"}).get("entity")
        b_ent = c.command("create_empty", {"name": "BoneTestB"}).get("entity")
        c.command("set_component_fields", {
            "entity": a_ent, "component": "TransformComponent",
            "fields": {"localPosition": [0, 0, 0]}})
        c.command("set_component_fields", {
            "entity": b_ent, "component": "TransformComponent",
            "fields": {"localPosition": [0.3, 0, 0]}})
        c.command("add_component", {
            "entity": a_ent, "component": "ColliderComponent",
            "fields": {"enabled": True, "elements": [
                {"type": "Sphere", "enabled": True, "radius": 0.5,
                 "offsetLocal": [0, 0, 0], "attribute": "Attack", "nodeIndex": -1}]}})
        c.command("add_component", {
            "entity": b_ent, "component": "ColliderComponent",
            "fields": {"enabled": True, "elements": [
                {"type": "Sphere", "enabled": True, "radius": 0.5,
                 "offsetLocal": [0, 0, 0], "attribute": "Body", "nodeIndex": -1}]}})
        hit = c.command("collision.is_hitting", {
            "sourceEntity": a_ent, "targetEntity": b_ent})
        print(f"  hit.hitting={hit.get('hitting')} pairs={len(hit.get('pairs', []))}")
        if hit.get("pairs"):
            p0 = hit["pairs"][0]
            print(f"    pair0: boneAttached(src={p0.get('sourceBoneAttached')}, "
                  f"tgt={p0.get('targetBoneAttached')}), distance={p0.get('distance'):.3f}")
            if "sourceBoneAttached" not in p0:
                issue("WARN", "collision.is_hitting: missing boneAttached metadata in pair")
        if not hit.get("hitting"):
            issue("WARN", f"collision.is_hitting: expected hit at distance 0.3 with radius 0.5+0.5=1.0")

        # === Deep#5: visual.baseline.save + compare round-trip ===
        print(f"\n--- Deep#5 visual.baseline save/compare round-trip ---")
        saved = c.command("visual.baseline.save", {"name": "smoke_baseline", "target": "window"})
        print(f"  saved: {saved.get('path')} size={saved.get('size')}")
        listed = c.command("visual.baseline.list")
        names = [b.get("name") for b in listed.get("baselines", [])]
        if "smoke_baseline" not in names:
            issue("WARN", f"baseline.list missing 'smoke_baseline': {names}")

        cmp = c.command("visual.baseline.compare", {"name": "smoke_baseline", "target": "window"})
        print(f"  compare: match={cmp.get('match')} percentDiff={cmp.get('percentDiff')}")
        # Same frame so should match (or very close)
        if cmp.get("percentDiff", 100) > 5.0:
            issue("WARN", f"baseline compare immediately after save shows large diff: {cmp.get('percentDiff')}%")

        # cleanup
        c.command("visual.baseline.delete", {"name": "smoke_baseline"})

        # === Deep#6: render.queue.snapshot summary modelPath key ===
        print(f"\n--- Deep#6 render.queue.snapshot stable key ---")
        rs = c.command("render.queue.snapshot", {"includeSummary": True, "includeMeshSources": True})
        summary = rs.get("summary", {})
        by_model = summary.get("byModelPath", [])
        print(f"  byModelPath entries: {len(by_model)}")
        for entry in by_model[:3]:
            key = entry.get("modelKey", "?")
            if key.startswith("<unmapped:"):
                # OK in 2D scene with no mesh entities
                pass
            elif key == "<null>":
                pass
            else:
                print(f"    {key} x{entry.get('drawCount')}")

        # === Deep#7: collision.events.pull cursor advance + overflow detection ===
        print(f"\n--- Deep#7 collision.events.pull cursor semantics ---")
        ev1 = c.command("collision.events.pull", {"cursorName": "smoke_deep", "peek": False})
        print(f"  first pull: prev={ev1.get('previousCursor')}, "
              f"current={ev1.get('currentCursor')}, returned={ev1.get('returnedCount')}, "
              f"overflowed={ev1.get('overflowed')}")
        ev2 = c.command("collision.events.pull", {"cursorName": "smoke_deep", "peek": False})
        print(f"  second pull: returned={ev2.get('returnedCount')}")
        if ev2.get("returnedCount", 0) > 0 and ev2.get("currentCursor") == ev1.get("currentCursor"):
            issue("FAIL", f"collision.events.pull: cursor did not advance but returned new events")

    print(f"\n=== Issues Found: {len(ISSUES)} ===")
    for severity, msg in ISSUES:
        print(f"  [{severity}] {msg}")
    print()
    return 0 if not any(s == "FAIL" for s, _ in ISSUES) else 1


if __name__ == "__main__":
    sys.exit(main())
