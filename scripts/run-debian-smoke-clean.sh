#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/run-debian-smoke-clean.sh [--db <path>] [--open]

Run a clean Debian build, CTest, and offscreen screenshot smoke.
The default database is <repo>/data/ssas.db.
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
db_path="${repo_root}/data/ssas.db"
open_requested="false"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --db)
      if [[ -z "${2-}" ]]; then
        echo "--db requires a value." >&2
        exit 1
      fi
      db_path="${2}"
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
# shellcheck source=smoke-debian-core.sh
source "${script_dir}/smoke-debian-core.sh"
run_debian_smoke_core "${repo_root}" "${db_path}" "true" "dev" "${open_requested}"
