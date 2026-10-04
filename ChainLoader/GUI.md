# BootKitStudio GUI + BootKit Dropper (Defensive)

## Safety
The GUI is defensive-only.
- **BootKit Dropper** is an artifact inbox for lab files (`.json/.yaml/.txt/.md`).
- **ASM Binary Stager** stores `.asm/.s/.o/.bin/.elf` modules for simulation inventory only.
- Neither feature deploys payloads or writes to boot sectors/firmware.

## Start
```bash
cd /Users/premise/Documents/github/ChainLoader
./gui/run_gui.sh
```

Open:
- [http://127.0.0.1:8088](http://127.0.0.1:8088)

## Features
- Scenario Runner
  - Runs existing modes: `baseline`, `controlled-drift`, `recovery-validation`
  - Enforces `BKS_LAB_MODE=1` automatically on API run
- BootKit Dropper
  - Uploads defensive artifacts into `gui/dropzone/inbox/`
  - Validates extension and size (max 2MB)
  - JSON files are syntax-checked
- ASM Binary Stager
  - Uploads ASM-related modules into `gui/dropzone/asm-staged/`
  - Accepts base64-uploaded `.asm/.s/.o/.bin/.elf` files
  - Returns SHA-256 and inventory listing
  - Does not execute or deploy staged binaries
- Evidence Viewer
  - Lists JSON outputs from `output/evidence/`
  - Opens and pretty-prints evidence bundles

## API Endpoints
- `GET /api/health`
- `GET /api/scenarios`
- `POST /api/run`
- `GET /api/evidence`
- `GET /api/evidence?file=<relative-path>`
- `POST /api/drop`
- `GET /api/asm-staged`
- `POST /api/asm-stage`
