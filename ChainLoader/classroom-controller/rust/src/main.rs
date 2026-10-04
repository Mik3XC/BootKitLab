use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const DEFAULT_SCENARIO: &str = "classroom-acpi-annotation";
const DEFAULT_OUTPUT: &str = "output/evidence/classroom-rust-controller";

#[derive(Debug, Clone)]
struct Args {
    repo_root: PathBuf,
    scenario_id: String,
    runner: Option<PathBuf>,
    output_dir: PathBuf,
    json: bool,
    run: bool,
}

#[derive(Debug, Clone)]
struct Scenario {
    id: String,
    platform: String,
    safe_action: String,
    expected_detection: Vec<String>,
    expected_hardening: Vec<String>,
    stage_simulators: Vec<String>,
}

#[derive(Debug, Clone)]
struct StageManifest {
    module_id: String,
    boot_stage: String,
    marker_contract: String,
    input_contract: String,
}

#[derive(Debug, Clone)]
struct MockInstaller {
    non_operational: bool,
    phases: Vec<String>,
    prohibited_actions: Vec<String>,
    evidence_contract: String,
}

#[derive(Debug, Clone)]
struct ControllerModel {
    scenario: Scenario,
    markers: Vec<StageManifest>,
    mock_installer: MockInstaller,
    runner_status: Option<String>,
}

fn main() {
    if let Err(error) = run_controller() {
        eprintln!("ERROR: {error}");
        std::process::exit(1);
    }
}

fn run_controller() -> Result<(), String> {
    let args = parse_args()?;
    let scenarios = parse_scenarios(&args.repo_root.join("Scenario-Matrix.yaml"))?;
    let manifests = parse_stage_manifests(
        &args
            .repo_root
            .join("bootkitstudio/manifests/Stage-Simulator-Manifest.yaml"),
    )?;
    let mock_installer = parse_mock_installer(
        &args
            .repo_root
            .join("bootkitstudio/manifests/Mock-Installer-Manifest.yaml"),
    )?;

    validate_mock_installer(&mock_installer)?;

    let scenario = scenarios
        .into_iter()
        .find(|candidate| candidate.id == args.scenario_id)
        .ok_or_else(|| format!("scenario not found: {}", args.scenario_id))?;

    let mut markers = Vec::new();
    for module_id in &scenario.stage_simulators {
        let manifest = manifests
            .iter()
            .find(|candidate| candidate.module_id == *module_id)
            .ok_or_else(|| format!("stage simulator not found in manifest: {module_id}"))?;
        markers.push(manifest.clone());
    }

    let runner_status = if args.run {
        Some(run_safe_baseline(&args)?)
    } else {
        None
    };

    let model = ControllerModel {
        scenario,
        markers,
        mock_installer,
        runner_status,
    };

    if args.json {
        println!("{}", model_to_json(&model));
    } else {
        print_lesson(&model);
    }

    Ok(())
}

fn parse_args() -> Result<Args, String> {
    let mut repo_root = env::current_dir().map_err(|error| error.to_string())?;
    let mut scenario_id = DEFAULT_SCENARIO.to_string();
    let mut runner = None;
    let mut output_dir = PathBuf::from(DEFAULT_OUTPUT);
    let mut json = false;
    let mut run = false;

    let mut iter = env::args().skip(1);
    while let Some(arg) = iter.next() {
        match arg.as_str() {
            "--repo-root" => {
                repo_root = PathBuf::from(require_value("--repo-root", iter.next())?);
            }
            "--scenario" => {
                scenario_id = require_value("--scenario", iter.next())?;
            }
            "--runner" => {
                runner = Some(PathBuf::from(require_value("--runner", iter.next())?));
            }
            "--output" => {
                output_dir = PathBuf::from(require_value("--output", iter.next())?);
            }
            "--json" => json = true,
            "--run" => run = true,
            "--help" | "-h" => {
                print_usage();
                std::process::exit(0);
            }
            other => return Err(format!("unknown flag: {other}")),
        }
    }

    Ok(Args {
        repo_root,
        scenario_id,
        runner,
        output_dir,
        json,
        run,
    })
}

