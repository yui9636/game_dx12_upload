from __future__ import annotations

import base64
import hashlib
import json
import re
import secrets
import socket
import struct
import time
from dataclasses import dataclass
from typing import Any, Dict, Iterable, List, Mapping, Optional
from urllib.parse import urlparse


PROTOCOL_VERSION = 1
DEFAULT_URL = "ws://127.0.0.1:9876"
DEFAULT_TIMEOUT = 5.0


COMMANDS: List[str] = [
    "ping",
    "batch",
    "ai_session.begin",
    "ai_session.status",
    "ai_session.rollback",
    "ai_session.end",
    "get_engine_state",
    "get_visual_state",
    "editor.recovery.get_state",
    "editor.recovery.restore",
    "editor.recovery.dismiss",
    "get_component_schema",
    "asset_browser.list",
    "asset_browser.search",
    "asset_browser.create_folder",
    "asset_browser.copy",
    "asset_browser.move",
    "asset_browser.rename",
    "asset_browser.delete",
    "material.create",
    "material.get",
    "material.set",
    "effect_editor.list_node_types",
    "effect_editor.create_asset",
    "effect_editor.apply_preset",
    "effect_editor.open_workspace",
    "effect_editor.set_preview_view",
    "effect_editor.get_state",
    "effect_editor.timeline_play",
    "effect_editor.timeline_seek",
    "effect_editor.timeline_step",
    "effect_editor.timeline_stop",
    "effect_editor.select_node",
    "effect_editor.focus_node",
    "effect_editor.assert_preview_visible",
    "effect_editor.capture_review_set",
    "effect_editor.capture_multi_time_review",
    "effect_editor.get_asset",
    "effect_editor.set_asset",
    "effect_editor.set_semantic_params",
    "effect_editor.add_node",
    "effect_editor.set_node",
    "effect_editor.delete_node",
    "effect_editor.connect",
    "effect_editor.disconnect",
    "effect_editor.compile",
    "visual.evaluate_capture",
    "visual.verify_entity",
    "visual.verify_entity_game_view",
    "visual.assert_entities_visible",
    "visual.capture_review_set",
    "capture_screenshot",
    "ecs.query",
    "ecs.hierarchy",
    "ecs.diff",
    "list_entities",
    "get_entity",
    "get_component",
    "select_entity",
    "add_component",
    "remove_component",
    "set_component_fields",
    "create_empty",
    "create_model_entity",
    "set_transform",
    "delete_entity",
    "duplicate_entity",
    "reparent_entity",
    "instantiate_prefab",
    "focus_entity",
    "frame_selection",
    "raycast_scene_view",
    "place_asset_at_cursor",
    "scene_view.frame_entities",
    "scene_view.frame_all",
    "camera.frame_entities",
    "prefab.save",
    "prefab.apply",
    "prefab.unpack",
    "material.assign",
    "light.create",
    "camera.create",
    "terrain.create",
    "terrain.list",
    "terrain.apply_brush",
    "terrain.save",
    "terrain.load",
    "terrain.get",
    "terrain.set_dimensions",
    "terrain.set_noise",
    "terrain.set_erosion",
    "terrain.run_erosion",
    "terrain.set_auto_splat",
    "terrain.set_water",
    "terrain.fit_water",
    "terrain.apply_preset",
    "terrain.regenerate",
    "terrain.set_layer",
    "terrain.get_foliage",
    "terrain.set_foliage",
    "terrain.add_foliage_layer",
    "terrain.set_foliage_layer",
    "terrain.delete_foliage_layer",
    "terrain.reset_foliage",
    "effect_editor.preview_spawn",
    "save_scene",
    "load_scene",
    "terrain.open",
    "terrain.set_brush",
    "coingame.start",
    "coingame.reset",
    "coingame.status",
    "coingame.tag_coins",
    "play",
    "stop",
    "pause",
    "step",
    "game.play",
    "game.stop",
    "game.pause",
    "game.step_frames",
    "game.set_time_scale",
    "gameplay.get_state",
    "gameplay.get_events",
    "gameplay.clear_events",
    "game.input.press",
    "game.input.release",
    "game.input.tap",
    "game.input.axis",
    "game.input.mouse_move",
    "player_editor.open",
    "player_editor.get_status",
    "player_editor.load_model",
    "player_editor.get_state_machine",
    "player_editor.add_state",
    "player_editor.set_state",
    "player_editor.delete_state",
    "player_editor.add_transition",
    "player_editor.set_transition",
    "player_editor.delete_transition",
    "player_editor.select",
    "player_editor.set_default_state",
    "player_editor.get_timeline",
    "player_editor.add_track",
    "player_editor.delete_track",
    "player_editor.set_playhead",
    "player_editor.timeline_play",
    "player_editor.save_prefab",
    "game_loop_editor.open",
    "game_loop_editor.get_status",
    "game_loop_editor.get_asset",
    "game_loop_editor.load",
    "game_loop_editor.save",
    "game_loop_editor.validate",
    "game_loop_editor.add_node",
    "game_loop_editor.set_node",
    "game_loop_editor.delete_node",
    "game_loop_editor.add_transition",
    "game_loop_editor.set_transition",
    "game_loop_editor.delete_transition",
    "game_loop_editor.select",
    "game_loop_editor.set_start_node",
    "game_loop_editor.reverse_transition",
    "game_loop_editor.fit_graph",
    "gameloop_editor.open",
    "gameloop_editor.get_status",
    "gameloop_editor.get_asset",
    "gameloop_editor.load",
    "gameloop_editor.save",
    "gameloop_editor.validate",
    "player_editor.get_sockets",
    "player_editor.add_socket",
    "player_editor.set_socket",
    "player_editor.delete_socket",
    "player_editor.get_colliders",
    "player_editor.add_collider",
    "player_editor.set_collider",
    "player_editor.delete_collider",
    "player_editor.get_animations",
    "player_editor.set_animator",
    "player_editor.get_input_map",
    "player_editor.add_action_binding",
    "player_editor.set_action_binding",
    "player_editor.delete_action_binding",
    "player_editor.add_axis_binding",
    "player_editor.set_axis_binding",
    "player_editor.delete_axis_binding",
    "scene_view.get_state",
    "scene_view.set_camera",
    "scene_view.set_look_at",
    "scene_view.set_gizmo_operation",
    "scene_view.set_gizmo_space",
    "scene_view.set_shading_mode",
    "scene_view.set_mode",
    "scene_view.set_visibility",
    "scene_view.save_bookmark",
    "scene_view.load_bookmark",
    "lighting.open",
    "lighting.get",
    "lighting.set_environment",
    "lighting.set_post_effects",
    "ui_editor.open",
    "ui_editor.get_state",
    "ui_editor.select",
    "ui_editor.create_canvas",
    "ui_editor.create_template",
    "ui_editor.create_part",
    "ui_editor.save_prefab",
    "ui_editor.apply_prefab",
    "ui_editor.revert_prefab",
    "ui_editor.unpack_prefab",
    "ui_editor.set_view",
    "sequencer.open",
    "sequencer.new",
    "sequencer.load",
    "sequencer.save",
    "sequencer.get",
    "sequencer.set_playback",
    "sequencer.add_binding",
    "sequencer.add_track",
    "sequencer.add_master_track",
    "sequencer.add_section",
    "sequencer.select",
    "sequencer.set_params",
    "scene.new",
    "editor.undo",
    "editor.redo",
    "editor.focus_panel",
    "editor.get_panels",
    "editor.show_panel",
    "scene_view.get_snap",
    "scene_view.set_snap",
    "scene_view.set_grid",
    "scene_view.set_2d_view",
    "scene_view.align_camera",
    "game_view.get_state",
    "game_view.set_resolution",
    "game_view.set_overlay",
    "model_serializer.build",
    "fire_ui_button",
    "push_flow_event",
    "gameflow.register",
]


