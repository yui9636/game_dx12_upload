"""Create a fire slash effect in the Effect Editor."""
import sys
import time
import os

sys.path.insert(0, os.path.dirname(__file__))
from engine_client import EngineClient, EngineConnectionError, EngineCommandError

EFFECT_PATH = "Data/Effect/AI/FireSlash.effectgraph.json"

FIRE_SLASH_SEMANTIC = {
    "name":             "Fire Slash",
    "duration":         2.5,
    "spawnRate":        5000.0,
    "particleLifetime": 0.85,
    "startSize":        0.12,
    "endSize":          0.02,
    "speed":            2.5,
    "shape":            "line",
    "shapeParams":      [0.45, 0.0, 0.0],
    "spinRate":         2.0,
    "drawMode":         "ribbon",
    "ribbonWidth":      0.18,
    "ribbonStretch":    2.2,
    "curlNoiseStrength": 0.22,
    "curlNoiseScale":   0.18,
    "curlNoiseScroll":  0.22,
    "vortexStrength":   1.8,
    "startColor":       [1.0, 0.55, 0.05, 1.0],
    "endColor":         [0.85, 0.08, 0.0, 0.0],
    "alphaCurveBias":   0.3,
    "sizeCurveBias":    -0.2,
    "texture":          "Data/Effect/particle/slash_02.png",
}


def main():
    print("Connecting to engine...")
    with EngineClient(timeout=10.0) as client:

        # --- ping ---
        client.ping()
        print("  ping OK")

        # --- session begin ---
        session = client.ai_session_begin({"label": "FireSlash creation"})
        print(f"  session: {session.get('sessionId', '?')}")

        # --- create Data/Effect/AI folder (ignore error if exists) ---
        try:
            client.asset_browser_create_folder({"path": "Data/Effect/AI"})
            print("  folder created: Data/Effect/AI")
        except EngineCommandError:
            print("  folder already exists (ok)")

        # --- create the asset ---
        result = client.effect_editor_create_asset({"path": EFFECT_PATH, "name": "Fire Slash"})
        print(f"  asset created: {result.get('asset', {}).get('path', '?')}")

        # --- apply slash preset + fire color override ---
        result = client.effect_editor_apply_preset({
            "path":    EFFECT_PATH,
            "preset":  "slash",
            "semantic": FIRE_SLASH_SEMANTIC,
        })
        print(f"  preset applied: {result.get('asset', {}).get('name', '?')}")

        # --- open in effect editor ---
        result = client.effect_editor_open_workspace({"path": EFFECT_PATH})
        print(f"  workspace opened: {result.get('effectEditorActive', '?')}")

        # give the editor a frame to settle
        time.sleep(0.3)

        # --- set preview camera ---
        client.effect_editor_set_preview_view({
            "yaw":      0.85,
            "pitch":    -0.18,
            "distance": 4.5,
            "clearColor": [0.05, 0.05, 0.08, 1.0],
        })
        print("  preview view set")

        # --- play timeline ---
        client.effect_editor_timeline_play({"path": EFFECT_PATH, "loop": True})
        print("  timeline playing")

        # let it run for a couple frames before screenshot
        time.sleep(0.5)

        # --- screenshot ---
        shot = client.capture_screenshot({"target": "effect_editor", "format": "png"})
        print(f"  screenshot: {shot.get('path', '?')}")
        print(f"  size: {shot.get('width', '?')}x{shot.get('height', '?')}")

        # --- end session ---
        client.ai_session_end({"commit": True})
        print("  session committed")

    print("\nDone. Fire Slash effect created at:", EFFECT_PATH)


if __name__ == "__main__":
    try:
        main()
    except EngineConnectionError as e:
        print(f"\n[ERROR] Cannot connect to engine: {e}")
        print("Make sure the engine is running (Game.exe in editor mode).")
        sys.exit(1)
    except EngineCommandError as e:
        print(f"\n[ERROR] Engine command failed: {e}")
        sys.exit(1)
