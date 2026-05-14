#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
preset="${1:-dev}"

"${repo_root}/tools/configure-dev.sh" "${preset}"
cmake --build --preset "${preset}"
