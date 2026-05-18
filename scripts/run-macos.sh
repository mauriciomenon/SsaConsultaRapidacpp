#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/run-macos.sh

Run built macOS app with defaults loaded from env vars.

Defaults:
  SSA_CPP_DB_PATH: Optional explicit path to ssas.db. Default: <repo>/data/ssas.db.
  SSA_CPP_PRESET: CMake preset used to locate executable (default: dev).
  SSA_CPP_PROJECT_ROOT: Project root for executable and default db path.
  SSA_CPP_CONFIG_DIR: Optional app --config-dir argument.
  SSA_CPP_SCREENSHOT: Optional app --screenshot argument.

For explicit argument-driven execution use:
  ./scripts/lazy_scripts/run-macos.sh <path-to-ssas.db> [--config-dir <dir>] [--screenshot <file>]
EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi
if [[ $# -ne 0 ]]; then
  echo "This script is no-argument by default. Use --help for usage." >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
repo_root="${SSA_CPP_PROJECT_ROOT:-${repo_root}}"
preset="${SSA_CPP_PRESET:-dev}"
# shellcheck disable=SC1091
# shellcheck source=smoke-macos-core.sh
source "${script_dir}/smoke-macos-core.sh"

resolve_run_db_path() {
  if [[ -n "${SSA_CPP_DB_PATH-}" ]]; then
    printf '%s\n' "${SSA_CPP_DB_PATH}"
    return 0
  fi

  resolve_project_default_db_path "${repo_root}"
}

if ! db_path="$(resolve_run_db_path)"; then
  echo "Database file not found." >&2
  echo "Set SSA_CPP_DB_PATH or place ssas.db in the default location:" >&2
  print_project_default_db_not_found \
    "${repo_root}" \
    "Or pass an explicit DB path with SSA_CPP_DB_PATH."
  exit 1
fi

if [[ ! -f "${db_path}" ]]; then
  echo "Database file not found: ${db_path}" >&2
  if [[ -n "${SSA_CPP_DB_PATH-}" ]]; then
    echo "SSA_CPP_DB_PATH points to a missing file." >&2
  fi
  exit 1
fi

launch_mode="open"
screenshot=""
if [[ -n "${SSA_CPP_SCREENSHOT-}" ]]; then
  launch_mode="screenshot"
  screenshot="${SSA_CPP_SCREENSHOT}"
fi

run_macos_app "${repo_root}" "${preset}" "${db_path}" "${SSA_CPP_CONFIG_DIR-}" "${screenshot}" \
  "${launch_mode}"
