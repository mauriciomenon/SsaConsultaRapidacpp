#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/lazy_scripts/run-debian.sh <path-to-ssas.db> [--project-root <dir>] [--config-dir <dir>] [--screenshot <file>]

Run Debian app using explicit params.

Positional:
  <path-to-ssas.db>   Source database path.

Options:
  --project-root <dir>  Override project root.
  --config-dir <dir>    Optional app --config-dir argument.
  --screenshot <file>   Optional app --screenshot argument.
  --help                Show this help.
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
db_path="$1"
shift

project_root="${repo_root}"
config_dir=""
screenshot=""

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --project-root)
      require_option_value "${1}" "${2-}"
      project_root="${2}"
      shift 2
      ;;
    --config-dir)
      require_option_value "${1}" "${2-}"
      config_dir="${2}"
      shift 2
      ;;
    --screenshot)
      require_option_value "${1}" "${2-}"
      screenshot="${2}"
      shift 2
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      show_help >&2
      exit 1
      ;;
  esac
done

# shellcheck disable=SC1091
source "${repo_root}/scripts/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root" || exit 1
ssa_native_guard_repo "$project_root" || exit 1
ssa_native_guard_path "$db_path" "$repo_root" || exit 1

if [[ ! -f "${db_path}" ]]; then
  echo "Database file not found: ${db_path}" >&2
  exit 1
fi

executable="${project_root}/build/dev/ssa_consulta_rapida"
if [[ ! -x "${executable}" ]]; then
  echo "Binary not found: ${executable}" >&2
  exit 1
fi
ssa_native_guard_tool "${executable}" || exit 1

args=(--project-root "${project_root}" --db "${db_path}")
if [[ -n "${config_dir}" ]]; then
  args+=(--config-dir "${config_dir}")
fi
if [[ -n "${screenshot}" ]]; then
  args+=(--screenshot "${screenshot}")
fi

"${executable}" "${args[@]}"
