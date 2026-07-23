#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/build-debian.sh

Build Debian/Ubuntu target using default preset (dev).

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

# Do not inherit Windows temporary directories when invoked from WSL.
export TMPDIR=/tmp
export TMP=/tmp
export TEMP=/tmp

"${repo_root}/tools/configure-dev.sh" "${preset}"
(cd "${repo_root}" && cmake --build --preset "${preset}")
