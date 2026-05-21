from __future__ import annotations

import json
from pathlib import Path
import sys

SDK_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SDK_ROOT))

from engine_client import EngineClient
from tools import EngineTools


def main() -> int:
    with EngineClient() as engine:
        tools = EngineTools(engine)
        result = tools.create_battle_layout(
            name="AI_SampleBattleLayout",
            center=[0.0, 0.0, 0.0],
            radius=6.0,
            enemy_count=3,
            capture=True,
        )
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