def command_to_method_name(command_name: str) -> str:
    return re.sub(r"[^0-9A-Za-z_]+", "_", command_name).strip("_")


COMMAND_METHODS: Dict[str, str] = {
    command_to_method_name(command): command for command in COMMANDS
}


class EngineConnectionError(RuntimeError):
    pass


class EngineProtocolError(RuntimeError):
    pass


class EngineCommandError(RuntimeError):
    def __init__(self, response: Mapping[str, Any]):
        self.response = dict(response)
        error = self.response.get("error") or {}
        code = error.get("code", "command_failed") if isinstance(error, Mapping) else "command_failed"
        message = error.get("message", "Engine command failed.") if isinstance(error, Mapping) else str(error)
        super().__init__(f"{code}: {message}")


@dataclass(frozen=True)
class EngineEndpoint:
    url: str
    host: str
    port: int
    path: str


def parse_endpoint(url: str) -> EngineEndpoint:
    parsed = urlparse(url)
    if parsed.scheme != "ws":
        raise ValueError(f"Only ws:// URLs are supported, got {url!r}.")
    if not parsed.hostname:
        raise ValueError(f"WebSocket URL has no host: {url!r}.")
    port = parsed.port or 80
    path = parsed.path or "/"
    if parsed.query:
        path += "?" + parsed.query
    return EngineEndpoint(url=url, host=parsed.hostname, port=port, path=path)


