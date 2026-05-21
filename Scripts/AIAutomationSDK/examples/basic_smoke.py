from __future__ import annotations

import json
from pathlib import Path
import sys

SDK_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SDK_ROOT))

from engine_client import EngineClient


def main() -> int:
    with EngineClient() as engine:
        result = {
            "ping": engine.ping(),
            "state": engine.get_engine_state(),
            "entities": engine.list_entities(),
        }
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
