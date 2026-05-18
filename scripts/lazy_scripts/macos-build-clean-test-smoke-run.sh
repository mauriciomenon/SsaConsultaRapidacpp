#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/lazy_scripts/macos-build-clean-test-smoke-run.sh

Run default clean macOS smoke flow using:
  - data/ssas.db from repository root
  - preset: dev
  - runtime at build/runtime/macos
  - screenshot at build/runtime/macos/main.png

This script is a compatibility wrapper that forwards default clean arguments to
macos-build-test-smoke-run.sh.
Use:
  ./scripts/lazy_scripts/macos-build-test-smoke-run.sh
for full parameterized control.
EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
# shellcheck disable=SC1091
# shellcheck source=../smoke-macos-core.sh
source "${script_dir}/../smoke-macos-core.sh"

if ! db_path="$(resolve_project_default_db_path "${repo_root}")"; then
  print_project_default_db_not_found \
    "${repo_root}" \
    "Or call macos-build-test-smoke-run.sh with an explicit DB path."
  exit 1
fi

"${script_dir}/macos-build-test-smoke-run.sh" \
  "${db_path}" \
  --project-root "${repo_root}" \
  --preset "dev" \
  --config-src "${repo_root}/config/ssa_cpp_preferences.json.example" \
  --clean
