"""Round 3 specific regression tests.

Covers:
  R3#1 visual.compare_capture match tolerance
  R3#2 step_frames fixedDeltaTime reset
  R3#5 rollback executed=false when actionsReverted=0
  R3#assert vector approx / ne with element-wise tolerance
  R3#10 entity API naming aliases
  R3#11 collision.is_hitting Box vs Box OBB
  R3#15 entity.batch_create
  R3#20 scene.save guard in mutate_and_assert
"""
from __future__ import annotations
import pytest


def test_baseline_match_tolerance(client):
    """Same capture vs itself should report match=True even with 1-pixel jitter."""
    client.command("visual.baseline.save",
                   {"name": "_round3_match_probe", "target": "window"})
    try:
        r = client.command("visual.baseline.compare",
                           {"name": "_round3_match_probe", "target": "window",
                            "matchTolerancePercent": 0.5})
        assert r["strictMatch"] in (True, False)
        # match should be True even if strictMatch=False, as long as percentDiff < tolerance
        assert r["match"] is True, f"match should be True for same-frame capture, got percentDiff={r['percentDiff']}%"
    finally:
        client.command("visual.baseline.delete", {"name": "_round3_match_probe"})


def test_stepframes_fixed_dt_resets(client):
    """game.step_frames with fixedDeltaTime must reset after the step is consumed."""
    # 1st step with fixed dt
    client.command("game.step_frames", {"frames": 1, "fixedDeltaTime": 1.0 / 60.0})
    waited = client.wait_for_steps_completed(timeout_sec=2.0)
    assert not waited["_timedOut"], "step should consume within 2s"
    # 2nd step without fixedDeltaTime should NOT reuse the 1/60 value
    r = client.command("game.step_frames", {"frames": 1})
    assert r.get("fixedDeltaTime", -1.0) == 0.0, \
        f"fixedDeltaTime did not reset; got {r.get('fixedDeltaTime')}"


def test_rollback_not_executed_for_readonly_mutation(client):
    """rollback.executed should be False when the mutation pushed no undo action."""
    # 'ping' produces no undo action. Force assertion failure.
    r = client.command("editor.mutate_and_assert", {
        "command": "ping",
        "params": {},
        "assertions": [
            {"op": "eq", "lhs": "alpha", "rhs": "beta"}  # will fail
        ],
        "rollbackOnFail": True,
    })
    rb = r.get("rollback", {})
    assert rb["requested"] is True
    assert rb["executed"] is False, "no undo action was pushed; executed must be False"
    assert rb["info"]["actionsReverted"] == 0


def test_assert_vector_approx_ne(client):
    """approx with vector lhs/rhs and ne with near-equal vectors."""
    e = client.command("create_empty", {"name": "_R3VecProbe"})["entity"]
    client.command("set_component_fields", {
        "entity": e, "component": "TransformComponent",
        "fields": {"localPosition": [1.0, 2.0, 3.0]}
    })
    # approx with vector, tight tolerance
    r = client.command("session.assert_invariant", {
        "checks": [
            {"op": "approx",
             "lhs": {"ecs": {"entity": e, "component": "TransformComponent",
                             "field": "localPosition"}},
             "rhs": [1.0, 2.0, 3.0], "tolerance": 0.001},
        ]
    })
    assert r["allPassed"] is True
    # ne with very close vectors (within tolerance) should report ne=false
    r2 = client.command("session.assert_invariant", {
        "checks": [
            {"op": "ne",
             "lhs": {"ecs": {"entity": e, "component": "TransformComponent",
                             "field": "localPosition"}},
             "rhs": [1.0000001, 2.0000001, 3.0000001], "tolerance": 0.001},
        ]
    })
    assert r2["allPassed"] is False, "vector ne should be False when values are within tolerance"


def test_entity_naming_aliases(client):
    """entity.create alias must be accepted in addition to create_empty."""
    r1 = client.command("entity.create", {"name": "_R3Alias1"})
    assert "entity" in r1
    # entity.delete alias
    r2 = client.command("entity.delete", {"entity": r1["entity"]})
    assert r2 is not None


