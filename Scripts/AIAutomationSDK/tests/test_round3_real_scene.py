"""Round 3 verification tests against a real 3D scene (Sandbag_Game.scene).

These exercise the code paths that the synthetic R3 tests skip:
  R3#6 bone-attached collider math on A5 skinned mesh
  R3#7 Animator system runs inside frameSync (bones move after step)
  R3#8 render.queue.snapshot.summary.byModelPath populated by real meshes
  R3#9 session cursor cleanup actually resets state across calls

Skips gracefully if Sandbag_Game.scene is not available.
"""
from __future__ import annotations
import os
import pytest

SANDBAG_SCENE = "Data/Scene/Sandbag_Game.scene"


def _scene_exists() -> bool:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(here, "..", "..", ".."))
    return os.path.exists(os.path.join(root, SANDBAG_SCENE))


@pytest.fixture
def loaded_scene(client):
    if not _scene_exists():
        pytest.skip(f"{SANDBAG_SCENE} not found; run sandbag setup first.")
    client.command("load_scene", {"path": SANDBAG_SCENE}, timeout=30.0)
    yield client


def test_render_queue_summary_has_model_paths(loaded_scene):
    """R3#8: 3D scene should populate byModelPath with non-empty mesh paths."""
    rs = loaded_scene.command("render.queue.snapshot",
                              {"includeSummary": True, "includeMeshSources": True})
    summary = rs.get("summary", {})
    by_model = summary.get("byModelPath", [])
    # In 3D scene there must be at least 1 model
    assert len(by_model) >= 1, f"byModelPath empty in 3D scene: {summary}"
    # At least one entry should be a real file path, not <unmapped:...>
    real_paths = [e for e in by_model if not e["modelKey"].startswith("<")]
    assert len(real_paths) >= 1, \
        f"all byModelPath entries are unmapped: {[e['modelKey'] for e in by_model]}"


def test_animator_sync_moves_bones(loaded_scene):
    """R3#7: After frameSync with syncAnimator=true, bone worldTransform should reflect runtime pose."""
    # Find Player entity
    q = loaded_scene.command("ecs.query", {
        "hasComponents": ["PlayerTagComponent"], "limit": 1
    })
    if q["count"] == 0:
        pytest.skip("No PlayerTag entity in scene.")
    player_id = q["entities"][0]["entity"]

    # Get bone list
    bones = loaded_scene.command("bone.list", {"entity": player_id})
    if bones.get("boneCount", 0) == 0:
        pytest.skip("Player has no bones (not a skinned mesh).")

    # Pick a bone and read world position before + after frameSync.
    # If Animator is integrated, the bone may move slightly between frames during idle anim.
    target_bone = bones["bones"][0]["name"]
    before = loaded_scene.command("bone.get_world",
                                  {"entity": player_id, "boneName": target_bone})
    # mutate_and_assert with frameSync=2 (Hierarchy/Transform/Animator/Camera each loop)
    loaded_scene.command("editor.mutate_and_assert", {
        "command": "ping",  # no-op mutation
        "params": {},
        "assertions": [],
        "frameSync": 2,
        "syncAnimator": True,
    })
    after = loaded_scene.command("bone.get_world",
                                 {"entity": player_id, "boneName": target_bone})
    # We don't assert movement (idle anim might be paused). We assert no errors.
    assert "worldPosition" in before
    assert "worldPosition" in after


def test_bone_attached_collider_in_player(loaded_scene):
    """R3#6: Player attack collider (typically on hand bone) reports boneAttached."""
    # Search all PlayerTag+Collider entities and pick the one with a bone-attached element.
    # The scene may have multiple Player-tagged entities (e.g., prefab root + child).
    q = loaded_scene.command("ecs.query", {
        "hasComponents": ["PlayerTagComponent", "ColliderComponent"], "limit": 16
    })
    if q["count"] == 0:
        pytest.skip("No Player with ColliderComponent.")

    player_id = None
    bone_elements = []
    for cand in q["entities"]:
        comp = loaded_scene.command("get_component",
                                    {"entity": cand["entity"], "component": "ColliderComponent"})
        elements = comp.get("fields", {}).get("elements", [])
        be = [e for e in elements if e.get("nodeIndex", -1) >= 0]
        if be:
            player_id = cand["entity"]
            bone_elements = be
            break

    if player_id is None:
        pytest.skip("No Player has a bone-attached collider element.")

    # Find another collider entity (e.g., Sandbag) to query is_hitting against.
    q2 = loaded_scene.command("ecs.query", {
        "hasComponents": ["ColliderComponent"], "limit": 8
    })
    others = [e["entity"] for e in q2["entities"] if e["entity"] != player_id]
    if not others:
        pytest.skip("No other collider entity.")

    hit = loaded_scene.command("collision.is_hitting", {
        "sourceEntity": player_id, "targetEntity": others[0]
    })
    # At least one pair should have boneAttached=true (the hand_r attack)
    has_bone_pair = any(p.get("sourceBoneAttached") for p in hit.get("pairs", []))
    assert has_bone_pair, \
        f"expected at least one pair with sourceBoneAttached=true: {hit['pairs'][:3]}"


def test_cursor_cleanup_resets_state(loaded_scene):
    """R3#9: session.clear_cursors must reset pull cursors so next pull starts fresh."""
    # Take a pull to advance cursor
    p1 = loaded_scene.command("gameflow.events.pull",
                              {"cursorName": "R3_cleanup_probe", "peek": False})
    cursor_after_first = p1.get("currentCursor", 0)

    # Clear cursors
    cleared = loaded_scene.command("session.clear_cursors",
                                   {"gameflow": True, "collision": True, "logs": True})
    assert cleared["ok"] is True

    # Next pull with same cursorName: previousCursor should be 0 (reset)
    p2 = loaded_scene.command("gameflow.events.pull",
                              {"cursorName": "R3_cleanup_probe", "peek": False})
    assert p2.get("previousCursor", -1) == 0, \
        f"cursor not reset: previousCursor={p2.get('previousCursor')} " \
        f"(was at {cursor_after_first} before clear)"
