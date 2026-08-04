#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/run-debian.sh

Run built Debian app using default project DB resolution.

The database path is resolved in this order:
  1) SSA_DB_PATH
  2) ~/.ssaconsultarapida/data/ssas.db

Defaults:
  Preset: dev
  Project root: repository directory that contains this script.

For explicit argument-driven execution use:
  ./scripts/lazy_scripts/run-debian.sh <path-to-ssas.db> [--project-root <dir>] [--config-dir <dir>] [--screenshot <file>]
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
preset="dev"
# shellcheck disable=SC1091
source "${script_dir}/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root" || exit 1
ssa_native_guard_tools mkdir || exit 1
# shellcheck disable=SC1091
# shellcheck source=debian-paths.sh
source "${script_dir}/debian-paths.sh"

db_path="${SSA_DB_PATH:-${HOME}/.ssaconsultarapida/data/ssas.db}"
ssa_native_guard_path "$db_path" "$repo_root" || exit 1
if [[ ! -f "${db_path}" ]]; then
  mkdir -p "$(dirname "${db_path}")"
  echo "Database file not found at '${db_path}'. The application will open so you can load data and create it." >&2
fi

executable="$(debian_app_executable "${repo_root}" "${preset}")"
if [[ ! -x "${executable}" ]]; then
  echo "Binary not found: ${executable}" >&2
  exit 1
fi
ssa_native_guard_tool "${executable}" || exit 1

"${executable}" --project-root "${repo_root}" --db "${db_path}"
