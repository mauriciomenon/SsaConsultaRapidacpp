#!/usr/bin/env bash
# Fast incremental compile for quick syntax/code validation.
# No configure, no clean, no tests - just `cmake --build` for the preset.
# Stops on the first compile error (set -e). Use the official build-macos.sh
# (which reconfigures) or the smoke flow (which also tests) for full checks.
set -euo pipefail

preset="${SSA_CPP_PRESET:-dev}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
(cd "${repo_root}" && cmake --build --preset "${preset}")
