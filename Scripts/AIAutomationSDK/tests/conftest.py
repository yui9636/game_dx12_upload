"""pytest fixtures for AI automation smoke tests.

Engine must be running before the tests start.
URL can be overridden via env var ENGINE_URL.
"""
from __future__ import annotations
import os
import sys
import pytest

# Make engine_client importable when running pytest from any cwd.
_HERE = os.path.dirname(os.path.abspath(__file__))
_SDK_DIR = os.path.dirname(_HERE)
if _SDK_DIR not in sys.path:
    sys.path.insert(0, _SDK_DIR)

from engine_client import EngineClient, EngineConnectionError  # noqa: E402

URL = os.environ.get("ENGINE_URL", "ws://127.0.0.1:9876")


@pytest.fixture(scope="session")
def engine_url() -> str:
    return URL


@pytest.fixture
def client(engine_url: str):
    """Per-test connected client. Skips test if engine is not running."""
    try:
        c = EngineClient(engine_url, timeout=20.0)
        c.connect()
        # Sanity ping. If this fails, skip rather than blow up downstream.
        c.command("ping")
    except (EngineConnectionError, OSError) as e:
        pytest.skip(f"Engine not running at {engine_url}: {e}")
    try:
        yield c
    finally:
        c.close()
