"""Create a multi-layer inferno ring effect matching the reference image."""
import sys, time, json, os
sys.path.insert(0, os.path.dirname(__file__))
from engine_client import EngineClient, EngineCommandError

# ── ① メインリング: circle shape + ribbon, 高vortexで弧を描く ──────────────
RING_PATH   = "Data/Effect/AI/InfernoRing.effectgraph.json"
RING_PARAMS = {
    "name":             "Inferno Ring",
    "duration":         3.0,
    "spawnRate":        14000.0,
    "particleLifetime": 1.8,
    "startSize":        0.26,
    "endSize":          0.06,
    "speed":            1.2,
    "shape":            "circle",
    "shapeParams":      [1.0, 0.0, 0.0],
    "spinRate":         4.0,
    "drawMode":         "ribbon",
    "ribbonWidth":      0.55,
    "ribbonStretch":    3.5,
    "curlNoiseStrength": 0.65,
    "curlNoiseScale":   0.09,
    "curlNoiseScroll":  0.32,
    "vortexStrength":   4.5,
    "startColor":       [1.0, 0.88, 0.22, 1.0],
    "endColor":         [1.0, 0.08, 0.0, 0.0],
    "alphaCurveBias":   0.15,
    "sizeCurveBias":    -0.25,
    "texture":          "Data/Effect/particle/flame_03.png",
}

# ── ② スパーク: circle spawn + billboard, 高速飛散 ────────────────────────
SPARK_PATH   = "Data/Effect/AI/InfernoSparks.effectgraph.json"
SPARK_PARAMS = {
    "name":             "Inferno Sparks",
    "duration":         3.0,
    "spawnRate":        40000.0,
    "particleLifetime": 0.55,
    "startSize":        0.06,
    "endSize":          0.01,
    "speed":            5.5,
    "shape":            "circle",
    "shapeParams":      [0.85, 0.0, 0.0],
    "spinRate":         6.0,
    "drawMode":         "billboard",
    "curlNoiseStrength": 0.30,
    "curlNoiseScale":   0.22,
    "curlNoiseScroll":  0.45,
    "vortexStrength":   2.0,
    "startColor":       [1.0, 0.95, 0.65, 1.0],
    "endColor":         [1.0, 0.25, 0.0, 0.0],
    "alphaCurveBias":   0.10,
    "texture":          "Data/Effect/particle/spark_01.png",
}

# ── ③ スモーク: sphere spawn + billboard, じわっと広がる暗煙 ──────────────
SMOKE_PATH   = "Data/Effect/AI/InfernoSmoke.effectgraph.json"
SMOKE_PARAMS = {
    "name":             "Inferno Smoke",
    "duration":         3.0,
    "spawnRate":        900.0,
    "particleLifetime": 2.8,
    "startSize":        0.55,
    "endSize":          2.2,
    "speed":            0.35,
    "shape":            "sphere",
    "shapeParams":      [1.1, 1.1, 0.2],
    "drawMode":         "billboard",
    "drag":             0.12,
    "curlNoiseStrength": 0.42,
    "curlNoiseScale":   0.07,
    "curlNoiseScroll":  0.10,
    "vortexStrength":   0.6,
    "startColor":       [0.22, 0.13, 0.09, 0.40],
    "endColor":         [0.08, 0.06, 0.06, 0.0],
    "alphaCurveBias":   0.35,
    "sizeCurveBias":    0.40,
    "texture":          "Data/Effect/particle/flame_02.png",
}

LAYERS = [
    ("ring",   RING_PATH,   RING_PARAMS,   "spark"),   # base preset
    ("sparks", SPARK_PATH,  SPARK_PARAMS,  "spark"),
    ("smoke",  SMOKE_PATH,  SMOKE_PARAMS,  "smoke"),
]


def build_effect(c: EngineClient, label: str, path: str, semantic: dict, preset: str) -> str:
    """Create, apply preset, compile. Returns previewEntity."""
    try:
        c.asset_browser_create_folder({"path": "Data/Effect/AI"})
    except EngineCommandError:
        pass

    c.effect_editor_create_asset({"path": path, "name": semantic["name"]})
    c.effect_editor_apply_preset({"path": path, "preset": preset, "semantic": semantic})

    c.effect_editor_open_workspace({"path": path})
    r = c.effect_editor_compile({"path": path})
    compile = r.get("compile", {})
    warnings = compile.get("warnings", [])
    valid    = compile.get("valid", False)
    print(f"  [{label}] compile valid={valid}  warnings={warnings}")

    r = c.effect_editor_timeline_play({"path": path, "loop": True})
    entity = r.get("previewEntity")
    print(f"  [{label}] entity={entity}")
    return entity or ""


def main():
    with EngineClient(timeout=15.0) as c:
        c.ping()
        print("connected")

        # --- セッション開始 ---
        c.ai_session_begin({"label": "InfernoRing"})

        # --- 3レイヤーを順次作成 ---
        print("\n--- Building layers ---")
        ring_entity   = build_effect(c, "ring",   RING_PATH,  RING_PARAMS,  "spark")
        time.sleep(0.3)
        spark_entity  = build_effect(c, "sparks", SPARK_PATH, SPARK_PARAMS, "spark")
        time.sleep(0.3)
        smoke_entity  = build_effect(c, "smoke",  SMOKE_PATH, SMOKE_PARAMS, "smoke")

        # --- リングを最前面に表示してプレビュー角度設定 ---
        c.effect_editor_open_workspace({"path": RING_PATH})
        c.effect_editor_set_preview_view({
            "yaw":        0.0,
            "pitch":     -0.55,
            "distance":   3.8,
            "clearColor": [0.05, 0.04, 0.06, 1.0],
        })
        c.effect_editor_compile({"path": RING_PATH})
        r = c.effect_editor_timeline_play({"path": RING_PATH, "loop": True})
        print(f"\n--- Main preview entity: {r.get('previewEntity')} ---")

        # --- パーティクルが溜まるまで待機 ---
        print("Waiting for particles to accumulate...")
        time.sleep(1.5)

        # --- effect_preview でリング単体を撮影 ---
        shot1 = c.capture_screenshot({"target": "effect_preview", "format": "png"})
        print(f"ring preview: {shot1.get('path')}")

        # --- セッション終了 ---
        c.ai_session_end({"commit": True})
        print("\nDone.")
        print(f"  InfernoRing  : {RING_PATH}")
        print(f"  InfernoSparks: {SPARK_PATH}")
        print(f"  InfernoSmoke : {SMOKE_PATH}")


if __name__ == "__main__":
    from engine_client import EngineConnectionError
    try:
        main()
    except EngineConnectionError as e:
        print(f"[ERROR] Cannot connect: {e}")
        sys.exit(1)
    except EngineCommandError as e:
        print(f"[ERROR] Command failed: {e}")
        sys.exit(1)
