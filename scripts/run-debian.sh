#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ssas.db> [--project-root <dir>] [--config-dir <dir>] [--screenshot <file>]" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
db_path="$1"
shift

project_root="${repo_root}"
executable="${repo_root}/build/dev/ssa_consulta_rapida"
if [[ ! -x "${executable}" ]]; then
  echo "Binary not found: ${executable}" >&2
  exit 1
fi

"${executable}" --project-root "${project_root}" --db "${db_path}" "$@"
