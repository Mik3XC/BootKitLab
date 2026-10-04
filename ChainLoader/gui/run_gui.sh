#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ ! -x "$ROOT/build/bootkitstudio" ]]; then
  echo "bootkitstudio binary not found; building..."
  cmake -S "$ROOT" -B "$ROOT/build"
  cmake --build "$ROOT/build" -j
fi

exec python3 "$ROOT/gui/server.py" --host 127.0.0.1 --port 8088