fn require_value(flag: &str, value: Option<String>) -> Result<String, String> {
    value.ok_or_else(|| format!("missing value for {flag}"))
}

fn print_usage() {
    println!("ChainLoader classroom controller");
    println!("Usage:");
    println!("  cargo run --manifest-path classroom-controller/rust/Cargo.toml -- [options]");
    println!("Options:");
    println!("  --repo-root <path>     Repository root. Defaults to current directory.");
    println!("  --scenario <id>        Scenario id. Defaults to classroom-acpi-annotation.");
    println!("  --runner <path>        BootKitStudio binary to run when --run is set.");
    println!("  --output <path>        Evidence output directory for --run.");
    println!("  --json                 Emit JSON for the JSX controller.");
    println!("  --run                  Run only the safe baseline scenario through BootKitStudio.");
}

fn parse_scenarios(path: &Path) -> Result<Vec<Scenario>, String> {
    let content =
        fs::read_to_string(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let mut scenarios = Vec::new();
    let mut current: Option<Scenario> = None;

    for raw_line in content.lines() {
        let line = clean_line(raw_line);
        if line.is_empty() || line == "scenarios:" {
            continue;
        }

        if let Some(rest) = line.strip_prefix("- ") {
            if let Some(scenario) = current.take() {
                scenarios.push(scenario);
            }
            let mut scenario = Scenario {
                id: String::new(),
                platform: String::new(),
                safe_action: String::new(),
                expected_detection: Vec::new(),
                expected_hardening: Vec::new(),
                stage_simulators: Vec::new(),
            };
            assign_scenario(&mut scenario, rest);
            current = Some(scenario);
            continue;
        }

        if let Some(scenario) = current.as_mut() {
            assign_scenario(scenario, &line);
        }
    }

    if let Some(scenario) = current {
        scenarios.push(scenario);
    }

    Ok(scenarios)
}

fn assign_scenario(scenario: &mut Scenario, line: &str) {
    if let Some((key, value)) = split_key_value(line) {
        match key {
            "id" => scenario.id = unquote(value),
            "platform" => scenario.platform = unquote(value),
            "safe_action" => scenario.safe_action = unquote(value),
            "expected_detection" => scenario.expected_detection = parse_inline_list(value),
            "expected_hardening" => scenario.expected_hardening = parse_inline_list(value),
            "stage_simulators" => scenario.stage_simulators = parse_inline_list(value),
            _ => {}
        }
    }
}

fn parse_stage_manifests(path: &Path) -> Result<Vec<StageManifest>, String> {
    let content =
        fs::read_to_string(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let mut manifests = Vec::new();
    let mut current: Option<StageManifest> = None;

    for raw_line in content.lines() {
        let line = clean_line(raw_line);
        if line.is_empty() || line == "manifests:" {
            continue;
        }

        if let Some(rest) = line.strip_prefix("- ") {
            if let Some(manifest) = current.take() {
                manifests.push(manifest);
            }
            let mut manifest = StageManifest {
                module_id: String::new(),
                boot_stage: String::new(),
                marker_contract: String::new(),
                input_contract: String::new(),
            };
            assign_manifest(&mut manifest, rest);
            current = Some(manifest);
            continue;
        }

        if let Some(manifest) = current.as_mut() {
            assign_manifest(manifest, &line);
        }
    }

    if let Some(manifest) = current {
        manifests.push(manifest);
    }

    Ok(manifests)
}

fn assign_manifest(manifest: &mut StageManifest, line: &str) {
    if let Some((key, value)) = split_key_value(line) {
        match key {
            "module_id" => manifest.module_id = unquote(value),
            "boot_stage" => manifest.boot_stage = unquote(value),
            "marker_contract" => manifest.marker_contract = unquote(value),
            "input_contract" => manifest.input_contract = unquote(value),
            _ => {}
        }
    }
}

fn parse_mock_installer(path: &Path) -> Result<MockInstaller, String> {
    let content =
        fs::read_to_string(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let mut mock = MockInstaller {
        non_operational: true,
        phases: Vec::new(),
        prohibited_actions: Vec::new(),
        evidence_contract: String::new(),
    };

    for raw_line in content.lines() {
        let line = clean_line(raw_line);
        if line.is_empty() || line == "mock_installer:" {
            continue;
        }
        if let Some((key, value)) = split_key_value(&line) {
            match key {
                "non_operational" => mock.non_operational = value == "true",
                "phases" => mock.phases = parse_inline_list(value),
                "prohibited_actions" => mock.prohibited_actions = parse_inline_list(value),
                "evidence_contract" => mock.evidence_contract = unquote(value),
                _ => {}
            }
        }
    }

    Ok(mock)
}

fn validate_mock_installer(mock: &MockInstaller) -> Result<(), String> {
    if !mock.non_operational {
        return Err("mock installer must be non_operational=true".to_string());
    }

    for required in ["would_stage", "would_hook", "would_chain", "would_restore"] {
        if !mock.phases.iter().any(|phase| phase == required) {
            return Err(format!("mock installer missing phase: {required}"));
        }
    }

    for required in [
        "disk-write",
        "firmware-write",
        "real-bootmgr-patch",
        "real-winload-patch",
        "persistent-installer",
        "live-acpi-aml-modification",
    ] {
        if !mock
            .prohibited_actions
            .iter()
            .any(|action| action == required)
        {
            return Err(format!(
                "mock installer missing prohibited action: {required}"
            ));
        }
    }

    Ok(())
}

fn run_safe_baseline(args: &Args) -> Result<String, String> {
    let runner = args
        .runner
        .clone()
        .unwrap_or_else(|| default_runner_path(&args.repo_root));
    if !runner.exists() {
        return Err(format!(
            "runner not found: {}. Build with `cmake -S . -B build-local && cmake --build build-local` or pass --runner.",
            runner.display()
        ));
    }

    let output = Command::new(&runner)
        .current_dir(&args.repo_root)
        .env("BKS_LAB_MODE", "1")
        .arg("--mode")
        .arg("baseline")
        .arg("--scenario")
        .arg(&args.scenario_id)
        .arg("--output")
        .arg(&args.output_dir)
        .output()
        .map_err(|error| format!("failed to run {}: {error}", runner.display()))?;

    if !output.status.success() {
        return Err(format!(
            "BootKitStudio baseline failed:\n{}{}",
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
}

fn default_runner_path(repo_root: &Path) -> PathBuf {
    let build_local = repo_root.join("build-local/bootkitstudio");
    if build_local.exists() {
        build_local
    } else {
        repo_root.join("build/bootkitstudio")
    }
}

fn print_lesson(model: &ControllerModel) {
    println!("ChainLoader Classroom Controller");
    println!(
        "Scenario: {} ({})",
        model.scenario.id, model.scenario.platform
    );
    println!();
    println!("What is happening:");
    println!(
        "  1. The controller reads the scenario, stage manifest, and mock installer manifest."
    );
    println!("  2. It validates the non-operational safety contract.");
    println!("  3. It presents marker-only stages for discussion.");
    println!("  4. Optional --run executes only the safe BootKitStudio baseline mode.");
    println!();
    println!("Safety contract:");
    for action in &model.mock_installer.prohibited_actions {
        println!("  blocked: {action}");
    }
    println!();
    println!("Mock installer phases:");
    for phase in &model.mock_installer.phases {
        println!("  {phase}");
    }
    println!();
    println!("Stage markers:");
    for marker in &model.markers {
        println!(
            "  {} -> {} -> {}",
            marker.module_id, marker.boot_stage, marker.marker_contract
        );
    }
    println!();
    println!("Detection prompts:");
    for detection in &model.scenario.expected_detection {
        println!("  would detect: {detection}");
    }
    println!();
    println!("Teaching boundary:");
    println!("  ACPI firmware SSDT is not the Windows System Service Dispatch Table.");
    println!("  BIOS disk-read and hook mechanics stay as labeled stubs.");
    println!("  The class artifact emits evidence; it does not deploy code.");

    if let Some(status) = &model.runner_status {
        println!();
        println!("Runner status:");
        println!("{status}");
    }
}

fn model_to_json(model: &ControllerModel) -> String {
    let markers = model
        .markers
        .iter()
        .map(|marker| {
            format!(
                "{{\"moduleId\":\"{}\",\"bootStage\":\"{}\",\"markerContract\":\"{}\",\"inputContract\":\"{}\"}}",
                escape_json(&marker.module_id),
                escape_json(&marker.boot_stage),
                escape_json(&marker.marker_contract),
                escape_json(&marker.input_contract)
            )
        })
        .collect::<Vec<_>>()
        .join(",");

    format!(
        "{{\"scenario\":{{\"id\":\"{}\",\"platform\":\"{}\",\"safeAction\":\"{}\",\"expectedDetection\":{},\"expectedHardening\":{}}},\"mockInstaller\":{{\"nonOperational\":{},\"phases\":{},\"prohibitedActions\":{},\"evidenceContract\":\"{}\"}},\"markers\":[{}],\"runnerStatus\":{}}}",
        escape_json(&model.scenario.id),
        escape_json(&model.scenario.platform),
        escape_json(&model.scenario.safe_action),
        string_vec_to_json(&model.scenario.expected_detection),
        string_vec_to_json(&model.scenario.expected_hardening),
        model.mock_installer.non_operational,
        string_vec_to_json(&model.mock_installer.phases),
        string_vec_to_json(&model.mock_installer.prohibited_actions),
        escape_json(&model.mock_installer.evidence_contract),
        markers,
        match &model.runner_status {
            Some(status) => format!("\"{}\"", escape_json(status)),
            None => "null".to_string(),
        }
    )
}

fn clean_line(line: &str) -> String {
    line.split('#').next().unwrap_or("").trim().to_string()
}

fn split_key_value(line: &str) -> Option<(&str, &str)> {
    let (key, value) = line.split_once(':')?;
    Some((key.trim(), value.trim()))
}

fn parse_inline_list(value: &str) -> Vec<String> {
    value
        .trim()
        .trim_start_matches('[')
        .trim_end_matches(']')
        .split(',')
        .map(|token| unquote(token.trim()))
        .filter(|token| !token.is_empty())
        .collect()
}

fn unquote(value: &str) -> String {
    let trimmed = value.trim();
    if trimmed.len() >= 2 {
        let bytes = trimmed.as_bytes();
        if (bytes[0] == b'"' && bytes[trimmed.len() - 1] == b'"')
            || (bytes[0] == b'\'' && bytes[trimmed.len() - 1] == b'\'')
        {
            return trimmed[1..trimmed.len() - 1].to_string();
        }
    }
    trimmed.to_string()
}

fn string_vec_to_json(values: &[String]) -> String {
    let body = values
        .iter()
        .map(|value| format!("\"{}\"", escape_json(value)))
        .collect::<Vec<_>>()
        .join(",");
    format!("[{body}]")
}

fn escape_json(value: &str) -> String {
    value
        .chars()
        .flat_map(|c| match c {
            '\\' => "\\\\".chars().collect::<Vec<_>>(),
            '"' => "\\\"".chars().collect::<Vec<_>>(),
            '\n' => "\\n".chars().collect::<Vec<_>>(),
            '\r' => "\\r".chars().collect::<Vec<_>>(),
            '\t' => "\\t".chars().collect::<Vec<_>>(),
            other => vec![other],
        })
        .collect()
}
