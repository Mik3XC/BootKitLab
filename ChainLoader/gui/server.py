#!/usr/bin/env python3
"""BootKitStudio defensive GUI server.

This server intentionally supports only defensive lab simulation workflows.
It does not provide payload deployment or persistence features.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import mimetypes
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlparse


REPO_ROOT = Path(__file__).resolve().parent.parent
STATIC_ROOT = REPO_ROOT / "gui" / "static"
DROPPER_ROOT = REPO_ROOT / "gui" / "dropzone" / "inbox"
ASM_STAGE_ROOT = REPO_ROOT / "gui" / "dropzone" / "asm-staged"
SCENARIO_MATRIX = REPO_ROOT / "Scenario-Matrix.yaml"
EVIDENCE_ROOT = REPO_ROOT / "output" / "evidence"
RUNNER_BIN = REPO_ROOT / "build" / "bootkitstudio"

ALLOWED_MODES = {"baseline", "controlled-drift", "recovery-validation"}
ALLOWED_DROP_EXTENSIONS = {".json", ".yaml", ".yml", ".txt", ".md"}
ALLOWED_ASM_STAGE_EXTENSIONS = {".asm", ".s", ".o", ".bin", ".elf"}
MAX_DROP_BYTES = 2 * 1024 * 1024
MAX_ASM_STAGE_BYTES = 4 * 1024 * 1024


@dataclass
class Scenario:
    scenario_id: str
    platform: str


def sanitize_filename(name: str) -> str:
    name = Path(name).name
    sanitized = re.sub(r"[^a-zA-Z0-9._-]", "_", name)
    return sanitized[:120] or "dropped_artifact.txt"


def parse_scenarios(path: Path) -> list[Scenario]:
    scenarios: list[Scenario] = []
    if not path.exists():
        return scenarios

    current_id = ""
    current_platform = ""
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        if line.startswith("- id:"):
            if current_id:
                scenarios.append(Scenario(current_id, current_platform or "unknown"))
            current_id = line.split(":", 1)[1].strip().strip('"').strip("'")
            current_platform = ""
            continue

        if line.startswith("platform:") and current_id and not current_platform:
            current_platform = line.split(":", 1)[1].strip().strip('"').strip("'")

    if current_id:
        scenarios.append(Scenario(current_id, current_platform or "unknown"))

    return scenarios


def list_evidence() -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    if not EVIDENCE_ROOT.exists():
        return items

    for file_path in EVIDENCE_ROOT.rglob("*.json"):
        if not file_path.is_file():
            continue
        stat = file_path.stat()
        items.append(
            {
                "name": str(file_path.relative_to(EVIDENCE_ROOT)),
                "size": stat.st_size,
                "modified": datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc).isoformat(),
            }
        )

    items.sort(key=lambda x: x["modified"], reverse=True)
    return items[:200]


def read_json_body(handler: BaseHTTPRequestHandler) -> dict[str, Any]:
    content_length = int(handler.headers.get("Content-Length", "0"))
    if content_length <= 0:
        raise ValueError("Request body is required")
    body = handler.rfile.read(content_length)
    return json.loads(body.decode("utf-8"))


def sha256_bytes(data: bytes) -> str:
    digest = hashlib.sha256()
    digest.update(data)
    return digest.hexdigest()


def list_asm_staged() -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    if not ASM_STAGE_ROOT.exists():
        return items

    for file_path in ASM_STAGE_ROOT.glob("*"):
        if not file_path.is_file():
            continue
        if file_path.name.endswith(".note.txt"):
            continue
        blob = file_path.read_bytes()
        stat = file_path.stat()
        items.append(
            {
                "name": file_path.name,
                "size": stat.st_size,
                "sha256": sha256_bytes(blob),
                "modified": datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc).isoformat(),
            }
        )

    items.sort(key=lambda x: x["modified"], reverse=True)
    return items[:200]


class BootKitStudioHandler(BaseHTTPRequestHandler):
    server_version = "BootKitStudioGUI/1.0"

    def log_message(self, format: str, *args: Any) -> None:  # noqa: A003
        sys.stderr.write("%s - - [%s] %s\n" % (self.client_address[0], self.log_date_time_string(), format % args))

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)

        if parsed.path == "/api/health":
            self._send_json({"status": "ok", "repo_root": str(REPO_ROOT)})
            return

        if parsed.path == "/api/scenarios":
            scenarios = [s.__dict__ for s in parse_scenarios(SCENARIO_MATRIX)]
            self._send_json({"scenarios": scenarios})
            return

        if parsed.path == "/api/evidence":
            query = parse_qs(parsed.query)
            file_name = query.get("file", [""])[0]
            if file_name:
                self._send_evidence_file(file_name)
                return
            self._send_json({"items": list_evidence()})
            return

        if parsed.path == "/api/asm-staged":
            self._send_json({"items": list_asm_staged()})
            return

        if parsed.path == "/" or parsed.path == "":
            self._send_static("index.html")
            return

        normalized = parsed.path.lstrip("/")
        if normalized in {"app.js", "styles.css"}:
            self._send_static(normalized)
            return

        self.send_error(HTTPStatus.NOT_FOUND, "Not found")

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)

        if parsed.path == "/api/run":
            self._handle_run()
            return

        if parsed.path == "/api/drop":
            self._handle_drop()
            return

        if parsed.path == "/api/asm-stage":
            self._handle_asm_stage()
            return

        self.send_error(HTTPStatus.NOT_FOUND, "Not found")

    def _handle_run(self) -> None:
        if not RUNNER_BIN.exists():
            self._send_json(
                {
                    "error": "Runner binary not found. Build first with: cmake -S . -B build && cmake --build build",
                },
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        try:
            body = read_json_body(self)
            mode = str(body.get("mode", "baseline"))
            scenario = str(body.get("scenario", "all"))
        except Exception as exc:  # noqa: BLE001
            self._send_json({"error": f"Invalid request body: {exc}"}, status=HTTPStatus.BAD_REQUEST)
            return

        if mode not in ALLOWED_MODES:
            self._send_json({"error": f"Unsupported mode: {mode}"}, status=HTTPStatus.BAD_REQUEST)
            return

        allowed_scenarios = {"all"} | {s.scenario_id for s in parse_scenarios(SCENARIO_MATRIX)}
        if scenario not in allowed_scenarios:
            self._send_json({"error": f"Unsupported scenario: {scenario}"}, status=HTTPStatus.BAD_REQUEST)
            return

        env = os.environ.copy()
        env["BKS_LAB_MODE"] = "1"

        command = [
            str(RUNNER_BIN),
            "--mode",
            mode,
            "--scenario",
            scenario,
            "--repo-root",
            str(REPO_ROOT),
        ]

        try:
            completed = subprocess.run(
                command,
                cwd=str(REPO_ROOT),
                env=env,
                capture_output=True,
                text=True,
                timeout=300,
                check=False,
            )
        except subprocess.TimeoutExpired:
            self._send_json({"error": "Runner timed out"}, status=HTTPStatus.REQUEST_TIMEOUT)
            return

        self._send_json(
            {
                "command": command,
                "exit_code": completed.returncode,
                "stdout": completed.stdout,
                "stderr": completed.stderr,
                "evidence": list_evidence()[:20],
            },
            status=HTTPStatus.OK if completed.returncode == 0 else HTTPStatus.BAD_REQUEST,
        )

    def _handle_drop(self) -> None:
        try:
            body = read_json_body(self)
        except Exception as exc:  # noqa: BLE001
            self._send_json({"error": f"Invalid request body: {exc}"}, status=HTTPStatus.BAD_REQUEST)
            return

        filename = sanitize_filename(str(body.get("filename", "")))
        content = body.get("content", "")

        if not filename:
            self._send_json({"error": "filename is required"}, status=HTTPStatus.BAD_REQUEST)
            return

        ext = Path(filename).suffix.lower()
        if ext not in ALLOWED_DROP_EXTENSIONS:
            self._send_json(
                {
                    "error": f"Unsupported file type: {ext}. Allowed: {sorted(ALLOWED_DROP_EXTENSIONS)}",
                },
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        if not isinstance(content, str):
            self._send_json({"error": "content must be a UTF-8 string"}, status=HTTPStatus.BAD_REQUEST)
            return

        encoded = content.encode("utf-8")
        if len(encoded) > MAX_DROP_BYTES:
            self._send_json(
                {"error": f"Dropped file exceeds max size of {MAX_DROP_BYTES} bytes"},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        if ext == ".json":
            try:
                json.loads(content)
            except json.JSONDecodeError as exc:
                self._send_json({"error": f"Invalid JSON: {exc}"}, status=HTTPStatus.BAD_REQUEST)
                return

        timestamp = datetime.now(tz=timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        output_name = f"{timestamp}.{filename}"
        output_path = DROPPER_ROOT / output_name
        DROPPER_ROOT.mkdir(parents=True, exist_ok=True)
        output_path.write_text(content, encoding="utf-8")

        self._send_json(
            {
                "status": "stored",
                "stored_as": str(output_path.relative_to(REPO_ROOT)),
                "bytes": len(encoded),
                "note": "BootKit Dropper accepts defensive lab artifacts only.",
            }
        )

    def _handle_asm_stage(self) -> None:
        try:
            body = read_json_body(self)
        except Exception as exc:  # noqa: BLE001
            self._send_json({"error": f"Invalid request body: {exc}"}, status=HTTPStatus.BAD_REQUEST)
            return

        filename = sanitize_filename(str(body.get("filename", "")))
        content_b64 = body.get("content_b64", "")
        note = str(body.get("note", ""))

        if not filename:
            self._send_json({"error": "filename is required"}, status=HTTPStatus.BAD_REQUEST)
            return

        ext = Path(filename).suffix.lower()
        if ext not in ALLOWED_ASM_STAGE_EXTENSIONS:
            self._send_json(
                {
                    "error": (
                        f"Unsupported asm module type: {ext}. "
                        f"Allowed: {sorted(ALLOWED_ASM_STAGE_EXTENSIONS)}"
                    ),
                },
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        if not isinstance(content_b64, str) or not content_b64:
            self._send_json(
                {"error": "content_b64 is required and must be base64 text"},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        try:
            payload = base64.b64decode(content_b64, validate=True)
        except Exception as exc:  # noqa: BLE001
            self._send_json({"error": f"Invalid base64 payload: {exc}"}, status=HTTPStatus.BAD_REQUEST)
            return

        if len(payload) > MAX_ASM_STAGE_BYTES:
            self._send_json(
                {"error": f"ASM module exceeds max size of {MAX_ASM_STAGE_BYTES} bytes"},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        ASM_STAGE_ROOT.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.now(tz=timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        output_name = f"{timestamp}.{filename}"
        output_path = ASM_STAGE_ROOT / output_name
        output_path.write_bytes(payload)

        if note:
            note_path = ASM_STAGE_ROOT / f"{output_name}.note.txt"
            note_path.write_text(note[:5000], encoding="utf-8")

        self._send_json(
            {
                "status": "staged",
                "stored_as": str(output_path.relative_to(REPO_ROOT)),
                "bytes": len(payload),
                "sha256": sha256_bytes(payload),
                "note": "ASM module staged for defensive simulation only; it is not executed or deployed.",
            }
        )

    def _send_evidence_file(self, file_name: str) -> None:
        candidate = (EVIDENCE_ROOT / file_name).resolve()
        evidence_root = EVIDENCE_ROOT.resolve()

        if evidence_root not in candidate.parents and candidate != evidence_root:
            self._send_json({"error": "Invalid evidence path"}, status=HTTPStatus.BAD_REQUEST)
            return

        if not candidate.exists() or not candidate.is_file():
            self._send_json({"error": "Evidence file not found"}, status=HTTPStatus.NOT_FOUND)
            return

        if candidate.suffix.lower() != ".json":
            self._send_json({"error": "Evidence file must be .json"}, status=HTTPStatus.BAD_REQUEST)
            return

        try:
            content = json.loads(candidate.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            self._send_json({"error": f"Evidence JSON parse error: {exc}"}, status=HTTPStatus.BAD_REQUEST)
            return

        self._send_json({"file": file_name, "content": content})

    def _send_static(self, file_name: str) -> None:
        target = (STATIC_ROOT / file_name).resolve()
        static_root = STATIC_ROOT.resolve()

        if static_root not in target.parents and target != static_root:
            self.send_error(HTTPStatus.BAD_REQUEST, "Invalid static path")
            return

        if not target.exists() or not target.is_file():
            self.send_error(HTTPStatus.NOT_FOUND, "Static file not found")
            return

        mime, _ = mimetypes.guess_type(str(target))
        content = target.read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", mime or "application/octet-stream")
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def _send_json(self, payload: dict[str, Any], status: HTTPStatus = HTTPStatus.OK) -> None:
        data = json.dumps(payload, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def main() -> int:
    parser = argparse.ArgumentParser(description="BootKitStudio defensive GUI server")
    parser.add_argument("--host", default="127.0.0.1", help="Host interface")
    parser.add_argument("--port", default=8088, type=int, help="Port")
    args = parser.parse_args()

    httpd = ThreadingHTTPServer((args.host, args.port), BootKitStudioHandler)
    print(f"BootKitStudio GUI running on http://{args.host}:{args.port}")
    print("BootKit Dropper is defensive-only artifact intake.")
    print("ASM Binary Stager is defensive-only module staging.")
    print("Press Ctrl+C to stop.")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down GUI server.")
    finally:
        httpd.server_close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
