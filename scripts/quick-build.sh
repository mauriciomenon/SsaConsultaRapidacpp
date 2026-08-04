#!/usr/bin/env bash
# Fast incremental compile for quick syntax/code validation.
# No configure, no clean, no tests - just `cmake --build` for the preset.
# Stops on the first compile error (set -e). Use the official build-macos.sh
# (which reconfigures) or the smoke flow (which also tests) for full checks.
set -euo pipefail

preset="${SSA_CPP_PRESET:-dev}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
# shellcheck disable=SC1091
source "${script_dir}/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root" || exit 1
ssa_native_guard_tools cmake || exit 1
# shellcheck disable=SC1091
source "${script_dir}/debian-paths.sh"
cmake --build "$(debian_build_dir "${repo_root}" "${preset}")"
