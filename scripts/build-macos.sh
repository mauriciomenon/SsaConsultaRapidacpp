#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/build-macos.sh

Build the macOS target for the selected preset.

Defaults (set by env vars):
  SSA_CPP_PRESET: CMake preset to use (default: dev).
  SSA_CPP_PROJECT_ROOT: Project root (default: parent of script dir).

Examples:
  scripts/build-macos.sh
  SSA_CPP_PRESET=dev ./scripts/build-macos.sh
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

"${repo_root}/tools/configure-dev.sh" "${preset}"
(cd "${repo_root}" && cmake --build --preset "${preset}")