class EngineClient:
    """Dependency-free WebSocket client for the engine automation server."""

    commands = tuple(COMMANDS)
    command_methods = dict(COMMAND_METHODS)

    def __init__(self, url: str = DEFAULT_URL, timeout: float = DEFAULT_TIMEOUT):
        self.endpoint = parse_endpoint(url)
        self.timeout = float(timeout)
        self._socket: Optional[socket.socket] = None
        self._recv_buffer = b""
        self._next_id = 1

    def __enter__(self) -> "EngineClient":
        self.connect()
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        self.close()

    @property
    def is_connected(self) -> bool:
        return self._socket is not None

    def connect(self) -> "EngineClient":
        if self._socket is not None:
            return self

        sock = socket.create_connection((self.endpoint.host, self.endpoint.port), self.timeout)
        sock.settimeout(self.timeout)
        self._socket = sock
        self._recv_buffer = b""

        key = base64.b64encode(secrets.token_bytes(16)).decode("ascii")
        request = (
            f"GET {self.endpoint.path} HTTP/1.1\r\n"
            f"Host: {self.endpoint.host}:{self.endpoint.port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        )
        self._send_all(request.encode("ascii"))

        headers = self._read_http_headers()
        status_line = headers.split("\r\n", 1)[0]
        if " 101 " not in f" {status_line} ":
            self.close()
            raise EngineConnectionError(f"WebSocket upgrade failed: {status_line}")

        header_map = self._parse_headers(headers)
        accept = header_map.get("sec-websocket-accept")
        expected = base64.b64encode(
            hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()
        ).decode("ascii")
        if accept != expected:
            self.close()
            raise EngineConnectionError("WebSocket server returned an invalid Sec-WebSocket-Accept header.")

        return self

    def close(self) -> None:
        sock = self._socket
        self._socket = None
        self._recv_buffer = b""
        if sock is None:
            return
        try:
            self._send_frame_with_socket(sock, 0x8, b"")
        except OSError:
            pass
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        sock.close()

    def command(
        self,
        command_name: str,
        params: Optional[Mapping[str, Any]] = None,
        *,
        command_id: Optional[str] = None,
        raw_response: bool = False,
        timeout: Optional[float] = None,
    ) -> Any:
        if command_name not in COMMANDS:
            raise ValueError(f"Unknown automation command: {command_name!r}.")

        if self._socket is None:
            self.connect()

        old_timeout = None
        if timeout is not None and self._socket is not None:
            old_timeout = self._socket.gettimeout()
            self._socket.settimeout(float(timeout))

        try:
            payload = {
                "version": PROTOCOL_VERSION,
                "id": command_id or self._make_command_id(command_name),
                "command": command_name,
                "params": dict(params or {}),
            }
            self._send_text(json.dumps(payload, separators=(",", ":")))
            response_text = self._recv_text()
            response = json.loads(response_text)
            if not isinstance(response, dict):
                raise EngineProtocolError("Engine response must be a JSON object.")
            if not response.get("ok", False):
                if raw_response:
                    return response
                raise EngineCommandError(response)
            return response if raw_response else response.get("result")
        finally:
            if old_timeout is not None and self._socket is not None:
                self._socket.settimeout(old_timeout)

    def request(self, command_name: str, params: Optional[Mapping[str, Any]] = None, **kwargs: Any) -> Any:
        return self.command(command_name, params, **kwargs)

    def available_commands(self) -> List[str]:
        return list(COMMANDS)

    def available_methods(self) -> List[str]:
        return sorted(COMMAND_METHODS)

    def _make_command_id(self, command_name: str) -> str:
        safe = command_to_method_name(command_name) or "command"
        value = f"{safe}-{self._next_id:04d}-{int(time.time() * 1000)}"
        self._next_id += 1
        return value

    def _send_text(self, text: str) -> None:
        self._send_frame(0x1, text.encode("utf-8"))

    def _recv_text(self) -> str:
        while True:
            opcode, payload = self._recv_frame()
            if opcode == 0x1:
                return payload.decode("utf-8")
            if opcode == 0x8:
                self.close()
                raise EngineConnectionError("Engine closed the WebSocket connection.")
            if opcode == 0x9:
                self._send_frame(0xA, payload)
                continue
            if opcode == 0xA:
                continue
            raise EngineProtocolError(f"Unsupported WebSocket opcode from engine: {opcode}")

    def _send_frame(self, opcode: int, payload: bytes) -> None:
        sock = self._require_socket()
        self._send_frame_with_socket(sock, opcode, payload)

    def _send_frame_with_socket(self, sock: socket.socket, opcode: int, payload: bytes) -> None:
        first = 0x80 | (opcode & 0x0F)
        length = len(payload)
        mask_bit = 0x80
        header = bytearray([first])
        if length <= 125:
            header.append(mask_bit | length)
        elif length <= 0xFFFF:
            header.append(mask_bit | 126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(mask_bit | 127)
            header.extend(struct.pack("!Q", length))

        mask = secrets.token_bytes(4)
        masked = bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload))
        self._send_all(bytes(header) + mask + masked, sock=sock)

    def _recv_frame(self) -> tuple[int, bytes]:
        header = self._recv_exact(2)
        first, second = header[0], header[1]
        fin = bool(first & 0x80)
        opcode = first & 0x0F
        masked = bool(second & 0x80)
        length = second & 0x7F

        if not fin:
            raise EngineProtocolError("Fragmented WebSocket frames are not supported by this SDK.")
        if length == 126:
            length = struct.unpack("!H", self._recv_exact(2))[0]
        elif length == 127:
            length = struct.unpack("!Q", self._recv_exact(8))[0]

        mask = self._recv_exact(4) if masked else b""
        payload = self._recv_exact(length) if length else b""
        if masked:
            payload = bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload))
        return opcode, payload

    def _read_http_headers(self) -> str:
        marker = b"\r\n\r\n"
        data = self._recv_buffer
        while marker not in data:
            chunk = self._require_socket().recv(4096)
            if not chunk:
                raise EngineConnectionError("Connection closed during WebSocket handshake.")
            data += chunk
            if len(data) > 64 * 1024:
                raise EngineConnectionError("WebSocket handshake header is too large.")

        index = data.index(marker) + len(marker)
        header_bytes = data[:index]
        self._recv_buffer = data[index:]
        return header_bytes.decode("iso-8859-1")

    def _parse_headers(self, headers: str) -> Dict[str, str]:
        parsed: Dict[str, str] = {}
        for line in headers.split("\r\n")[1:]:
            if not line or ":" not in line:
                continue
            key, value = line.split(":", 1)
            parsed[key.strip().lower()] = value.strip()
        return parsed

    def _send_all(self, data: bytes, *, sock: Optional[socket.socket] = None) -> None:
        (sock or self._require_socket()).sendall(data)

    def _recv_exact(self, size: int) -> bytes:
        data = self._recv_buffer
        while len(data) < size:
            chunk = self._require_socket().recv(size - len(data))
            if not chunk:
                raise EngineConnectionError("Connection closed while receiving WebSocket data.")
            data += chunk
        result = data[:size]
        self._recv_buffer = data[size:]
        return result

    def _require_socket(self) -> socket.socket:
        if self._socket is None:
            raise EngineConnectionError("EngineClient is not connected.")
        return self._socket


