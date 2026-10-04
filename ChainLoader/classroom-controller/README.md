# ChainLoader Classroom Controller

This folder contains a safe teaching controller in two parts:

- `rust/`: a dependency-free Rust CLI that reads the existing ChainLoader
  scenario and manifest files, validates the non-operational contract, and can
  optionally run only the safe `baseline` scenario.
- `jsx/`: a React JSX component that presents the lesson flow, mock installer
  phases, blocked actions, ACPI clarification, marker contracts, and evidence
  preview.

## Safety Boundary

The controller is not an installer. It does not write boot sectors, firmware,
ACPI/AML, `bootmgr`, `winload`, BCD, ESP contents, drivers, or persistent loader
state. The only optional execution path is a BootKitStudio `baseline` run with
`BKS_LAB_MODE=1`.

## Rust Controller

Build:

```bash
cargo build --manifest-path classroom-controller/rust/Cargo.toml
```

Print the classroom lesson:

```bash
cargo run --manifest-path classroom-controller/rust/Cargo.toml -- \
  --repo-root . \
  --scenario classroom-acpi-annotation
```

Emit JSON for the JSX component:

```bash
cargo run --manifest-path classroom-controller/rust/Cargo.toml -- \
  --repo-root . \
  --scenario classroom-acpi-annotation \
  --json
```

Run the safe baseline scenario after building BootKitStudio:

```bash
cmake -S . -B build-local
cmake --build build-local
cargo run --manifest-path classroom-controller/rust/Cargo.toml -- \
  --repo-root . \
  --scenario classroom-acpi-annotation \
  --run
```

The Rust controller intentionally has no `controlled-drift` or deploy mode.

## JSX Component

Import the component into any React app:

```jsx
import ChainloaderClassroomController from "./ChainloaderClassroomController.jsx";
import "./styles.css";

export default function App() {
  return <ChainloaderClassroomController />;
}
```

To use live controller JSON, pass the parsed JSON as `controllerModel`:

```jsx
<ChainloaderClassroomController controllerModel={controllerModel} />
```

## Static Hosted Classroom

The static host requires no package install. Serve the repository root and open
the classroom page:

```bash
python3 -m http.server 8091
```

Open:

```text
http://127.0.0.1:8091/classroom-controller/site/
```

The page can load evidence JSON from `output/evidence/...` while preserving the
same non-operational boundary.

## Teaching Script

1. Start with the safety contract and blocked actions.
2. Walk the class through `would_stage`, `would_hook`, `would_chain`, and
   `would_restore`.
3. Show that `sim.acpi.annotation.marker` returns a marker string only.
4. Separate ACPI firmware tables, Windows `acpi.sys`, and Windows SSDT.
5. Map every concept to a defensive detection or rollback evidence item.
6. Run the safe baseline scenario and inspect the JSON evidence.