def test_batch_create(client):
    """entity.batch_create returns N entities in 1 call."""
    r = client.command("entity.batch_create", {
        "entities": [
            {"name": f"_R3Batch_{i}", "position": [float(i), 0, 0]}
            for i in range(5)
        ]
    })
    assert r["createdCount"] == 5
    assert r["errorCount"] == 0
    assert len(r["created"]) == 5


def test_collision_is_hitting_box_box(client):
    """Box vs Box uses OBB SAT (not sphere approx)."""
    a = client.command("create_empty", {"name": "_R3BoxA"})["entity"]
    b = client.command("create_empty", {"name": "_R3BoxB"})["entity"]
    client.command("set_component_fields", {
        "entity": a, "component": "TransformComponent",
        "fields": {"localPosition": [0, 0, 0]}})
    client.command("set_component_fields", {
        "entity": b, "component": "TransformComponent",
        "fields": {"localPosition": [0.4, 0, 0]}})
    client.command("add_component", {
        "entity": a, "component": "ColliderComponent",
        "fields": {"enabled": True, "elements": [
            {"type": "Box", "enabled": True, "size": [1.0, 1.0, 1.0],
             "offsetLocal": [0, 0, 0], "attribute": "Body", "nodeIndex": -1}]}})
    client.command("add_component", {
        "entity": b, "component": "ColliderComponent",
        "fields": {"enabled": True, "elements": [
            {"type": "Box", "enabled": True, "size": [1.0, 1.0, 1.0],
             "offsetLocal": [0, 0, 0], "attribute": "Body", "nodeIndex": -1}]}})
    r = client.command("collision.is_hitting", {"sourceEntity": a, "targetEntity": b})
    assert r["pairs"][0]["sourceShape"] == "Box"
    assert r["pairs"][0]["targetShape"] == "Box"
    assert r["hitting"] is True


def test_scene_save_guard(client):
    """mutate_and_assert blocks scene.save without allowFilesystemMutation."""
    from engine_client import EngineCommandError

    def err_code(exc: EngineCommandError) -> str:
        err = (exc.response or {}).get("error") or {}
        return err.get("code", "") if isinstance(err, dict) else ""

    with pytest.raises(EngineCommandError) as exc:
        client.command("editor.mutate_and_assert", {
            "command": "scene.save",
            "params": {},
            "assertions": []
        })
    assert err_code(exc.value) == "filesystem_mutation_blocked", \
        f"expected filesystem_mutation_blocked, got: {exc.value!r}"

    # With allowFilesystemMutation it should not error on the guard
    # (it may still fail for other reasons like "no scene", but not the guard)
    try:
        client.command("editor.mutate_and_assert", {
            "command": "scene.save",
            "params": {},
            "assertions": [],
            "allowFilesystemMutation": True,
        })
    except EngineCommandError as e:
        # If it errors, it should NOT be the filesystem_mutation_blocked code
        assert err_code(e) != "filesystem_mutation_blocked"


def test_session_clear_cursors(client):
    """session.clear_cursors returns ok and reports cleared count."""
    r = client.command("session.clear_cursors", {})
    assert r["ok"] is True
    assert "remaining" in r
    assert "collision" in r["remaining"]


def test_ecs_query_paging_no_overlap(client):
    """Paging across ecs.query must not return overlapping entities."""
    # Create 8 known entities
    batch = client.command("entity.batch_create", {
        "entities": [{"name": f"_R3Page_{i}"} for i in range(8)]
    })
    assert batch["createdCount"] == 8

    page1 = client.command("ecs.query", {"nameContains": "_R3Page_", "limit": 3, "offset": 0})
    page2 = client.command("ecs.query", {"nameContains": "_R3Page_", "limit": 3, "offset": 3})
    page3 = client.command("ecs.query", {"nameContains": "_R3Page_", "limit": 3, "offset": 6})

    ids1 = {e["entity"] for e in page1["entities"]}
    ids2 = {e["entity"] for e in page2["entities"]}
    ids3 = {e["entity"] for e in page3["entities"]}
    assert ids1 & ids2 == set(), f"page1/page2 overlap: {ids1 & ids2}"
    assert ids2 & ids3 == set(), f"page2/page3 overlap: {ids2 & ids3}"
    assert page3["hasMore"] is False, "last page should report hasMore=False"
    assert page3["returnedCount"] == 2, f"last page should return 2 (8 total - 6 offset), got {page3['returnedCount']}"