def _make_command_method(command_name: str):
    def method(
        self: EngineClient,
        params: Optional[Mapping[str, Any]] = None,
        *,
        command_id: Optional[str] = None,
        raw_response: bool = False,
        timeout: Optional[float] = None,
        **kwargs: Any,
    ) -> Any:
        merged: Dict[str, Any] = {}
        if params is not None:
            if not isinstance(params, Mapping):
                raise TypeError("params must be a mapping when provided.")
            merged.update(params)
        merged.update(kwargs)
        return self.command(
            command_name,
            merged,
            command_id=command_id,
            raw_response=raw_response,
            timeout=timeout,
        )

    method.__name__ = command_to_method_name(command_name)
    method.__qualname__ = f"EngineClient.{method.__name__}"
    method.__doc__ = f"Call the {command_name!r} automation command."
    return method


for _method_name, _command_name in COMMAND_METHODS.items():
    if not hasattr(EngineClient, _method_name):
        setattr(EngineClient, _method_name, _make_command_method(_command_name))


def _capture_scene_view(self: EngineClient, *args: Any, **kwargs: Any) -> Any:
    return self.capture_screenshot(*args, **kwargs)


EngineClient.capture_scene_view = _capture_scene_view  # type: ignore[attr-defined]


__all__ = [
    "COMMANDS",
    "COMMAND_METHODS",
    "DEFAULT_TIMEOUT",
    "DEFAULT_URL",
    "EngineClient",
    "EngineCommandError",
    "EngineConnectionError",
    "EngineProtocolError",
    "PROTOCOL_VERSION",
    "command_to_method_name",
]
