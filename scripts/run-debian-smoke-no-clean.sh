#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/run-debian-smoke-no-clean.sh [--db <path>] [--preset <preset>] [--open]

Run an incremental Debian build, CTest, and offscreen screenshot smoke.
The default database is SSA_DB_PATH or ~/.ssaconsultarapida/data/ssas.db.
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
db_path="${SSA_DB_PATH:-${HOME}/.ssaconsultarapida/data/ssas.db}"
db_path_explicit="false"
preset="dev"
open_requested="false"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --db)
      if [[ -z "${2-}" ]]; then
        echo "--db requires a value." >&2
        exit 1
      fi
      db_path="${2}"
      db_path_explicit="true"
      shift 2
      ;;
    --preset)
      if [[ -z "${2-}" ]]; then
        echo "--preset requires a value." >&2
        exit 1
      fi
      preset="${2}"
      shift 2
      ;;
    --open)
      open_requested="true"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      usage >&2
      exit 1
      ;;
  esac
done

# shellcheck disable=SC1091
source "${script_dir}/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root" || exit 1
ssa_native_guard_tools cmake ctest rm mkdir cp || exit 1
ssa_native_guard_path "$db_path" "$repo_root" || exit 1

# shellcheck disable=SC1091
# shellcheck source=smoke-debian-core.sh
source "${script_dir}/smoke-debian-core.sh"
run_debian_smoke_core \
  "${repo_root}" "${db_path}" "false" "${preset}" "${open_requested}" "${db_path_explicit}"
