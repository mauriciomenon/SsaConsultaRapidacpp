#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  ./scripts/package-debian.sh [--help] [options]

Compatibility wrapper for Linux tarball packaging.
This script does not create a .deb package.
The canonical command is:

  ./scripts/package-linux.sh [options]

This file is kept for backwards compatibility with older scripts and docs.
EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${script_dir}/package-linux.sh" "$@"
