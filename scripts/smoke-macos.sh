#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF_USAGE'
Usage: ./scripts/smoke-macos.sh [--open]

Run default macOS smoke flow with clean reconfigure:
- equivalent to make clean and full rebuild for this flow.
- builds, tests, and launches app with offscreen screenshot.

Use --open to run the same build/test flow and open the visible app window.

Defaults:
  - Database: <repo_root>/data/ssas.db
  - Preset: dev
  - Runtime dir: <repo_root>/build/runtime/macos
  - Screenshot: ${runtime_dir}/main.png
  - No env var is required or read by this smoke wrapper for DB path or launch mode.
  - SSA_CPP_* env vars belong to scripts/run-macos.sh and do not change this smoke flow.

If you need parameterized flow, use:
  ./scripts/lazy_scripts/macos-build-clean-test-smoke-run.sh
EOF_USAGE
}

launch_mode="screenshot"
while [[ $# -gt 0 ]]; do
  case "${1}" in
    --help|-h)
      usage
      exit 0
      ;;
    --open)
      launch_mode="open"
      shift
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      usage >&2
      exit 1
      ;;
  esac
done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
# shellcheck disable=SC1091
# shellcheck source=smoke-macos-core.sh
source "${script_dir}/smoke-macos-core.sh"

db_path="${repo_root}/data/ssas.db"
preset="dev"
runtime_dir="$(macos_default_runtime_dir "${repo_root}")"
runtime_config_dir="$(macos_default_config_dir "${runtime_dir}")"
screenshot="$(macos_default_screenshot_path "${runtime_dir}")"
project_root="${repo_root}"
config_src="${repo_root}/config/ssa_cpp_preferences.json.example"
clean_requested="true"

run_macos_smoke_core \
  "${project_root}" \
  "${preset}" \
  "${db_path}" \
  "${runtime_dir}" \
  "${runtime_config_dir}" \
  "${screenshot}" \
  "${clean_requested}" \
  "${config_src}" \
  "${launch_mode}" \
  "true"
