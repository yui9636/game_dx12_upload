from __future__ import annotations

import json
import struct
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Sequence


class VerificationValidationError(ValueError):
    pass


class VerificationRunner:
    """Phase 5 verification runner for engine state, assets, files, and screenshots."""

    def __init__(self, tools: Any, project_root: Optional[Path] = None):
        self.tools = tools
        self.project_root = Path.cwd() if project_root is None else Path(project_root)

    def validate(self, spec: Any) -> Dict[str, Any]:
        normalized = self._normalize_spec(spec)
        errors: List[str] = []
        ids: set[str] = set()
        supported = sorted(self.supported_check_types())

        for index, check in enumerate(normalized["checks"]):
            if not isinstance(check, Mapping):
                errors.append(f"checks[{index}] must be an object.")
                continue
            check_id = str(check.get("id") or f"check_{index}")
            if check_id in ids:
                errors.append(f"Duplicate check id: {check_id}")
            ids.add(check_id)
            check_type = str(check.get("type") or "")
            if not check_type:
                errors.append(f"{check_id}: type is required.")
            elif check_type not in supported:
                errors.append(f"{check_id}: unsupported check type {check_type!r}.")

        return {
            "ok": not errors,
            "errors": errors,
            "checkCount": len(normalized["checks"]),
            "supportedTypes": supported,
        }

    def run(
        self,
        spec: Any,
        *,
        dry_run: bool = False,
        variables: Optional[Mapping[str, Any]] = None,
        report_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        normalized = self._normalize_spec(spec)
        validation = self.validate(normalized)
        if not validation["ok"]:
            raise VerificationValidationError("; ".join(validation["errors"]))

        context = {
            "vars": dict(normalized.get("variables", {})),
            "verification": {
                "name": normalized.get("name", "verification"),
                "dryRun": dry_run,
            },
        }
        if variables:
            context["vars"].update(dict(variables))

        started = time.time()
        report: Dict[str, Any] = {
            "name": context["verification"]["name"],
            "dryRun": dry_run,
            "startedAt": self._timestamp(started),
            "validation": validation,
            "checks": [],
            "summary": {
                "ok": True,
                "passed": 0,
                "failed": 0,
                "skipped": 0,
            },
        }

        for index, raw_check in enumerate(normalized["checks"]):
            check = self._resolve(raw_check, context)
            check_report = self._run_check(check, index=index, dry_run=dry_run)
            report["checks"].append(check_report)
            if check_report.get("skipped"):
                report["summary"]["skipped"] += 1
            elif check_report.get("ok"):
                report["summary"]["passed"] += 1
            else:
                report["summary"]["failed"] += 1
                report["summary"]["ok"] = False

        finished = time.time()
        report["finishedAt"] = self._timestamp(finished)
        report["durationMs"] = int((finished - started) * 1000)
        report["variables"] = context["vars"]
        report["summary"]["ok"] = report["summary"]["failed"] == 0

        target_path = report_path or normalized.get("reportPath")
        if target_path:
            resolved_path = str(self._resolve(target_path, context))
            if not dry_run:
                target = self._project_path(resolved_path)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            report["reportPath"] = resolved_path
        return report

    def supported_check_types(self) -> set[str]:
        return {
            "engine.ping",
            "coverage",
            "entity.exists",
            "entity.component",
            "asset.exists",
            "file.exists",
            "image.bmp",
            "visual.selected_visible",
            "scene_view.state",
            "game_view.state",
            "workflow.report",
        }

    def _normalize_spec(self, spec: Any) -> Dict[str, Any]:
        if isinstance(spec, list):
            return {"name": "verification", "checks": spec}
        if not isinstance(spec, Mapping):
            raise VerificationValidationError("Verification spec must be an object or a list of checks.")
        normalized = dict(spec)
        if "checks" not in normalized:
            raise VerificationValidationError("Verification spec must contain a checks array.")
        if not isinstance(normalized["checks"], list):
            raise VerificationValidationError("Verification checks must be an array.")
        return normalized

    def _run_check(self, check: Mapping[str, Any], *, index: int, dry_run: bool) -> Dict[str, Any]:
        check_id = str(check.get("id") or f"check_{index}")
        check_type = str(check.get("type") or "")
        started = time.time()
        report: Dict[str, Any] = {
            "id": check_id,
            "type": check_type,
            "startedAt": self._timestamp(started),
        }

        if not bool(check.get("enabled", True)):
            report.update({
                "ok": True,
                "skipped": True,
                "reason": "disabled",
                "finishedAt": self._timestamp(),
                "durationMs": int((time.time() - started) * 1000),
            })
            return report

        if dry_run:
            report.update({
                "ok": True,
                "dryRun": True,
                "finishedAt": self._timestamp(),
                "durationMs": int((time.time() - started) * 1000),
            })
            return report

        try:
            details = self._dispatch_check(check_type, check)
            passed, failures = self._evaluate_expectations(check, details)
            report.update({
                "ok": passed,
                "details": details,
                "failures": failures,
                "finishedAt": self._timestamp(),
                "durationMs": int((time.time() - started) * 1000),
            })
            return report
        except Exception as exc:
            report.update({
                "ok": False,
                "error": str(exc),
                "finishedAt": self._timestamp(),
                "durationMs": int((time.time() - started) * 1000),
            })
            return report

    def _dispatch_check(self, check_type: str, check: Mapping[str, Any]) -> Dict[str, Any]:
        if check_type == "engine.ping":
            return {"response": self.tools.runtime.ping()}
        if check_type == "coverage":
            return self.tools.coverage_report()
        if check_type == "entity.exists":
            return self._check_entity_exists(check)
        if check_type == "entity.component":
            return self._check_entity_component(check)
        if check_type == "asset.exists":
            return self._check_asset_exists(check)
        if check_type == "file.exists":
            return self._check_file_exists(check)
        if check_type == "image.bmp":
            return self._check_bmp_image(check)
        if check_type == "visual.selected_visible":
            return self._check_selected_visible(check)
        if check_type == "scene_view.state":
            return self.tools.editor.scene_view_state()
        if check_type == "game_view.state":
            return self.tools.editor.game_view_state()
        if check_type == "workflow.report":
            return self._check_workflow_report(check)
        raise VerificationValidationError(f"Unsupported check type: {check_type}")

    def _check_entity_exists(self, check: Mapping[str, Any]) -> Dict[str, Any]:
        if check.get("entity"):
            entity = self.tools.entities.get(str(check["entity"]))
            return {"count": 1, "entities": [entity]}
        name = str(check.get("name") or "")
        if not name:
            raise VerificationValidationError("entity.exists requires name or entity.")
        matches = self.tools.entities.find_by_name(name, exact=bool(check.get("exact", False)))
        return {"count": len(matches), "entities": matches}

    def _check_entity_component(self, check: Mapping[str, Any]) -> Dict[str, Any]:
        entity = str(check.get("entity") or "")
        if not entity and check.get("name"):
            entity = self.tools.entities.id_by_name(str(check["name"]), exact=bool(check.get("exact", False))) or ""
        component = str(check.get("component") or check.get("componentType") or "")
        if not entity or not component:
            raise VerificationValidationError("entity.component requires entity/name and component.")
        value = self.tools.entities.get_component(entity, component)
        return {"entity": entity, "component": component, "value": value}

    def _check_asset_exists(self, check: Mapping[str, Any]) -> Dict[str, Any]:
        path = str(check.get("path") or "")
        if path:
            file_check = self._check_file_exists({"path": path, "minBytes": check.get("minBytes", 1)})
            return {"path": path, "file": file_check}
        query = str(check.get("query") or "")
        if not query:
            raise VerificationValidationError("asset.exists requires path or query.")
        result = self.tools.assets.search(
            query,
            root=str(check.get("root", "Data")),
            type_filter=str(check.get("type", "")),
            limit=int(check.get("limit", 100)),
        )
        entries = result.get("results", []) if isinstance(result, Mapping) else []
        return {"query": query, "count": len(entries), "results": entries}

    def _check_file_exists(self, check: Mapping[str, Any]) -> Dict[str, Any]:
        path = self._project_path(str(check.get("path") or ""))
        exists = path.exists()
        size = path.stat().st_size if exists else 0
        return {"path": str(path), "exists": exists, "size": size}

    def _check_bmp_image(self, check: Mapping[str, Any]) -> Dict[str, Any]:
        file_info = self._check_file_exists(check)
        if not file_info["exists"]:
            return {**file_info, "validBmp": False}
        path = Path(file_info["path"])
        data = path.read_bytes()
        info = self._parse_bmp(data)
        return {**file_info, **info}

    def _check_selected_visible(self, check: Mapping[str, Any]) -> Dict[str, Any]:
        visual = self.tools.editor.visual_state()
        selected = visual.get("selectedVisuals", []) if isinstance(visual, Mapping) else []
        entity = str(check.get("entity") or "")
        name = str(check.get("name") or "")
        exact = bool(check.get("exact", False))
        visible = []
        for item in selected:
            if not isinstance(item, Mapping):
                continue
            if entity and str(item.get("entity")) != entity:
                continue
            if name:
                current_name = str(item.get("name") or "")
                if (current_name != name) if exact else (name.lower() not in current_name.lower()):
                    continue
            screen = item.get("sceneScreenPosition", {})
            if isinstance(screen, Mapping) and bool(screen.get("visibleInSceneView", False)):
                visible.append(item)
        return {"selectedCount": len(selected), "visibleCount": len(visible), "visible": visible, "visualState": visual}

    def _check_workflow_report(self, check: Mapping[str, Any]) -> Dict[str, Any]:
        file_info = self._check_file_exists(check)
        if not file_info["exists"]:
            return {**file_info, "summaryOk": False}
        data = json.loads(Path(file_info["path"]).read_text(encoding="utf-8"))
        summary = data.get("summary", {}) if isinstance(data, Mapping) else {}
        return {**file_info, "summary": summary, "summaryOk": bool(summary.get("ok", False))}

    def _evaluate_expectations(self, check: Mapping[str, Any], details: Mapping[str, Any]) -> tuple[bool, List[str]]:
        failures: List[str] = []
        check_type = str(check.get("type") or "")

        if check_type == "coverage" and bool(check.get("phase3Complete", True)):
            self._expect(bool(details.get("phase3Complete", False)), "phase3Complete is not true.", failures)
        if check_type == "entity.exists":
            self._expect(int(details.get("count", 0)) >= int(check.get("min", 1)), "Entity count is below minimum.", failures)
        if check_type == "asset.exists":
            if check.get("path"):
                file_info = details.get("file", {})
                self._expect(bool(file_info.get("exists", False)), "Asset path does not exist.", failures)
                self._expect(int(file_info.get("size", 0)) >= int(check.get("minBytes", 1)), "Asset file is too small.", failures)
            else:
                self._expect(int(details.get("count", 0)) >= int(check.get("min", 1)), "Asset search count is below minimum.", failures)
        if check_type == "file.exists":
            self._expect(bool(details.get("exists", False)), "File does not exist.", failures)
            self._expect(int(details.get("size", 0)) >= int(check.get("minBytes", 1)), "File size is below minimum.", failures)
        if check_type == "image.bmp":
            self._expect(bool(details.get("exists", False)), "Image file does not exist.", failures)
            self._expect(bool(details.get("validBmp", False)), "Image is not a valid BMP.", failures)
            self._expect(int(details.get("size", 0)) >= int(check.get("minBytes", 1)), "Image file is too small.", failures)
            self._expect(int(details.get("width", 0)) >= int(check.get("minWidth", 1)), "Image width is below minimum.", failures)
            self._expect(int(details.get("height", 0)) >= int(check.get("minHeight", 1)), "Image height is below minimum.", failures)
            self._expect(int(details.get("uniqueColors", 0)) >= int(check.get("minUniqueColors", 1)), "Image has too few unique colors.", failures)
            if "minNonBlackRatio" in check:
                self._expect(float(details.get("nonBlackRatio", 0.0)) >= float(check["minNonBlackRatio"]), "Image non-black ratio is too low.", failures)
        if check_type == "visual.selected_visible":
            self._expect(int(details.get("visibleCount", 0)) >= int(check.get("minVisible", 1)), "Selected visual is not visible in Scene View.", failures)
        if check_type == "workflow.report":
            self._expect(bool(details.get("exists", False)), "Workflow report does not exist.", failures)
            self._expect(bool(details.get("summaryOk", False)), "Workflow report summary is not ok.", failures)

        equals = check.get("equals", {})
        if isinstance(equals, Mapping):
            for path, expected in equals.items():
                actual = self._get_path(details, str(path))
                self._expect(actual == expected, f"{path} expected {expected!r}, got {actual!r}.", failures)

        minimums = check.get("minimums", {})
        if isinstance(minimums, Mapping):
            for path, expected in minimums.items():
                actual = self._get_path(details, str(path))
                self._expect(float(actual) >= float(expected), f"{path} expected >= {expected!r}, got {actual!r}.", failures)

        maximums = check.get("maximums", {})
        if isinstance(maximums, Mapping):
            for path, expected in maximums.items():
                actual = self._get_path(details, str(path))
                self._expect(float(actual) <= float(expected), f"{path} expected <= {expected!r}, got {actual!r}.", failures)

        return not failures, failures

    def _parse_bmp(self, data: bytes) -> Dict[str, Any]:
        if len(data) < 54 or data[:2] != b"BM":
            return {"validBmp": False}
        file_size = struct.unpack_from("<I", data, 2)[0]
        pixel_offset = struct.unpack_from("<I", data, 10)[0]
        dib_size = struct.unpack_from("<I", data, 14)[0]
        if dib_size < 40 or len(data) < 14 + dib_size:
            return {"validBmp": False}
        width = struct.unpack_from("<i", data, 18)[0]
        height_raw = struct.unpack_from("<i", data, 22)[0]
        planes = struct.unpack_from("<H", data, 26)[0]
        bits_per_pixel = struct.unpack_from("<H", data, 28)[0]
        compression = struct.unpack_from("<I", data, 30)[0]
        height = abs(height_raw)
        valid = planes == 1 and width > 0 and height > 0 and pixel_offset < len(data)
        unique_colors = 0
        avg_brightness = 0.0
        non_black_ratio = 0.0

        if valid and compression == 0 and bits_per_pixel in (24, 32):
            bytes_per_pixel = bits_per_pixel // 8
            stride = ((width * bits_per_pixel + 31) // 32) * 4
            sample_limit = 4096
            step_x = max(1, width // 64)
            step_y = max(1, height // 64)
            colors = set()
            brightness_total = 0
            non_black = 0
            samples = 0
            for y in range(0, height, step_y):
                row = height - 1 - y if height_raw > 0 else y
                row_offset = pixel_offset + row * stride
                for x in range(0, width, step_x):
                    offset = row_offset + x * bytes_per_pixel
                    if offset + 2 >= len(data):
                        continue
                    b, g, r = data[offset], data[offset + 1], data[offset + 2]
                    colors.add((r, g, b))
                    brightness = int(r) + int(g) + int(b)
                    brightness_total += brightness
                    if brightness > 12:
                        non_black += 1
                    samples += 1
                    if samples >= sample_limit:
                        break
                if samples >= sample_limit:
                    break
            unique_colors = len(colors)
            avg_brightness = (brightness_total / samples / 3.0) if samples else 0.0
            non_black_ratio = (non_black / samples) if samples else 0.0

        return {
            "validBmp": valid,
            "fileSizeHeader": file_size,
            "pixelOffset": pixel_offset,
            "width": width,
            "height": height,
            "bitsPerPixel": bits_per_pixel,
            "compression": compression,
            "uniqueColors": unique_colors,
            "averageBrightness": avg_brightness,
            "nonBlackRatio": non_black_ratio,
        }

    def _project_path(self, path: str) -> Path:
        target = Path(path)
        if target.is_absolute():
            return target
        return self.project_root / target

    def _resolve(self, value: Any, context: Mapping[str, Any]) -> Any:
        if isinstance(value, str):
            result = value
            for key, replacement in context.get("vars", {}).items():
                token = "${vars." + str(key) + "}"
                if token in result:
                    result = result.replace(token, str(replacement))
            for key, replacement in context.get("verification", {}).items():
                token = "${verification." + str(key) + "}"
                if token in result:
                    result = result.replace(token, str(replacement))
            return result
        if isinstance(value, list):
            return [self._resolve(item, context) for item in value]
        if isinstance(value, Mapping):
            return {str(k): self._resolve(v, context) for k, v in value.items()}
        return value

    def _get_path(self, data: Mapping[str, Any], path: str) -> Any:
        current: Any = data
        for part in path.split("."):
            if isinstance(current, Mapping) and part in current:
                current = current[part]
            elif isinstance(current, Sequence) and not isinstance(current, (str, bytes)):
                current = current[int(part)]
            else:
                raise KeyError(path)
        return current

    def _expect(self, condition: bool, message: str, failures: List[str]) -> None:
        if not condition:
            failures.append(message)

    def _timestamp(self, value: Optional[float] = None) -> str:
        dt = datetime.fromtimestamp(time.time() if value is None else value, timezone.utc)
        return dt.isoformat(timespec="milliseconds").replace("+00:00", "Z")
