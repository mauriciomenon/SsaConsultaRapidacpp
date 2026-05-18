#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/lazy_scripts/build-macos.sh [--preset <preset>]

Build the macOS target for an explicit preset.

Options:
  --preset <preset>    Optional CMake preset used by configure and build. Default: dev.
  --help               Show this help.
EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
preset="dev"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --preset)
      preset="${2}"
      shift 2
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      show_help >&2
      exit 1
      ;;
  esac
done

if [[ -z "${preset}" ]]; then
  echo "Preset cannot be empty." >&2
  exit 1
fi

"${repo_root}/tools/configure-dev.sh" "${preset}"
cmake --build --preset "${preset}"
