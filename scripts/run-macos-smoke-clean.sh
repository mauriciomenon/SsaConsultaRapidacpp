#!/usr/bin/env bash
# Root symlink target: ./run-macos-smoke-clean -> scripts/run-macos-smoke-clean.sh
# Resolves real script dir when invoked via symlink so BASH_SOURCE stays correct.
set -euo pipefail

source="${BASH_SOURCE[0]}"
while [[ -L "${source}" ]]; do
  dir="$(cd "$(dirname "${source}")" && pwd)"
  source="$(readlink "${source}")"
  [[ "${source}" != /* ]] && source="${dir}/${source}"
done
script_dir="$(cd "$(dirname "${source}")" && pwd)"
exec "${script_dir}/smoke-macos.sh" "$@"
