#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/lazy_scripts/macos-build-test-smoke-run.sh <path-to-ssas.db> [options]

Required:
  <path-to-ssas.db>    Absolute or relative path for source SQLite database.

Options:
  --project-root <path>  Application project root. Default is parent of script dir.
  --preset <preset>      CMake preset used by cmake/cmake build/ctest. Default: dev.
  --config-src <path>    Optional custom preferences template to copy into runtime config.
  --screenshot <path>    Output screenshot path. Default: <project-root>/build/runtime/macos/main.png
  --runtime-dir <path>   Parent runtime folder used for copied db and generated config.
  --config-dir <path>    Optional explicit config directory path.
  --clean                Force fresh build configuration using --fresh when available.
  --open                 Open the visible app window after build/test instead of offscreen screenshot.
  --help                 Show this help.
EOF
}

require_option_value() {
  local option="$1"
  local value="${2-}"
  if [[ -z "${value}" || "${value}" == --* ]]; then
    echo "${option} requires a value." >&2
    show_help >&2
    exit 1
  fi
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

if [[ $# -lt 1 ]]; then
  show_help >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
db_path="${1}"
shift
# shellcheck disable=SC1091
# shellcheck source=../smoke-macos-core.sh
source "${script_dir}/../smoke-macos-core.sh"

preset="dev"
project_root="${repo_root}"
runtime_dir=""
screenshot=""
custom_config_src=""
config_dir=""
clean_requested="false"
launch_mode="screenshot"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --project-root)
      require_option_value "${1}" "${2-}"
      project_root="${2}"
      shift 2
      ;;
    --preset)
      require_option_value "${1}" "${2-}"
      preset="${2}"
      shift 2
      ;;
    --config-src)
      require_option_value "${1}" "${2-}"
      custom_config_src="${2}"
      shift 2
      ;;
    --screenshot)
      require_option_value "${1}" "${2-}"
      screenshot="${2}"
      shift 2
      ;;
    --runtime-dir)
      require_option_value "${1}" "${2-}"
      runtime_dir="${2}"
      shift 2
      ;;
    --config-dir)
      require_option_value "${1}" "${2-}"
      config_dir="${2}"
      shift 2
      ;;
    --clean)
      clean_requested="true"
      shift
      ;;
    --open)
      launch_mode="open"
      shift
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      show_help >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "${db_path}" ]]; then
  echo "Database file not found: ${db_path}" >&2
  exit 1
fi

if [[ -z "${runtime_dir}" ]]; then
  runtime_dir="$(macos_default_runtime_dir "${project_root}")"
fi
if [[ -z "${screenshot}" ]]; then
  screenshot="$(macos_default_screenshot_path "${runtime_dir}")"
fi
if [[ -z "${config_dir}" ]]; then
  config_dir="$(macos_default_config_dir "${runtime_dir}")"
fi
run_macos_smoke_core \
  "${project_root}" \
  "${preset}" \
  "${db_path}" \
  "${runtime_dir}" \
  "${config_dir}" \
  "${screenshot}" \
  "${clean_requested}" \
  "${custom_config_src:-${project_root}/config/ssa_cpp_preferences.json.example}" \
  "${launch_mode}"
