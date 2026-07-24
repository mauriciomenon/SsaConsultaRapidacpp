#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: ./scripts/smoke-macos-no-clean.sh [--open]

Run macOS smoke flow using existing build output when available (no make clean).
Use --open to run the same build/test flow and open the visible app window.
Defaults:
  - Database: <repo_root>/data/ssas.db
  - Preset: dev
  - Runtime dir: <repo_root>/build/runtime/macos
  - Screenshot: <repo_root>/build/runtime/macos/main.png
  - No env var is required or read by this smoke wrapper for DB path or launch mode.
  - SSA_CPP_* env vars belong to scripts/run-macos.sh and do not change this smoke flow.

If you need parameterized flow use:
  ./scripts/lazy_scripts/macos-build-test-smoke-run.sh <path-to-ssas.db> [options]
USAGE
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
repo_root="${script_dir}/.."
repo_root="$(cd "${repo_root}" && pwd)"
# shellcheck disable=SC1091
# shellcheck source=smoke-macos-core.sh
source "${script_dir}/smoke-macos-core.sh"

db_path="${repo_root}/data/ssas.db"
preset="dev"
runtime_dir="$(macos_default_runtime_dir "${repo_root}")"
runtime_config_dir="$(macos_default_config_dir "${runtime_dir}")"
screenshot="$(macos_default_screenshot_path "${runtime_dir}")"
config_src="${repo_root}/config/ssa_cpp_preferences.json.example"
project_root="${repo_root}"

run_macos_smoke_core \
  "${project_root}" \
  "${preset}" \
  "${db_path}" \
  "${runtime_dir}" \
  "${runtime_config_dir}" \
  "${screenshot}" \
  "false" \
  "${config_src}" \
  "${launch_mode}" \
  "true"
